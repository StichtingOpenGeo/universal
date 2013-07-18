#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

int main (int argc, char *argv[]) {
	/* Our process ID and Session ID */
	pid_t pid, sid;

	if (argc != 3) {
		printf("%s [pubsub]\n\nEx.\n%s tcp://127.0.0.1:7817 /path/to/storage\n", argv[0], argv[0]);
		exit(-1);
	}

	/* Fork off the parent process */
	pid = fork();
	// pid = 0;

	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* If forking actually didn't work */
	if (pid < 0) {
		/* Change the file mode mask */
		umask(022);

		/* Create a new SID for the child process */
		sid = setsid();

		/* Close out the standard file descriptors */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	/* Security is important, first we move to our target directory */
	chdir(argv[2]);

	/* Setup the connection to the PubSub and subscribe to the envelopes */
	void *context = zmq_init (1);
	void *subscriber = zmq_socket (context, ZMQ_SUB);

	zmq_connect (subscriber, argv[1]);
	zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "/GOVI/KV7calendar", 17);
	zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "/GOVI/KV7planning", 17);

	/* Check if we are root */
	if (getuid() == 0  || geteuid() == 0) {
		struct passwd *pw;
		uid_t puid = 65534; /* just use the traditional value */
		gid_t pgid = 65534;

		if ((pw = getpwnam("nobody"))) {
			puid = pw->pw_uid;
			pgid = pw->pw_gid;
		}

		/* Now we chroot to this directory, preventing any write access outside it */
		chroot(argv[2]);

		/* After we bind to the socket and chrooted, we drop priviledges to nobody */
		setuid(puid);
		setgid(pgid);
	}
	
	/* First make our destination directories */
	mkdir("GOVI/KV7calendar", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir("GOVI/KV7planning", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	while (1) {
#if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
		int more;
#else
		int64_t more;
#endif
		size_t more_size = sizeof(more);
		FILE *fd = NULL;
		do {
			/* Create an empty 0MQ message to hold the message part */
			zmq_msg_t part;
			int rc = zmq_msg_init (&part);
			assert (rc == 0);
			/* Block until a message is available to be received from socket */
#if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
			rc = zmq_msg_recv (&part, subscriber, 0);
#else
			rc = zmq_recv (subscriber, &part, 0);
#endif
			assert (rc != -1);

			if (fd == NULL) {
				char path[222];
				size_t len = zmq_msg_size(&part);
				assert (len < 200);
				memcpy(path, zmq_msg_data(&part), len);
				if (strncmp(path, "/GOVI/KV7calendar", 17) == 0 && len != 17) {
					snprintf(path + len, 4, ".gz");
				} else { 
					struct timeval tv;
					gettimeofday(&tv,NULL);
					snprintf(path + 17, 22, "/%u.%06u.gz", (unsigned int) tv.tv_sec, (unsigned int) tv.tv_usec);
				}
				fd = fopen(path + 1, "w");
				assert (fd != NULL);
			} else {
				fwrite(zmq_msg_data(&part), zmq_msg_size(&part), 1, fd);
			}			
			/* Determine if more message parts are to follow */
			rc = zmq_getsockopt (subscriber, ZMQ_RCVMORE, &more, &more_size);
			assert (rc == 0);
			zmq_msg_close (&part);
		} while (more);

		if (fd) {
			fclose(fd);
		}
	}
}
