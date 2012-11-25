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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <zmq.h>

int main (int argc, char *argv[]) {
    if (argc < 3) {
        printf("%s [subscriber] (filter1 filter2 filterN) [pubsub]\n\nEx.\n" \
                "%s tcp://127.0.0.1:7817 tcp://127.0.0.1:7827\n",
                argv[0], argv[0]);
        exit(-1);
    }

    void *context  = zmq_init (1);
    void *pubsub   = zmq_socket (context, ZMQ_XPUB);
    void *subscriber = zmq_socket (context, ZMQ_SUB);

    unsigned int i;

    /* Apply filters to the PubSub */
    for (i = 2; i < (argc - 1); i++) {
        zmq_setsockopt(pubsub, ZMQ_SUBSCRIBE, argv[i], strlen(argv[i]));
    }

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 255;
    zmq_setsockopt(pubsub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(pubsub, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    zmq_bind (pubsub, argv[argc - 1]);
    zmq_connect (subscriber, argv[1]);

    /* Apply the subscriptions */
    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);

    while (1) {
        int more;
        size_t more_size = sizeof more;
        do {
            /* Create an empty 0MQ message to hold the message part */
            zmq_msg_t part;
            int rc = zmq_msg_init (&part);
            assert (rc == 0);

            /* Block until a message is available to be received from the socket */
            rc = zmq_msg_recv (&part, subscriber, 0);
            assert (rc != -1);

            /* Determine if more message parts are to follow */
            rc = zmq_getsockopt (subscriber, ZMQ_RCVMORE, &more, &more_size);
            assert (rc == 0);

            /* Send the message, when more is set, apply the flag, otherwise don't */
            zmq_msg_send (&part, pubsub, (more ? ZMQ_SNDMORE : 0));

            zmq_msg_close (&part);
        } while (more);
    }
}
