#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "interface.h"


/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);
void terminate_handler(int sig); // this is the functin that will handle the process being terminated
void recv_message(void * arg); // function to recieve messages and display them to the terminal
int terminate_chat = 0; // this flag allows to run chat mode until CNTR_C
int ip_converter(const char * host, char * ip);

pthread_t tid;

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();

	//signal(SIGINT, terminate_handler);
    int sockfd = connect_to(argv[1], atoi(argv[2]));
    
	while (1) {
	
		
 
		char command[MAX_DATA];
        get_command(command, MAX_DATA);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0) {
			printf("Now you are in the chatmode\n");
			close(sockfd);
			process_chatmode(argv[1], reply.port);
		}
		
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

    // below is just dummy code for compilation, remove it.
	// int sockfd = -1;
	// return sockfd;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		perror("Error creating socket");
		return sockfd;
	}
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	
	char ip[MAX_DATA];
	ip_converter(host,&ip);
	if(inet_pton(AF_INET, &ip, &server.sin_addr)<=0) 
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
	if (connect(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0){
		perror("Error connecting to server");
		return -1;
	}
	return sockfd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------

	// create a reply for the client to send the server
	struct Reply reply;
	reply.status = FAILURE_UNKNOWN; // set the status to unkown
	
	//Parse the command to create a new command
	Command cmd;
	cmd.type = UNKNOWN;
	
	// check for the type of command
	if(strncmp(command, "CREATE", 6) == 0){
		cmd.type = CREATE;
	}else if(strncmp(command, "DELETE", 6) == 0){
		cmd.type = DELETE;
	}else if (strncmp(command, "JOIN", 4) == 0){
		cmd.type = JOIN;
	}else if (strncmp(command, "LIST", 4) == 0){
		cmd.type = LIST;
	}else{
		return reply;
	}
	
	// set the chat name to the command (if not a list)
	if(cmd.type != LIST){
		int i;
		for(i =0; command[i] ; ++i){
			if(command[i] == ' '){ // check for the space to split the command and the name
				char buff[strlen(command)-i];
				memcpy(buff, &command[i+1], strlen(command));
				strcpy(cmd.chat_name, buff);
				break;
			}
		}
	}
	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------

	//send the command to the server as it is so that the server can parse this command
	if(send(sockfd, &cmd, sizeof(cmd), 0) < 0){
		perror("Error: Command not able to be sent");
		return reply;
	}

	// recieve reply from the server
	read(sockfd, &reply, sizeof(reply));
	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------
	
	return reply;
}

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------
	int sockfd =  connect_to(host, port);
	if(sockfd < 0){
		perror("chat connection");
	}
	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	char user_msg[MAX_DATA];
	get_message((char *) &user_msg, MAX_DATA); // get message from the user
	
	// send a message from the user to the server
	if(send(sockfd, user_msg, MAX_DATA, 0) < 0){
		perror("Error: Message can not be sent");
		return;
	}

	pthread_create(&tid, NULL,(void *) &recv_message, &sockfd);

	char msg[MAX_DATA];
	
	while(!terminate_chat){
		// recieve input form the user
		get_message((char *) &msg, MAX_DATA);

		// send the message
		send(sockfd, &msg, sizeof(msg), 0);
	}	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
}

void terminate_handler(int sig){
	terminate_chat = 1;
}

void recv_message(void * arg){
	int * sockfd = (int *) arg;
	char msg[MAX_DATA];
	
	while(recv(*sockfd,msg,MAX_DATA,0) > 0){
		display_message((char *) &msg);
	}
}

int ip_converter(const char * host, char * ip){
	struct hostent *_host;
	struct in_addr **addr_list;
	
	int i;
	if ( (_host = gethostbyname(host)) == NULL) {
		return -1;
	}

	addr_list = (struct in_addr **) _host->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}
	
	return -1;
}
