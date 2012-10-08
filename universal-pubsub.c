/* This software is the core component of the ND-OV system
 * each multipart message it receives from a transport operator
 * consisting of an envelope and its data, is broadcasted to
 * all connected serviceproviders.
 *
 * Requirements: zeromq2 or zeromq3.2
 * gcc -lzmq -o universal-pubsub universal-pubsub.c
 *
 * Changes:
 *  - Initial version <stefan@opengeo.nl>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <zmq.h>

int main (int argc, char *argv[]) {
    if (argc != 3) {
        printf("%s [receiver] [pubsub]\n\nEx.\n" \
                "%s tcp://127.0.0.1:7807 tcp://127.0.0.1:7817\n",
                argv[0], argv[0]);
        exit(-1);
    }

    void *context  = zmq_init (1);
    void *pubsub   = zmq_socket (context, ZMQ_PUB);
    void *receiver = zmq_socket (context, ZMQ_PULL);

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 255;
    #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
    zmq_setsockopt(pubsub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(pubsub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    #else
    zmq_setsockopt(pubsub, ZMQ_HWM, &hwm, sizeof(hwm));
    #endif

    zmq_bind (pubsub,   argv[2]);
    zmq_bind (receiver, argv[1]);

    while (1) {
        int64_t more;
        size_t more_size = sizeof more;
        do {
            /* Create an empty 0MQ message to hold the message part */
            zmq_msg_t part;
            int rc = zmq_msg_init (&part);
            assert (rc == 0);

            /* Block until a message is available to be received from the socket */
	    #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
	    rc = zmq_recvmsg (subscriber, &part, 0);
	    #else
            rc = zmq_recv (receiver, &part, 0);
	    #endif
            assert (rc == 0);

            /* Determine if more message parts are to follow */
            rc = zmq_getsockopt (receiver, ZMQ_RCVMORE, &more, &more_size);
            assert (rc == 0);

            /* Send the message, when more is set, apply the flag, otherwise don't */
	    #if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
	    zmq_sendmsg (pubsub, &part, (more ? ZMQ_SNDMORE : 0));
	    #else
            zmq_send (pubsub, &part, (more ? ZMQ_SNDMORE : 0));
	    #endif

            zmq_msg_close (&part);
        } while (more);
    }
}
