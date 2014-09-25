ECEN602 HW1 Programming Assignment
----------------------------------
Team Number: 30
Member 1 # Raghavan S V (UIN: 722009006)
Member 2 # Hari Kishan Srikonda (UIN: 924002529)
Member 3 # Jiaju Shi (UIN: 823001803)
---------------------------------------
Description/Comments:
--------------------

Main and Bonus 1 implemented

Client:
1. We obtain the username, server IP address and the port number from the user
2. Create the socket and connect to the server.
3. Form the SBCP message along with the attribute using struct. We have two structures: one nested inside the other. The main one is for the SBCP message and the nested one is for the Attribute information
4. Join the chat by sending JOIN and the username to the server.
5. Do the I/O multiplexing inorder to send and receive the message using select() and fd_set. 
6. We send any message using the SEND message type and receive the message from another client which is of FWD message type.
7. We also receive ACK, NAK and OFFLINE/ONLINE message appropriately.

Server:
1. Open the socket and structure the SBCP message to decoding what are received from the client.
2. Bind the socket to an address and port combination that we can ensure all incoming data which is directed towards this port number is received by this application.
3. Listen for incoming connections on the socket by listen().
4. Accept connections from the server JOIN message and the username.
5. The server checks if the request from the client can be allowed i.e. MAX_CLIENTS reached or same usernames used. Send NAK in both cases. 
5. Read the SEND message from a client and FWD messages to others except the sender.
6. The server also sends an ACK message with information about active clients on JOIN.
7. The server can limit the number of active clients and indicate when clients enter and leave the chat session through the ONLINE/OFFLINE messages.
 
Unix command for starting server:
------------------------------------------
./server SERVER_IP SERVER_PORT MAX_CLIENTS
Unix command for starting client:
------------------------------------------
./client USERNAME SERVER_IP SERVER_PORT

