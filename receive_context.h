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

#ifndef RECVCTX_H
#define RECVCTX_H

#include <sys/socket.h> /* `struct mmsghdr` */

struct receive_ctx {
	unsigned int n_packets;
	/* inside each packet context: */
	/* the address won't be returned if user set addr_len to 0 */
	size_t addr_len;
	size_t buffer_len;
	size_t control_len;

	/* receive function */
	int (*do_recv)(int fd, struct mmsghdr *msgvec, unsigned int vlen);

	/* array of size `n_packets` */
	struct mmsghdr *packets;
};

/* flags */
#define RECVCTX_USE_RECVMMSG (1 << 0)

#define receive_context_for_each(ctx, packet) \
	for ((ctx)->current_packet = 0, \
	       packet = &(ctx)->packets[(ctx)->current_packet]; \
	     (ctx)->current_packet != (ctx)->n_packets; \
	     (ctx)->current_packet++, \
	       packet = &(ctx)->packets[(ctx)->current_packet])

int
receive_context_prepare(struct receive_ctx *ctx, int custom_addr_len,
                        int custom_control_len);

int
receive_context_do_receive(int fd, struct receive_ctx *ctx,
                           unsigned int n_packets);

void
receive_context_destroy(struct receive_ctx *ctx);

int
receive_context_init(struct receive_ctx *ctx, unsigned int flags);

#endif /* RECVCTX_H */
