all:
	gcc -lzmq -o universal-pubsub universal-pubsub.c
	gcc -lzmq -o universal-sub-pubsub universal-sub-pubsub.c
	gcc -lzmq -o universal-sub-pubsub universal-xpubxsub.c
	gcc -lzmq -o universal-sub-pubsub universal-sub-xpubxsub.c
