#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

/**
 * Functions for the server to talk with the client
*/
struct Reply create_room(const char * room_name);
struct Reply join_room(const char * room_name);
struct Reply delete_room(const char * room_name);
struct Reply room_list();
chat_room_t * search(const char * room_name);
void process_command(void * arg);
void send_message(const chat_room_t * chat_room, const char * message, const int client_id);
void client_worker(void * arg);
void listen_worker(void * arg);


// Database of rooms
chat_room_t * room_db[MAX_ROOM];
int num_rooms = 0;
int port_number;
pthread_mutex_t mtx;
int num_clients = 0;
pthread_t client_threads[MAX_MEMBER];

int main(int argc, char** argv){
    printf("=========== CHAT ROOM SERVER ================\n");
    printf("=============================================\n");
    
    port_number = atoi(argv[1]);
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t tid;
    
    //signal(SIGPIPE, SIG_IGN);
    
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( port_number );
    
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    while(1){
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
       
        // process the command for this client
        ++num_clients;
        printf("Current number of clients connected: %d \n", num_clients);
        pthread_create(&tid, NULL, (void *) &process_command, (void *) &new_socket);
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
    chat_room_t * new_room = (chat_room_t *)malloc(sizeof(chat_room_t));
    new_room->port_number = port_number;
    new_room->num_members = 0;
    strcpy(new_room->name,room_name);
    new_room->slave_socket[0] = sockfd;
    new_room->address = server_socket;
    room_db[num_rooms] = new_room;
    
    pthread_mutex_lock(&mtx);
    ++num_rooms;
    pthread_mutex_unlock(&mtx);
    
    // make a thread to start listening and accepting conections for the clients
    pthread_t tid;
    pthread_create(&tid, NULL, (void *) &client_worker, new_room);
    
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
    send_message(room, "WARNING: ROOM IS CLOSING, ALL CONNECTIONS WILL TERMINATE \n", 0);
    
    // close all the connection with the clients for that room
    int i;
    for(i = 0; i < room->num_members;++i){
        close(room->slave_socket[i]);
    }
    
    // delete the instance of that room in memory
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
        strcat(room_list,room_db[i]->name);
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
    
    if(!num_rooms){
        return NULL;
    }
    
    int i;
    for(i = 0; i < num_rooms; ++i){
        if(room_db[i]){
            if(strncmp(room_db[i]->name, room_name, strlen(room_name)) == 0){
            return room_db[i]; 
            }
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
void process_command(void * arg){
    
    int client_socket = *((int *) arg);
    Command cmd;
    struct Reply reply;
    int valread;
    
    while(1){
        valread = recv(client_socket, &cmd, sizeof(cmd), 0);
        if(valread < 0){
           perror("Client aborted abnormally");
           close(client_socket);
           --num_clients;
           break;
        }
       
       if(valread == 0){
           perror("client stopped listening...");
           close(client_socket);
           break;
       }
       
       switch(cmd.type){
            case CREATE:
                reply = create_room(cmd.chat_name);
                printf("Number of chat rooms: %d \n", num_rooms);
                break;
            case DELETE:
                reply = delete_room(cmd.chat_name);
                break;
            case JOIN:
                reply = join_room(cmd.chat_name);
                break;
            case LIST:
                reply = room_list();
                break;
            case UNKNOWN:
                reply.status = FAILURE_UNKNOWN;
                break;
        }
        send(client_socket, &reply, sizeof(reply), 0);
        if(cmd.type == JOIN){
            close(client_socket);
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
void send_message(const chat_room_t * chat_room, const char * message, const int client_id){
    
    pthread_mutex_lock(&mtx);
    // send a message to all clients in the chat room
    int i;
    for(i = 1; i <= chat_room->num_members; ++i){
        if(i == client_id){
            continue;
        }else{
            if(send(chat_room->slave_socket[i], message, MAX_DATA, 0) < 0){
            perror("Message: can not be sent");
            }
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
void client_worker(void * arg){
    
    chat_room_t * room = (chat_room_t *) arg;
    // listen with the master socket, and create a new sockfd for every client that 
    if(listen(room->slave_socket[0], MAX_MEMBER) < 0){
        perror("Server: listen");
    }
    
    int client_socket;
    socklen_t addr_size =  sizeof(room->address);
    pthread_t tid;
    
    printf("Chat room %s started, waiting for connections... \n", room->name);
    
    while(1){
        
        client_socket = accept(room->slave_socket[0], (struct sockaddr *) &room->address, &addr_size);
        
        pthread_mutex_lock(&mtx);
        room->slave_socket[++room->num_members] = client_socket;
        pthread_mutex_unlock(&mtx);
        
        printf("%s: client connected, clients: %d \n", room->name, room->num_members);
        
        pthread_create(&tid, NULL, (void *) &listen_worker, room);
    }
}

/**
 * 
 * used by thread, listens for all the communcations from a client and send messages to a chat room
 * 
 * @parameter room              room in whcih all communication occurs
*/
void listen_worker(void * arg){
    
    chat_room_t * room = (chat_room_t *) arg;
    char buff[MAX_DATA];
    int client_id = room->num_members; // make the exception in the for loop
    int client_socket = room->slave_socket[room->num_members];
    int read;
    while(1){
        read = recv(client_socket, &buff, MAX_DATA, 0);
        if(read <= 0){
            break;
        }
        send_message(room, buff, client_id);
    }
    
    close(client_socket);
}