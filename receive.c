/*
 * packet receive mangager
 * Copyright (C) 2018  Ricardo Biehl Pasquali
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 16/06/2017, 01/12/2017, 29/05/2018
 *
 * See include/linux/socket.h (from in Linux kernel tree)
 *
 * Also, see:
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_socket.h.html
 */

#include <errno.h>      /* errno variable, EAGAIN macro */
#include <string.h>     /* memset() */
#include <stdlib.h>     /* malloc() */
#include <sys/socket.h> /* recvmsg() recvmmsg() */

#include "receive.h"

static int
do_receive_recvmmsg(int fd, struct mmsghdr *msgvec, unsigned int vlen)
{
	return recvmmsg(fd, msgvec, vlen, 0, NULL);
}

static int
do_receive_recvmsg(int fd, struct mmsghdr *msgvec, unsigned int vlen)
{
	int i;

	for (i = 0; i < vlen; i++) {
		msgvec[i].msg_len = recvmsg(fd, &msgvec[i].msg_hdr, 0);
		if (msgvec[i].msg_len == -1) {
			if (errno == EAGAIN)
				break;
			return -1;
		}
	}

	/* i corresponds to the number of messages received */
	return i;
}

/* set custom_* to -1 if you want defaults */
int
receive_prepare(struct receive *r, int custom_addr_len,
                        int custom_control_len)
{
	size_t addr_len = r->addr_len;
	size_t control_len = r->control_len;
	int i;
	struct msghdr *current;

	if (custom_addr_len != -1 && custom_addr_len <= r->addr_len)
		addr_len = custom_addr_len;
	if (custom_control_len != -1 && custom_control_len <= r->control_len)
		control_len = custom_control_len;

	i = r->n_packets;
	while (i--) {
		current = &r->packets[i].msg_hdr;
		current->msg_namelen = addr_len;
		current->msg_controllen = control_len;
		//memset(current->msg_control, 0, control_len);
	}

	return 0;
}

int
receive(int fd, struct receive *r, unsigned int n_packets)
{
	int tmp;

	if (n_packets == 0 || n_packets > r->n_packets)
		n_packets = r->n_packets;

	tmp = r->do_recv(fd, r->packets, n_packets);
	if (tmp == -1)
		return -1;

	return tmp;
}

void
receive_destroy(struct receive *r)
{
	free(r->packets);
}

/*
 * Allocations we do ...
 * - We allocate (n_packets * `struct mmsghdr`) for recvmmsg(). Each
 *   `struct mmsghdr` has one `struct msghdr`. See recvmmsg(2) manual.
 * For each `struct msghdr` we allocate:
 * - A space of size (ctx->addr_len) where network peer address is going to be
 *   returned. It can be omitted by setting ctx->addr_len to zero.
 * - One IO vector structure (msg_iov).
 * - A buffer of size (ctx->buffer_len) where the received data will be placed.
 * - And, optionally, a struct which may have additional data (msg_control).
 *
 * when receiving from network ipv4:
 *   addr_len = sizeof(struct sockaddr_in)
 *   control_len = 0
 * when receiving from another process (Inter Process Communication):
 *   addr_len = sizeof(struct sockaddr_un)
 *   control_len = TODO
 */
int
receive_init(struct receive *r, unsigned int flags)
{
	size_t size_to_allocate;
	void *tmp;
	int i;
	struct msghdr *current;

	/* mmsghdr structures + per packet data used in msghdr */
	size_to_allocate =
	  r->n_packets *
	  (
	    sizeof(*r->packets) + r->addr_len + sizeof(struct iovec) +
	    r->buffer_len + r->control_len
	  );

	/* allocate all memory in just one malloc() */
	tmp = malloc(size_to_allocate);

	/* TODO: do memory locking */

	/* assign an address to the array of `struct mmsghdr` */
	r->packets = tmp;
	tmp += sizeof(*r->packets) * r->n_packets;

	/* prepare individual packets */
	i = r->n_packets;
	while (i--) {
		/*
		 * r->packets[i].msg_len  contains the number of read bytes
		 * after recvmmsg() call
		 */
		current = &r->packets[i].msg_hdr;

		/* it's the "address return space" (e.g. struct sockaddr_in) */
		current->msg_namelen = r->addr_len;
		current->msg_name = r->addr_len ? tmp : NULL;
		tmp += r->addr_len;

		/* only one IO vector is used */
		current->msg_iovlen = 1; /* # iovec structs in msg_iov */
		current->msg_iov = tmp;
		tmp += sizeof(struct iovec);

		/* set up the packet's buffer */
		current->msg_iov[0].iov_len = r->buffer_len;
		current->msg_iov[0].iov_base = tmp;
		tmp += r->buffer_len;

		/* used to receive control messages */
		current->msg_controllen = r->control_len;
		current->msg_control = r->control_len ? tmp : NULL;
		tmp += r->control_len;

		/* flags for this packet */
		current->msg_flags = 0;
	}

	if (flags & RECVCTX_USE_RECVMSG)
		r->do_recv = do_receive_recvmsg;
	else
		r->do_recv = do_receive_recvmmsg;

	return 0;
}