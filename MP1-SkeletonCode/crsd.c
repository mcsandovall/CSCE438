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
 * Functions for the server to talk with the client
*/
int create_room(const char * room_name, const int server_socket);
int join_room(const char * room_name);
int delete_room(const char * room_name);
chat_room_t * search(const char * room_name);

// Database of rooms 
chat_room_t room_db[MAX_ROOM];
int num_rooms = 0;
int port_number;

int main(int argc, char** argv){
    
    port_number = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_number);

    if(server_socket = socket(AF_INET, SOCK_STREAM, 0) < 0){
        perror("Server: socket");
        exit(EXIT_FAILURE);
    }

    if(bind(server_socket, (struct sockaddr *) &server, sizeof(server)) < 0){
        perror("Server: bind");
        exit(EXIT_FAILURE);
    }

    if(listen(server_socket,20) < 0){
        perror("Server: listen");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(client_socket = accept(server_socket, (struct sockaddr *) &server, sizeof(server)) < 0){
            perror("server: accep");
        }
    }
    return 0;
}

int create_room(const char * room_name, const int client_socket){
    // send a reply to the client
    struct Reply reply;
    reply.status = SUCCESS;
    
    // check if room exist in the databse
    chat_room_t * room =  search(room_name);
    if(room != NULL){
        reply.status = FAILURE_ALREADY_EXISTS;
        if(send(client_socket, &reply, sizeof(reply), 0) < 0){
            perror("server: message unsent");
            return -1;
        }
        return -1;
    }
    
    // open a new master socket
    int sockfd;;
    struct sockaddr_in socket;
    socket.sin_family = AF_INET;
    socket.sin_addr.s_addr = INADDR_ANY;
    socket.sin_port = htons(++port_number);
    
    if(sockfd = socket(AF_INET, SOCK_STREAM, 0) < 0){
        reply.status = FAILURE_UNKNOWN;
        perror("server: create the socket");
        
    }
    
    if(bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){
        reply.status = FAILURE_UNKNOWN;
        perror("Server: bind");
        
    }
    
    // make the a thread wait for the clients to connect
    
    // create a new entry in the db if the all the process where successful
    if(reply.result == SUCCESS){
        chat_room_t new_room;
        new_room.port_number = port_number;
        new_room.num_members = 0;
        strcpy(new_room.name,room_name);
        room_db[num_rooms] = new_room;
        ++num_rooms;
    }
    
    if(send(client_socket, &reply, sizeof(reply), 0) < 0){
        perror("server: message unsent");
        return -1;
    }
    
    return sockfd;
}

int join_room(const char * room_name){
    // create a reply for the server to send in case of the command 
    struct Reply reply;
    reply.status = ;
    // check if the room exist
    chat_room_t * room  = search(room_name);
    if(room == NULL){
        
    }
    return -1;
}

int delete_room(const char * room_name){
    return -1;
}

chat_room_t * search(const char * room_name){
    int i;
    for(i = 0; i < num_rooms; ++i){
        if(strncmp(room_db[i], room_name, strlen(room_name)) == 0){
            return &room_db[i]; 
        }
    }
    return NULL;
}