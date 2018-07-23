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
	int tmp;

	for (i = 0; i < vlen; i++) {
		tmp = recvmsg(fd, &msgvec[i].msg_hdr, 0);
		if (tmp == -1) {
			if (i)
				break;
			return -1;
		}

		msgvec[i].msg_len = tmp;
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
receive_free_buffer(struct receive *r)
{
	free(r->packets);
}

/*
 * Allocations done:
 *
 * - We allocate (n_packets * `struct mmsghdr`). Each
 *   `struct mmsghdr` has one `struct msghdr`. See
 *   recvmmsg.2 manual.
 *
 * For each `struct msghdr` it's allocated:
 *
 * - A space (msg_name) to return network peer address
 *   (e.g. `struct sockaddr_in`). The `addr_len` variable
 *   defines the size of this space. Setting to zero makes
 *   no allocation.
 *
 * - One IO vector structure (msg_iov). See readv.2 manual.
 *
 * - A buffer of size `buffer_len` where the received data
 *   will be placed.
 *
 * - A space (msg_control) which may carry additional data.
 *   It can be omitted by setting `control_len` to zero.
 *
 * When receiving from ipv4 socket, addr_len must be set
 * to sizeof(struct sockaddr_in). For unix socket,
 * sizeof(struct sockaddr_un).
 */
int
receive_allocate_buffer(struct receive *r, unsigned int flags)
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

	if (flags & RECEIVE_USE_RECVMSG)
		r->do_recv = do_receive_recvmsg;
	else
		r->do_recv = do_receive_recvmmsg;

	return 0;
}
