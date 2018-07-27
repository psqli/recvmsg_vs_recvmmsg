===================
recvmsg vs recvmmsg
===================

:Date: 2018-05-31

Both system calls are used to receive data from a socket.
``recvmsg()`` receives just one packet, ``recvmmsg()``
can receive multiple packets (as many as the user
requests).

This program aims to test the performance of both syscalls.


How to use
==========

``usage: cmd [OPTIONS] <port>``::

	-h Help.
	-b <buffer_size> Number of packets allocated to
	   send per round.
	-c <round_count> Number of rounds (send + receive)
	   to run. Every round sends and reads buffer_size
	   entries.
	-m Use recvmsg(). If not specified, use recvmmsg().


How it works?
=============

The program sends *buffer_size* packets to the loopback
and receives these packets using *buffer_size* calls to
``recvmsg()`` or a single call to ``recvmmsg()``. This
is repeated *round_count* times.


Test results
============

Not available yet.

It's necessary to run the test in both PTI (Page Table
Isolation) patched and not Linux kernels.
