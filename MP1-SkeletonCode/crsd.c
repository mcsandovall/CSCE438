#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"


/*
Instructions for the MP
-----
Listen on a well-known port to accept CREATE, DELETE, or JOIN re-
quests from Chat clients.
-----
For a CREATE request, check whether the given chat room exists al-
ready. If not, create a new master socket (make sure the port num-
ber is not usedr) Create an entry for the new chat room in the local
database, and store the name and the port number of the new chat
room. Inform the client about the result of the command.
-----
For a JOIN request, check whether the chat room exists. If it does,
return the port number of the master socket of that chat room and
the current number members in the chat room. The client will then
connect to the chat room through that port.
-----
For a DELETE request, check whether the given chat room exists. If
it does, send a warning message (e.g., chat room being deleted,
shutting down connection...) to all connected clients before ter-
minating their connections, closing the master socket, and deleting the
entry. Inform the client about the result.
-----
Incoming chat messages are handled on slave sockets that are derived
from the chat-room specific master socket. Whenever a chat message
comes in, forward that message to all clients that are part of the chat
room.
-----
Clients leave the chat room by (unceremoniously) terminating the con-
nection to the server. It is up to the server to handle this and manage
the chat room membership accordingly.
*/

/**
 * ChatRoom structure is design to be used for 
 * easy storage and access information about a chat room
*/

typedef struct ChatRoom {
    int port_number;
    char name[MAX_DATA];
} chat_room;

/**
 * Create a linked list in order create the channels
*/
chat_room head, * tail; // make a dumy head to start of the channels in the linked list

//create the functions for the server to handle 

// Functions need to have threads handling the work 
int process_request(const struct Command command);
int create_chatRoom(const char * chat_name);
int delete_chatRoom(const char * chat_name);
chat_room get_charRoom(const char * chat_name);

int main(int argc, char** argv){
    return 0;
}