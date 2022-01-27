#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
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
struct Reply create_room(const char * room_name);
struct Reply join_room(const char * room_name);
struct Reply delete_room(const char * room_name);
struct Reply room_list();
chat_room_t * search(const char * room_name);
void process_command(const int client_port);
void send_message(const chat_room_t * chat_room, const char * message);
void client_worker(chat_room_t * room);
void listen_worker(chat_room_t * room);



// Database of rooms 
chat_room_t room_db[MAX_ROOM];
int num_rooms = 0;
int port_number;
pthread_mutex_t mtx;

int main(int argc, char** argv){
    printf("=========== CHAT ROOM SERVER ================\n");
    printf("=============================================\n");
    port_number = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_number);

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
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
    socklen_t addr_size = sizeof(server);
    printf("Strarting communication with clients \n");
    while(1){
        if((client_socket = accept(server_socket, (struct sockaddr *) &server, &addr_size)) > -1){
            // make a thread that parses the command for that client
            pthread_t ctid;
            pthread_create(&ctid, NULL, (void *) &process_command, &client_socket);
        }
        perror("server: accpeting client");
    }
    return 0;
}

/**
 * 
 * Creates a room from a given name, adds it to the database and starts a thread on that channel to handle communications
 * 
 * @parameter room_name             name of the room given by the client
 * @return  Reply                   retruns a constructed reply to send the client
*/
struct Reply create_room(const char * room_name){
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
    struct sockaddr_in server_socket;
    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = INADDR_ANY;
    server_socket.sin_port = htons(++port_number);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        reply.status = FAILURE_UNKNOWN;
        perror("server: create the socket");
        return reply;
    }
    
    if(bind(sockfd, (struct sockaddr *) &server_socket, sizeof(server_socket)) < 0){
        reply.status = FAILURE_UNKNOWN;
        perror("Server: bind");
        return reply;
    }
    
    // create a new entry in the db
    chat_room_t new_room;
    new_room.port_number = port_number;
    new_room.num_members = 0;
    strcpy(new_room.name,room_name);
    new_room.slave_socket[0] = sockfd;
    new_room.address = server_socket;
    room_db[num_rooms] = new_room;
    ++num_rooms;
    
    // make a thread to start listening and accepting conections for the clients
    pthread_t tid;
    pthread_create(&tid, NULL, (void *) &client_worker, &new_room);
    
    return reply;
}
/**
 * 
 * Handles the join room command and construct the necesary reply for the client to connect with that port
 * 
 * @parameter room_name             name of the room given by client
 * @return Reply                    constructed reply with the number of clients connected and the port number
*/
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
    reply.port = room->port_number;
    reply.num_member = room->num_members;
    
    return reply;
}

/**
 * 
 * Deletes a room from the database and sends a message to all connected clients
 * 
 * @parameter room_name             name of the room to delete
 * @retrun  Reply                   constructed reply with the appropraite status
*/
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
    close(room->slave_socket[0]);
    return reply;
}

/**
 * 
 * Gets the names of all current rooms
 * 
 * @return Reply            reply with all the names of the rooms added to the member list_room
*/
struct Reply room_list(){
    struct Reply reply;
    reply.status = SUCCESS;
    
    char room_list[MAX_DATA];
    room_list[0] = '\0';

    int i;
    for(i = 0; i < num_rooms; ++i){
        strcat(room_list,room_db[i].name);
        strcat(room_list,",");
    }

    strcpy(reply.list_room,room_list);
    return reply;
}

/**
 * 
 * Searches in the database for the given room
 * 
 * @parameter room_name             name of the room to be searched in the db
 * @return chat_room_t *              NULL if the room was not found, else return a pointer to the room
*/
chat_room_t * search(const char * room_name){
    int i;
    for(i = 0; i < num_rooms; ++i){
        if(strncmp(room_db[i].name, room_name, strlen(room_name)) == 0){
            return &room_db[i]; 
        }
    }
    return NULL;
}

/**
 * 
 * Processes commands recived from clients, takes appropriate action and sends reply to client
 * 
 * @parameter client_port               socket the client established for communication
*/
void process_command(const int client_port){
    Command cmd;
    struct Reply reply;
    while(recv(client_port, &cmd, sizeof(cmd), 0) > 0){
        switch(cmd.type){
            case CREATE:
                reply = create_room(cmd.chat_name);
            case DELETE:
                reply = delete_room(cmd.chat_name);
            case JOIN:
                reply = join_room(cmd.chat_name);
            case LIST:
                reply = room_list();
            case UNKNOWN:
                reply.status = FAILURE_UNKNOWN;
        }
        send(client_port, &reply, sizeof(reply), 0);
        if(cmd.type == JOIN){ // no more commands after join
            break;
        }
    }
}

/**
 * 
 * Sends message to all users in a given chat room 
 * 
 * @parameter chat_room             pointer to a chat room which message should be sent 
 * @parameter message               user defined message to be send to users in chat room
 * 
*/
void send_message(const chat_room_t * chat_room, const char * message){
    pthread_mutex_lock(&mtx);
    // send a message to all clients in the chat room
    int i;
    for(i = 1; i <= chat_room->num_members; ++i){
        if(send(chat_room->slave_socket[i], message, sizeof(message), 0) < 0){
            perror("Message: can not be sent");
        }
    }
    pthread_mutex_unlock(&mtx);
}

/**
 * 
 * Used by thread, handles the work of listening and accepting for all clients connecting to a chat room
 * 
 * @parameter room          pointer to a chat room to in which to add new memebers to 
*/
void client_worker(chat_room_t *room){
    
    // listen with the master socket, and create a new sockfd for every client that 
    if(listen(room->slave_socket[0], MAX_MEMBER) < 0){
        perror("Server: listen");
    }
    int client_socket;
    socklen_t addr_size =  sizeof(room->address);
    while(1){
        if((client_socket = accept(room->slave_socket[0], (struct sockaddr *) &room->address, &addr_size)) > -1){
            // add the client socket to the db in the chat room
            pthread_mutex_lock(&mtx);
            room->slave_socket[room->num_members + 1] = client_socket; // 0 is master socket
            room->num_members = room->num_members + 1;
            pthread_mutex_unlock(&mtx);
            
            // make a thread that listens to the client and send message to the room
            pthread_t tid;
            pthread_create(&tid, NULL, (void *) &listen_worker, room);
        }
    }
}

/**
 * 
 * used by thread, listens for all the communcations from a client and send messages to a chat room
 * 
 * @parameter room              room in whcih all communication occurs
*/
void listen_worker(chat_room_t * room){
    char buff[MAX_DATA];
    int client_id = room->num_members;
    while(recv(room->slave_socket[client_id + 1], &buff, MAX_DATA, 0) > -1){
        send_message(room, buff);
    }
}