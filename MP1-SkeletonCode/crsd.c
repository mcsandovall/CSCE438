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
struct Reply create_room(const char * room_name, const int server_socket);
struct Reply join_room(const char * room_name, cons);
struct Reply delete_room(const char * room_name);
chat_room_t * search(const char * room_name);
void process_command(const int client_port);
void send_message(const chat_room_t chat_room, const char * message);
void client_worker(const chat_room_t * room);
void listen_worker(const chat_room_t * room, const int client_port);

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

    if(listen(server_socket,MAX_MEMBER) < 0){
        perror("Server: listen");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(client_socket = accept(server_socket, (struct sockaddr *) &server, sizeof(server)) < 0){
            perror("server: accept");
        }
        
        // make a thread that parses the command
        pthread_t ctid;
        pthread_create(&ctid, NULL, &process_command, client_socket);
    }
    return 0;
}

struct Reply create_room(const char * room_name, const int client_socket){
    // send a reply to the client
    struct Reply reply;
    reply.status = SUCCESS;
    
    // check if room exist in the databse
    chat_room_t * room =  search(room_name);
    if(room != NULL){
        reply.status = FAILURE_ALREADY_EXISTS;
        return reply;
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
        return reply;
    }
    
    if(bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){
        reply.status = FAILURE_UNKNOWN;
        perror("Server: bind");
        return reply;
    }
    
    // create a new entry in the db
    chat_room_t new_room;
    new_room.port_number = port_number;
    new_room.num_members = 0;
    strcpy(new_room.name,room_name);
    new_room.master_socket = sockfd;
    new_room.address = socket;
    room_db[num_rooms] = new_room;
    ++num_rooms;
    
    // make a thread to start listening and accepting conections for the clients
    
    
    return reply;
}

struct Reply join_room(const char * room_name){
    // create a reply for the server to send in case of the command 
    struct Reply reply;
    reply.status = SUCCESS;
    // check if the room exist
    chat_room_t * room  = search(room_name);
    if(room == NULL){
        reply.status = FAILURE_NOT_EXISTS;
        return reply;
    }
    
    // return the port number and the amount of people in the chat room
    reply.port = room.port_number;
    reply.num_member = room.num_members;
    
    return reply;
}

struct Reply delete_room(const char * room_name){
    struct Reply reply;
    reply.status = SUCCESS;
    
    // check if the room exist
    chat_room_t * room = search(room_name);
    if(room == NULL){
        reply.status = FAILURE_NOT_EXISTS;
        return reply;
    }
    
    // send a warning message to everyone
    send_message(room, "WARNING: ROOM IS CLOSING, ALL CONNECTIONS WILL TERMINATE");
    
    // close the master socket
    close(*(room->slave_socket[0]));
    return reply;
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

void process_command(const int client_port){
    command cmd;
    struct Reply reply;
    while(recv(client_port, &cmd, sizeof(cmd)) > 0){
        switch(cmd.type){
            case CREATE:
                reply = create_room(cmd.chat_name);
            case DELETE:
                reply = delete_room(cmd.chat_name);
            case JOIN:
                reply = join_room(cmd.chat_name);
            case LIST:
                reply = room_list(cmd.chat_name);
        }
        send(client_port, &reply, sizeof(reply), 0);
        if(cmd.type == JOIN){ // no more commands after join
            break;
        }
    }
}

void send_message(const chat_room_t * chat_room, const char * message){
    // send a message to all clients in the chat room
    int i;
    for(i = 1; i <= chat_room->num_members; ++i){
        if(send(chat_room->slave_socket[i], message, sizeof(message), 0) < 0){
            perror("Message: can not be sent");
        }
    }
}

void client_worker(const chat_room_t *room){
    
    // listen with the master socket, and create a new sockfd for every client that 
    if(listen(room->slave_socket[0], MAX_MEMBER) < 0){
        perror("Server: listen");
    }
    int client_socket;
    while(1){
        if(client_socket = accept(room->slave_socket[0], (struct sockaddr *) room->address, sizeof(room->address)) > -1){
            // add the client socket to the db in the chat room
            room->slave_socket[num_members + 1]; // 0 is master socket
            room->num_members = room->num_members + 1;
        }
        
        
    }
}