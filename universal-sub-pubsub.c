/* This software is the client component of the ND-OV system.
 * Each multipart message it receives from the ND-OV system
 * consisting of an envelope and its data is rebroadcasted 
 * to all connected clients of the serviceprovider.
 *
 * Requirements: zeromq2 or zeromq3.2
 * gcc -lzmq -o universal-sub-pubsub universal-sub-pubsub.c
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
    void *pubsub   = zmq_socket (context, ZMQ_PUB);

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 255;
    #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
    zmq_setsockopt (pubsub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt (pubsub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    #else
    zmq_setsockopt (pubsub, ZMQ_HWM, &hwm, sizeof(hwm));
    #endif

    zmq_bind (pubsub, argv[argc - 1]);

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

    /* Ensure that every 60s there is data */
    while ((rc = zmq_poll (items, 1, 60000)) >= 0) {
        if (rc > 0) {
            #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
            int more;
            #else
            int64_t more;
            #endif
            size_t more_size = sizeof more;
            do {
                /* Create an empty 0MQ message to hold the message part */
                zmq_msg_t part;
                rc = zmq_msg_init (&part);
                assert (rc == 0);

                /* Block until a message is available to be received from the socket */
                #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
                rc = zmq_msg_recv (&part, items[0].socket, 0);
                #else
                rc = zmq_recv (items[0].socket, &part, 0);
                #endif
                assert (rc != -1);

                /* Determine if more message parts are to follow */
                rc = zmq_getsockopt (items[0].socket, ZMQ_RCVMORE, &more, &more_size);
                assert (rc == 0);

                /* Send the message, when more is set, apply the flag, otherwise don't */
                #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
                zmq_msg_send (&part, pubsub, (more ? ZMQ_SNDMORE : 0));
                #else
                zmq_send (pubsub, &part, (more ? ZMQ_SNDMORE : 0));
                #endif

                zmq_msg_close (&part);
            } while (more);
        } else {
            zmq_close (items[0].socket);
            goto init;
        }
    }

    zmq_close (items[0].socket);

    zmq_close (pubsub);

    #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
    zmq_ctx_destroy (context);
    #else
    zmq_term (context);
    #endif

    return rc;
}
