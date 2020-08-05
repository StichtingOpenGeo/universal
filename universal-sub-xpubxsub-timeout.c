/* This software is the client component of the ND-OV system.
 * Each multipart message it receives from the ND-OV system
 * consisting of an envelope and its data is rebroadcasted
 * to all connected clients of the serviceprovider.
 *
 * Requirements: zeromq3.2
 * gcc -lzmq -o universal-sub-xpubxsub universal-sub-xpubxsub.c
 *
 * Changes:
 *  - Initial version <stefan@opengeo.nl>
 *  - zeromq 3.2 compatibility added,
 *    pubsub binding bugfix  <p.r.fokkinga@rug.nl>
 */

#include <pwd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <zmq.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>


int main (int argc, char *argv[]) {
    if (argc < 3) {
        printf("%s [subscriber] (filter1 filter2 filterN) [pubsub]\n\nEx.\n" \
                "%s tcp://127.0.0.1:7817 tcp://127.0.0.1:7827\n",
                argv[0], argv[0]);
        exit(-1);
    }

    chdir("/var/empty");

    void *context  = zmq_init (1);
    void *pubsub   = zmq_socket (context, ZMQ_XPUB);

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 8192;
    zmq_setsockopt (pubsub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt (pubsub, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    zmq_bind (pubsub, argv[argc - 1]);

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

    zmq_pollitem_t items[1];

    int timeout = 60;
    timeout = strtol(argv[1], NULL, 10);
    if (timeout > 3600 || timeout <= 0) timeout = 60;

init:
    items[0].socket = zmq_socket (context, ZMQ_SUB);
    items[0].events = ZMQ_POLLIN;

    /* Apply filters to the subscription from the remote source */
    if (argc > 4) {
        unsigned int i;

        for (i = 3; i < (argc - 1); i++) {
            zmq_setsockopt(items[0].socket, ZMQ_SUBSCRIBE, argv[i], strlen(argv[i]));
        }
    } else {
        zmq_setsockopt(items[0].socket, ZMQ_SUBSCRIBE, "", 0);
    }

    fprintf(stderr, "Connecting %s\n", argv[2]);
    zmq_connect (items[0].socket, argv[2]);

    int rc;
    size_t more_size = sizeof(int);

    /* Ensure that every 60s there is data */
    while ((rc = zmq_poll (items, 1, timeout * 1000L)) >= 0) {
        if (rc > 0) {
            int more;
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
                zmq_msg_send (&part, pubsub, (more ? ZMQ_SNDMORE : 0));

                zmq_msg_close (&part);
            } while (more);
        } else {
	    fprintf(stderr, "Timeout detected at %s\n", argv[2]);
            zmq_close (items[0].socket);
            sleep (1);
            goto init;
        }
    }

    zmq_close (items[0].socket);

    zmq_close (pubsub);

    zmq_ctx_destroy (context);

    return 0;
}
