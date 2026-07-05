# Design ideas
- single thread for each websocket socket - basically / API we have a thread
- pushing data into the MPSC queue
- in the first iteration, venues are only set during construction, no interaction with the user yet


# Error model
- Fatal errors:
  - failing to setup the connection

- Recoverable errors:
  - closing the socket - some APIs after some time tend to close connection -> recovery: reconnect

# Networking - websocket

`net::`
`net::ip::tcp::socket` - needed for tcp
context - `net::ssl::context` - needed for ???

## Multi-threading
Use `net::strand`

## How websocket works
HTTPS 1.1 Upgrade request -> handshake
