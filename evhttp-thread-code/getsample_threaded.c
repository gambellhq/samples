/**
 * Multithreaded, evhttp-based http get server.
 * Copyright (c) 2012 Qi Huang
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <signal.h>

#include "workqueue.h"

/* Port to listen on. */
#define SERVER_PORT 5555
/* Connection backlog (# of backlogged connections to accept). */
#define CONNECTION_BACKLOG 8
/* Number of worker threads.  Should match number of CPU cores reported in 
 * /proc/cpuinfo. */
#define NUM_THREADS 8

/* Behaves similarly to fprintf(stderr, ...), but adds file, line, and function
 information. */
#define errorOut(...) {\
    fprintf(stderr, "%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
    fprintf(stderr, __VA_ARGS__);\
}

/**
 * Struct to carry around connection (client)-specific data.
 */
typedef struct client {
    /* The event_base for this client. */
    struct event_base *evbase;

    /* The evhttp for this client. */
    struct evhttp *evhttp;

    /* The evhttp_bound_socket for this client. */
    struct evhttp_bound_socket *handle;
} client_t;

static struct event_base *evbase_accept;
static workqueue_t workqueue;

/* Signal handler function (defined below). */
static void sighandler(int signal);

static void closeClient(client_t *client) {
    if (client != NULL) {
        if (client->handle != NULL) {
            evutil_socket_t client_fd
                = evhttp_bound_socket_get_fd(client->handle);
            if (client_fd > 0) {
                evutil_closesocket(client_fd);
            }
            evhttp_del_accept_socket(client->evhttp, client->handle);
            free(client->handle);
            client->handle = NULL;
        }
    }
}

static void closeAndFreeClient(client_t *client) {
    if (client != NULL) {
        closeClient(client);
        if (client->evhttp != NULL) {
            evhttp_free(client->evhttp);
            client->evhttp = NULL;
        }
        if (client->evbase != NULL) {
            event_base_free(client->evbase);
            client->evbase = NULL;
        }
        free(client);
    }
}









/**
 * Called by libevent when there is request coming.
 */
void request_on_receivd(struct evhttp_request *req, void *arg) {
    client_t *client = (client_t *)arg;
    const char *cmdtype;

    switch(evhttp_request_get_command(req)) {
        case EVHTTP_REQ_GET: cmdtype = "GET"; break;
        case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
        default: cmdtype = "Unknown"; break;
    }

    printf("received a %s request for %s\n",
            cmdtype, evhttp_request_get_uri(req));

    /* Send a 200 OK reply to client */
    evhttp_send_reply(req, 200, "OK", NULL);
}



static void server_job_function(struct job *job) {
    client_t *client = (client_t *)job->user_data;

    event_base_dispatch(client->evbase);
    closeAndFreeClient(client);
    free(job);
}

/**
 * This function will be called by libevent when there is a connection
 * ready to be accepted.
 */
void on_accept(evutil_socket_t fd, short ev, void *arg) {
    workqueue_t *workqueue = (workqueue_t *)arg;
    client_t *client;
    job_t *job;

    /* Create a client object. */
    if ((client = malloc(sizeof(*client))) == NULL) {
        warn("failed to allocate memory for client state");
        return;
    }
    memset(client, 0, sizeof(*client));

    /* Create new event_base, evhttp and accept a new connection for client. */
    if ((client->evbase = event_base_new()) == NULL) {
        warn("client event_base creation failed");
        closeAndFreeClient(client);
        return;
    }
    if ((client->evhttp = evhttp_new(client->evbase)) == NULL) {
        warn("client evhttp creation failed");
        closeAndFreeClient(client);
        return;
    }
    client->handle = evhttp_accept_socket_with_handle(client->evhttp, fd);
    if (client->handle == NULL) {
        warn("accept failed");
        closeAndFreeClient(client);
        return;
    }

    /* Set the client socket to non-blocking mode. */
    evutil_socket_t client_fd = evhttp_bound_socket_get_fd(client->handle);
    if (evutil_make_socket_nonblocking(client_fd) < 0) {
        warn("failed to set client socket to non-blocking");
        closeAndFreeClient(client);
        return;
    }

    /* Add any custom code anywhere from here to the end of this function
     * to initialize your application-specific attributes in the client struct.
     */

    /* Set the default callback for evhttp */
    evhttp_set_gencb(client->evhttp, request_on_receivd, client);

    /* Create a job object and add it to the work queue. */
    if ((job = malloc(sizeof(*job))) == NULL) {
        warn("failed to allocate memory for job state");
        closeAndFreeClient(client);
        return;
    }
    job->job_function = server_job_function;
    job->user_data = client;

    workqueue_add_job(workqueue, job);
}

/**
 * Run the server.  This function blocks, only returning when the server has 
 * terminated.
 */
int runServer(void) {
    evutil_socket_t listenfd;
    struct sockaddr_in listen_addr;
    struct event *ev_accept;
    int reuseaddr_on;

    /* Set signal handlers */
    sigset_t sigset;
    sigemptyset(&sigset);
    struct sigaction siginfo = {
        .sa_handler = sighandler,
        .sa_mask = sigset,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGINT, &siginfo, NULL);
    sigaction(SIGTERM, &siginfo, NULL);

    /* Create our listening socket. */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(1, "listen failed");
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(SERVER_PORT);
    if (bind(listenfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) 
        < 0) {
        err(1, "bind failed");
    }
    if (listen(listenfd, CONNECTION_BACKLOG) < 0) {
        err(1, "listen failed");
    }
    reuseaddr_on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on,
               sizeof(reuseaddr_on));

    /* Set the socket to non-blocking, this is essential in event
     * based programming with libevent. */
    if (evutil_make_socket_nonblocking(listenfd) < 0) {
        err(1, "failed to set server socket to non-blocking");
    }

    if ((evbase_accept = event_base_new()) == NULL) {
        perror("Unable to create socket accept event base");
        close(listenfd);
        return 1;
    }

    /* Initialize work queue. */
    if (workqueue_init(&workqueue, NUM_THREADS)) {
        perror("Failed to create work queue");
        close(listenfd);
        workqueue_shutdown(&workqueue);
        return 1;
    }

    /* We now have a listening socket, we create a read event to
     * be notified when a client connects. */
    ev_accept = event_new(evbase_accept, listenfd, EV_READ|EV_PERSIST,
                          on_accept, (void *)&workqueue);
    event_add(ev_accept, NULL);

    printf("Server running.\n");

    /* Start the event loop. */
    event_base_dispatch(evbase_accept);

    event_base_free(evbase_accept);
    evbase_accept = NULL;

    close(listenfd);

    printf("Server shutdown.\n");

    return 0;
}

/**
 * Kill the server.  This function can be called from another thread to kill
 * the server, causing runServer() to return.
 */
void killServer(void) {
    fprintf(stdout, "Stopping socket listener event loop.\n");
    if (event_base_loopexit(evbase_accept, NULL)) {
        perror("Error shutting down server");
    }
    fprintf(stdout, "Stopping workers.\n");
    workqueue_shutdown(&workqueue);
}

static void sighandler(int signal) {
    fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal,
            strsignal(signal));
    killServer();
}

/* Main function for demonstrating the echo server.
 * You can remove this and simply call runServer() from your application. */
int main(int argc, char *argv[]) {
    return runServer();
}
