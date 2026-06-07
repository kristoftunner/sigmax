# Design ideas
- single thread for each websocket socket - basically / API we have a thread
- pushing data into the MPSC queue
- in the first iteration, venues are only set during construction, no interaction with the user yet


# Error model
- Fatal errors:
  - failing to setup the connection

- Recoverable errors:
  - closing the socket - some APIs after some time tend to close connection -> recovery: reconnect

