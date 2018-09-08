# SimpleFileTransfer
# Completed simple file transfer project

# This program sends data via a bytestream from a client to a server.

# Its a simplified file transfer protocol. 

# Handshakeless, similar to UDP.
# Both the client and the server have two sockets, one for sending and one for recieving packets.
# To handle two sockets both the client and the server are multithreaded.
# Both sides have a timer running to check for packet loss and resend packets upon loss.
# When a timer is expired a signal is used to notify a resend.
# Locks are used as this program is multithreaded and we dont want any race conditions

# The file to send, ip address and socket to connect to are entered by the user as command line arguments.
# The program parses those command line arguments.

# Packets are serialized and have a window in which they can be accepted in
# The window for acceptable packets is dynamic and uses AIMD
# Uses acks but no nacks
