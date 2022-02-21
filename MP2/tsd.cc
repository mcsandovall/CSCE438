#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include "server.h"

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

#define DB_PATH "user_db.json"

// use vector for the database
std::vector<User> user_db;
std::vector<User> current_db;

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    
    // get a list of all users and find the user from the request
    std::string user_name = request->username();
    User * user = nullptr;
    reply->set_msg("Server: LIST");
    for(User usr : current_db){
      if(user_name == usr.get_username()){
        user = &usr;
      }
      reply->add_all_users(usr.get_username());
    }
    
    if(!user){
      return Status::UNKNOWN; // for wtv reason the user making the request doesnt exist
    }
    
    for(std::string followee : user->getFollowingList()){
      reply->add_following_users(followee);
    }
    
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    
    // get both the requester and the requestee username 
    std::string rqtr = request->username(), rqte = request->arguments(0); // get the first initial
    
    // find both users
    User * requester = findUser(rqtr, &current_db);
    if(!requester){ // the user is not active for wtv reason
      reply->set_msg("USER NOT FOUND");
      return Status::CANCELLED;
    }
    
    User * requestee = findUser(rqte, &current_db);
    if(!requestee){
      // it doesnt exit in the current db try in the global
      requestee = findUser(rqte, &user_db);
      if(!requestee){
        // user doesnt exist
        reply->set_msg("U FAILURE_INVALID_USERNAME");
        Status:OK;
      }
    }
    
    // if both users were found add to following list and this-> to list_followers
    reply->set_msg(requester->follow_user(rqte));
    // requestee
    requestee->add_follower(rqtr);
    
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    
    // get the username from the request
    std::string user_name = 
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSService service;
  
  // populate the server databse
  std::string db = getDbFileContent(DB_PATH);
  ParseDB(db, *user_db);
  
  ServerBuilder builder;
  builder.AddListeningPort(port_no, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
   return 0;
}
