/* This software is the client component of the ND-OV system.
 * Each multipart message it receives from the ND-OV system
 * consisting of an envelope and its data is rebroadcasted
 * to all connected clients of the serviceprovider.
 *
 * Requirements: zeromq2 or zeromq3.2
 * gcc -lzmq -o universal-sub-push universal-sub-push.c
 *
 * Changes:
 *  - Initial version <stefan@opengeo.nl>
 *  - zeromq 3.2 compatibility added,
 *    pubsub binding bugfix  <p.r.fokkinga@rug.nl>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "zmq.h"

int main (int argc, char *argv[]) {
    /* Our process ID and Session ID */
    pid_t pid;

    if (argc < 3) {
        printf("%s [subscriber] (filter1 filter2 filterN) [pull]\n\nEx.\n" \
                "%s tcp://127.0.0.1:7827 tcp://127.0.0.1:7817\n",
                argv[0], argv[0]);
        exit(-1);
    }

    /* Fork off the parent process */
    pid = fork();

    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* If forking actually didn't work */
    if (pid < 0) {
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    chdir("/var/empty");

    void *context  = zmq_init (1);
    void *push     = zmq_socket (context, ZMQ_PUSH);

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 8192;
    zmq_setsockopt (push, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt (push, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    /* Check if we are root */
    if (getuid() == 0  || geteuid() == 0) {
        uid_t puid = 65534; /* just use the traditional value */
        gid_t pgid = 65534;

        /* Now we chroot to this directory, preventing any write access outside it */
        chroot("/var/empty");

        /* After we bind to the socket and chrooted, we drop priviledges to nobody */
        setuid(puid);
        setgid(pgid);
    }

    zmq_connect (push, argv[argc - 1]);

    zmq_pollitem_t items[1];

init:
    items[0].socket = zmq_socket (context, ZMQ_SUB);
    items[0].events = ZMQ_POLLIN;

    /* Apply filters to the subscription from the remote source */
    if (argc > 3) {
        unsigned int i;

        for (i = 2; i < (argc - 1); i++) {
            zmq_setsockopt (items[0].socket, ZMQ_SUBSCRIBE, argv[i], strlen(argv[i]));
        }
    } else {
        zmq_setsockopt (items[0].socket, ZMQ_SUBSCRIBE, "", 0);
    }

    zmq_connect (items[0].socket, argv[1]);

    int rc;
    size_t more_size = sizeof(more_t);

    /* Ensure that every 60s there is data */
    while ((rc = zmq_poll (items, 1, 60 * 1000 * ZMQ_POLL_MSEC)) >= 0) {
        if (rc > 0) {
            more_t more;
            do {
                /* Create an empty 0MQ message to hold the message part */
                zmq_msg_t part;
                rc = zmq_msg_init (&part);
                assert (rc == 0);

                /* Block until a message is available to be received from the socket */
                rc = zmq_msg_recv (&part, items[0].socket, 0);
                assert (rc != -1);

                /* Determine if more message parts are to follow */
                rc = zmq_getsockopt (items[0].socket, ZMQ_RCVMORE, &more, &more_size);
                assert (rc == 0);

                /* Send the message, when more is set, apply the flag, otherwise don't */
                zmq_msg_send (&part, push, (more ? ZMQ_SNDMORE : 0));

                zmq_msg_close (&part);
            } while (more);
        } else {
            zmq_close (items[0].socket);
            sleep (1);
            goto init;
        }
    }

    zmq_close (items[0].socket);

    zmq_close (push);

    zmq_ctx_destroy (context);

    return rc;
}
