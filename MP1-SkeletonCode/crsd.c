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

int port_number = 3005;

/**
 * Create a linked list in order create the channels
*/
chat_room *head = NULL, * tail = NULL;

//create the functions for the server to handle 

// Functions need to have threads handling the work 
int connect_to(const int port);
Command process_request(const char * command);
int create_chatRoom(const char * chat_name);
int delete_chatRoom(const char * chat_name);
void get_roomList(struct Reply * reply);
chat_room * get_chatRoom(const char * chat_name);

int main(int argc, char** argv){
    
    
    while(1){

    }
    return 0;
}

/**
 * 
 * Establishing communication for a port number
 * 
 * @param           port number to be binded
 * @return          -1 if there is a problem with the code, 0 if it worked
*/

int connect_to(const int port){

    int sockfd;
    struct sockaddr_in servaddr;

    //socket create
    if((sockfd = socket(AF_INET,SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        return -1;
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    

    return 0;
}

/**
 * 
 * Process the request from a command and utilize the function based on the request
 * 
 * @parameter command       string of a command to be parsed into the datastructure
 * @return int              success or error code
*/
Command process_request(const char * command){
    
    
}

/**
 * 
 * handles the create a room frature for the client and sends back a sucessful Reply
 * @parameter               name of the chat room to be created
 * @return                  -1 if the room already exist
*/
int create_chatRoom(const char * chat_name){
    
    // first check and see if the chat room doesnt already exist
    if(get_chatRoom(chat_name) != NULL){return -1;}
    
    chat_room * room = (chat_room *) malloc(sizeof(chat_room));
    strcpy(room->name,chat_name);

    //create a new master socket 

    // new entry in the database
    if(head == NULL){ // make the first node
        room->port_number = port_number;
        room->next = NULL;
        head = room;
        tail = room;
        return 0;
    }
    
    room->port_number = ++port_number;
    tail->next = room;
    tail = room;
    return 0;
}

/**
 * 
 * Delete the chat room of such name
 * 
 * @parameter               name of the chat room to be deleted
 * @return                  -1 if the room doesnt exist
*/
int delete_chatRoom(const char * chat_name){

    if(head == NULL){return -1;}

    //send a warning message to all the clients connected to the chat

    // close the master socket for that room
    
    // delete that room 
    chat_room * current = head;
    
    while(current->next != NULL){
        if(strncmp(current->next->name, chat_name, strlen(chat_name)) == 0){
            break;
        }
        current = current->next;
    }
    
    if(current->next == NULL){return -1;} // the room doesnt exist
    
    chat_room * room = current->next;
    current->next = room->next;
    free(room);
    return 0;
}
/**
 * 
 * Retrieve the chat room from the linked list database
 * 
 * @parameter chat_name     given name to identify the chat
 * @return chat_room        chat room of the given name, or a chat room with -1 as port number(DNE)
*/
chat_room* get_chatRoom(const char * chat_name){
    chat_room * ptr = head;
    
    if(ptr == NULL){return ptr;}
    
    while(ptr != NULL){
        if(strncmp(ptr->name,chat_name, strlen(chat_name)) == 0){
            return ptr;
        }
        ptr = ptr->next;
    }
    
    return NULL;
}

/**
 * 
 * Gets the name of the rooms
 * 
 * @parameter              reply in which to append the names
*/
void get_roomList(struct Reply * reply){
    char room_list[MAX_DATA];
    room_list[0] = '\0';
    
    chat_room * current_room = head;
    
    while(current_room != NULL){
        strcat(room_list, current_room->name);
        strcat(room_list, ",");
        current_room = current_room->next;
    }
    
    strcpy(reply->list_room, room_list);
}