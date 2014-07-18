/* This software is the core component of the ND-OV system
 * each multipart message it receives from a transport operator
 * consisting of an envelope and its data, is broadcasted to
 * all connected serviceproviders.
 *
 * Requirements: zeromq3.2
 * gcc -lzmq -o universal-xpubxsub universal-xpubxsub.c
 *
 * Changes:
 *  - Initial version <stefan@opengeo.nl>
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
    /* Our process ID and Session ID */
    pid_t pid;

    if (argc != 3) {
        printf("%s [receiver] [pubsub]\n\nEx.\n" \
                "%s tcp://127.0.0.1:7807 tcp://127.0.0.1:7817\n",
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
    void *pubsub   = zmq_socket (context, ZMQ_XPUB);
    void *receiver = zmq_socket (context, ZMQ_PULL);

    /* Apply a high water mark at the PubSub */
    uint64_t hwm   = 8192;
    zmq_setsockopt (pubsub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt (pubsub, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    zmq_bind (pubsub, argv[argc - 1]);
    zmq_bind (receiver, argv[1]);

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
    
    size_t more_size = sizeof(int);

    while (1) {
        int more;
        do {
            /* Create an empty 0MQ message to hold the message part */
            zmq_msg_t part;
            int rc = zmq_msg_init (&part);
            assert (rc == 0);

            /* Block until a message is available to be received from the socket */
            rc = zmq_msg_recv (&part, receiver, 0);
            assert (rc != -1);

            /* Determine if more message parts are to follow */
            rc = zmq_getsockopt (receiver, ZMQ_RCVMORE, &more, &more_size);
            assert (rc == 0);

            /* Send the message, when more is set, apply the flag, otherwise don't */
            zmq_msg_send (&part, pubsub, (more ? ZMQ_SNDMORE : 0));

            zmq_msg_close (&part);
        } while (more);
    }

    zmq_close (receiver);

    zmq_close (pubsub);

    zmq_ctx_destroy (context);
}
