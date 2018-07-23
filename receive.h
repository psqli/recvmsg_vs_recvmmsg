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

#ifndef RECEIVE_H
#define RECEIVE_H

#include <sys/socket.h> /* `struct mmsghdr` */

struct receive {
	unsigned int n_packets;
	/* inside each packet context: */
	/* the address won't be returned if user set addr_len to 0 */
	size_t addr_len;
	size_t buffer_len;
	size_t control_len;

	/* receive function */
	int (*do_recv)(int fd, struct mmsghdr *msgvec, unsigned int vlen,
	               unsigned int flags);

	/* array of size `n_packets` */
	struct mmsghdr *packets;
};

/* flags */
#define RECEIVE_USE_RECVMSG (1 << 0)

int
receive_prepare(struct receive *r, int custom_addr_len,
                int custom_control_len);

int
receive(int fd, struct receive *r, unsigned int n_packets, unsigned int flags);

void
receive_free_buffer(struct receive *r);

int
receive_allocate_buffer(struct receive *r, unsigned int flags);

#endif /* RECEIVE_H */
