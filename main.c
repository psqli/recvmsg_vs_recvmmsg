/*
 * receive performance test
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
 * 30/05/2018
 */

#include <arpa/inet.h>  /* hton*() */
#include <netinet/in.h> /* struct sockaddr_in */
#include <stdio.h>      /* printf() */
#include <stdlib.h>     /* atoi() */
#include <sys/mman.h>   /* mlockall() */
#include <sys/socket.h> /* socket() connect() send() */
#include <time.h>       /* clock_gettime() */
#include <unistd.h>     /* close() getopt() */

#include "receive.h"

static void
usage(void)
{
	printf("usage: cmd [OPTIONS] <port>\n");
}

static void
help(void)
{
	usage();
	printf(
"-h This help.\n"
"-b <buffer_size> Number of packets allocated to send per round.\n"
"-c <round_count> Number of rounds (send + receive) to run.\n"
"   Every round sends and reads buffer_size entries.\n"
"-m Use recvmsg(). If not specified, use recvmmsg().\n");
}

static int
do_round(struct receive *r, int fd, struct sockaddr_in *saddr)
{
	int i;
	char str[] = "Hello world!";

	for (i = 0; i < r->n_packets; i++)
		sendto(fd, str, sizeof(str), 0,
		       (struct sockaddr*) saddr, sizeof(*saddr));

	receive_prepare(r, -1, -1);

	if (receive(fd, r, r->n_packets) == -1) {
		perror("receive_context");
		return -1;
	}

#if 0
	struct mmsghdr *current;
	receive_context_for_each (rctx, current) {
		char *tmp_str;

		if (!current->msg_len)
			continue;

		tmp_str = current->msg_hdr.msg_iov->iov_base;
		tmp_str[current->msg_len - 1] = '\0';
		printf("msg %d: %s\n", rctx->current_packet, tmp_str);
	}
#endif

	return 0;
}

static void
time_diff(struct timespec *diff,
          struct timespec *stop,
          struct timespec *start)
{
	if (stop->tv_nsec < start->tv_nsec) {
		/* here we assume (stop->tv_sec - start->tv_sec) is not zero */
		diff->tv_sec = stop->tv_sec - start->tv_sec - 1;
		diff->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		diff->tv_sec = stop->tv_sec - start->tv_sec;
		diff->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

int
main(int argc, char **argv)
{
	/* option parsing */
	int opt;
	int buffer_size;
	int n_to_send;
	int flags;
	int port;
	/* network stuff */
	int fd;
	struct sockaddr_in saddr;
	/* buffer stuff */
	struct receive receive;

	struct timespec start;
	struct timespec stop;
	struct timespec diff;

	//mlockall(MCL_CURRENT | MCL_FUTURE);

	/*
	 * argument parsing
	 * ================
	 */

	if (argc < 2) {
		usage();
		return 1;
	}

	/* defaults */
	buffer_size = 1;
	n_to_send = 1;
	flags = 0;

	while ((opt = getopt(argc, argv, "+b:c:mh")) != -1) {
		switch (opt) {
		case 'b':
			buffer_size = atoi(optarg);
			if (buffer_size <= 0) {
				usage();
				return 1;
			}
			break;
		case 'c':
			n_to_send = atoi(optarg);
			if (n_to_send <= 0) {
				usage();
				return 1;
			}
			break;
		case 'm':
			flags = RECVCTX_USE_RECVMSG;
			break;
		case 'h':
			help();
			return 0;
		}
	}

	if (optind >= argc) {
		usage();
		return 1;
	}

	port = atoi(argv[optind]);

	if (flags & RECVCTX_USE_RECVMSG)
		printf("Using recvmsg()\n");
	else
		printf("Using recvmmsg()\n");

	/*
	 * initialize socket
	 * =================
	 */

	fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (fd == -1)
		return 1;

	saddr.sin_family =      AF_INET;
	saddr.sin_port =        htons(port);
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(fd, (struct sockaddr*) &saddr, sizeof(saddr)) == -1)
		goto _go_close_socket; /* NOTE: yet returning zero */

	/*
	 * initialize recvctx
	 * ==================
	 */

	receive.n_packets = buffer_size;
	receive.addr_len = sizeof(struct sockaddr_in);
	receive.buffer_len = 512;
	receive.control_len = 0;
	receive_init(&receive, flags);

	/*
	 * run test
	 * ========
	 */

	clock_gettime(CLOCK_MONOTONIC, &start);
	while (n_to_send--) {
		/* TODO: error handling */
		do_round(&receive, fd, &saddr);
	}
	clock_gettime(CLOCK_MONOTONIC, &stop);

	time_diff(&diff, &stop, &start);

	printf("Elapsed time: %ld.%06ld ms\n",
	       diff.tv_sec * 1000 + diff.tv_nsec / 1000000,
	       diff.tv_nsec % 1000000);

//_go_destroy_buffers:
	receive_destroy(&receive);
_go_close_socket:
	close(fd);

	return 0;
}
