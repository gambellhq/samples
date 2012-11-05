Multithreaded, libevent-based socket server.

Copyright (c) 2012 Ronald Bennett Cemer
This software is licensed under the BSD license.
See the accompanying LICENSE.txt for details.

To compile: gcc -o echoserver_threaded echoserver_threaded.c workqueue.c -levent -lpthread

To run: ./echoserver_threaded


Libevent is a nice library for handling and dispatching events, as well as doing nonblocking I/O.  This is fine, except that it is basically single-threaded -- which means that if you have multiple CPUs or a CPU with hyperthreading, you're really under-utilizing the CPU resources available to your server application because your event pump is running in a single thread and therefore can only use one CPU core at a time.

The solution is to create one libevent event queues (AKA event_base) per active connection, each with its own event pump thread.  This project does exactly that, giving you everything you need to write high-performance, multi-threaded, libevent-based socket servers.

There are mentionings of running libevent in a multithreaded implementation, however it is very difficult (if not impossible) to find working implementations.  This project is a working implementation of a multi-threaded, libevent-based socket server.

The server itself simply echoes whatever you send to it.  Start it up, then telnet to it:
    telnet localhost 5555
Everything you type should be echoed back to you.

The implementation is fairly standard.  The main thread listens on a socket and accepts new connections, then farms the actual handling of those connections out to a pool of worker threads.  Each connection has its own isolated event queue.

In theory, for maximum performance, the number of worker threads should be set to the number of CPU cores available.  Feel free to experiment with this.

Also note that the server includes a multithreaded work queue implementation, which can be re-used for other purposes.

Since the code is BSD licensed, you are free to use the source code however you wish, either in whole or in part.



Some inspiration and coding ideas came from echoserver and cliserver, both of which are single-threaded, libevent-based servers.

Echoserver is located here: http://ishbits.googlecode.com/svn/trunk/libevent-examples/echo-server/libevent_echosrv1.c
Cliserver is located here: http://nitrogen.posterous.com/cliserver-an-example-libevent-based-socket-se
