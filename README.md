universal
=========

A set of tools supporting automatic ZeroMQ distribution.


Receive and Broadcast
-----------------------

It is useful in many computer programs to publish content to a number of subscibers.
The same computer program is a publisher and orgin of these messages the Publish-Subscribe pattern can be used.
Having a single program as publisher and PubSub introduces a single point of failure, secondary scaling either of them is difficult.
This is the reason why we have build universal.

Universal receives message envelopes and content in a Push-Pull pattern, then transparantly publishes the message.
In this way the PubSub is decoupled from the content generation, and allows to scale up to multiple generators.
The universal program can be used in any project, and is content independent.


Connect, Filter and Rebroadcast
-------------------------------

Once universal is used as an aggregation point, it becomes clear that a demand exist for filtering data.
While interactive filtering of subscriptions is implemented as XPUB/XSUB in ZeroMQ3, a subscription in ZeromMQ2 will still receive all data.
Filtering data also allows special arrangements for clients, in a business sence.
When a filter is in place the program rebroadcasts the incoming data to all attached subscribers.

Initially we built sub-pubsub just to receive one remote network stream, and redistribute it on the local network, an optimisation similar to multicast.
Its use is versatile, and allows to scale up your content delivery.