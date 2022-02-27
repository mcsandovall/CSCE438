#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <thread>
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
#include <signal.h>
#include "sns.grpc.pb.h"
#include <iomanip>
#include <sstream>
#include <mutex>

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

void termination_handler(int sig);
// use vector for the database
std::vector<User> user_db;
std::vector<User> current_db;
std::mutex p_mtx;

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    
    // find the user in the current db
    std::string username = request->username();
    User * usr = findUser(username, current_db);
    if(!usr){ // user doesnt exist for wtv reasosn
      reply->set_msg("ERROR USER DOESNT EXIST");
      return Status::CANCELLED;
    }
    
    // get the names of all the current users 
    for(User usr : current_db){
      reply->add_all_users(usr.get_username());
    }
    
    // get the list of users following the current users
    for(std::string follower : usr->getListOfFollwers()){
      reply->add_following_users(follower);
    }
    
    reply->set_msg("SUCCESS");
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    
    // get the current user
    std::string username = request->username();
    User * usr = findUser(username, current_db);
    if(!usr){
      reply->set_msg("ERROR_USER_DOESNT_EXIST");
      return Status::CANCELLED;
    }
    
    // find the user to follow
    std::string f_username = request->arguments(0);
    User * followee = findUser(f_username, current_db);
    if(!followee){ // user does not exist
      reply->set_msg("FAILURE_INVALID_USERNAME");
      return Status::OK;
    }
    
    // follow the user
    std::string UStatus = usr->follow_user(f_username);
    if(UStatus != "SUCCESS"){
      reply->set_msg(UStatus);
      return Status::OK;
    }
    
    // add current as a follower for the other user
    std::string FStatus = followee->add_follower(username);
    if(FStatus != "SUCCESS"){
      reply->set_msg(FStatus);
      return Status::OK;
    }
    
    // success the process was completed
    reply->set_msg(UStatus);
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    
    // get current user
    std::string current_username = request->username();
    User * c_usr = findUser(current_username, current_db);
    if(!c_usr){
      reply->set_msg("ERROR_USER_DOESNT_EXIST");
      return Status::CANCELLED;
    }
    
    // get the user to unfollow
    std::string other_user = request->arguments(0);
    User * o_usr = findUser(other_user, current_db);
    if(!o_usr){ // other user doesnt exist
      reply->set_msg("FAILURE_INVALID_USERNAME");
      return Status::OK;
    }
    
    // unfollow the user
    std::string c_status = c_usr->unfollow_user(other_user);
    if(c_status != "SUCCESS"){
      reply->set_msg(c_status);
      return Status::OK;
    }
    
    // remove current user from other following list
    std::string o_status = o_usr->remove_follower(current_username);
    if(o_status != "SUCCESS"){
      reply->set_msg(o_status);
      return Status::OK;
    }
    
    reply->set_msg(c_status);
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    
    // check if user already exist
    std::string c_username = request->username();
    User * c_usr = findUser(c_username, current_db);
    if(!c_usr){
      // look for it in the global db
      c_usr = findUser(c_username, user_db);
      if(!c_usr){ // if not in either crete a new one
        current_db.push_back(User(c_username));
        std::ofstream ofs(c_username + ".txt");
        ofs.close();
        std::cout << c_username << " sucesfully connected ..." << std::endl;
      }else{
        // if successfully reconected add it to the current db
        current_db.push_back(*c_usr);
      }
    }
    
    if(c_usr){
      loadPosts(c_username, c_usr);
      if(c_usr->getUnseenPosts()->size() > 20){
        c_usr->getUnseenPosts()->erase(c_usr->getUnseenPosts()->begin(),c_usr->getUnseenPosts()->begin() + (c_usr->getUnseenPosts()->size()-20));
      }
    }
    
    reply->set_msg("SUCCESS");
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // -----------------------------------------------------------
    
    Message message;
    std::string c_username;
    std::string msg;
    User * usr = nullptr;
    
    // get an inital command for the username
    if(stream->Read(&message)){
      c_username = message.username();
    }
    
    usr = findUser(c_username, current_db);
    usr->inTimeline = true;
    std::thread writer([stream](User * usr){
      while(usr->inTimeline){
        if(usr->getUnseenPosts()->size() == 0) continue;
        stream->Write(usr->getUnseenPosts()->back());
        usr->getUnseenPosts()->pop_back();
      }
    }, usr);
    
    
    writer.detach();
    
    User * flwr;
    time_t utc;
    while(stream->Read(&message)){
      // Add the post to all the followers list
      for(std::string follower : usr->getListOfFollwers()){
        if(follower != usr->get_username()){
          flwr = findUser(follower, current_db);
          if(!flwr)continue;
          flwr->add_unseenPost(message);
        }
        std::ofstream ofs(follower + ".txt", std::ios::app);
        utc = google::protobuf::util::TimeUtil::TimestampToTimeT(message.timestamp());
        ofs << c_username + "-" + (message.msg() + "-" + std::ctime(&utc));
      }
    }
    usr->inTimeline = false;
    return Status::OK;
  }
};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSServiceImpl service;
  
  // populate the server databse
  std::string server_address("127.0.0.1:" + port_no);
  std::string db = getDbFileContent(DB_PATH);
  ParseDB(db, &user_db);
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << port_no << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  
  // signal handler
  signal(SIGINT, termination_handler);
  
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

void termination_handler(int sig){
  // in case of the server failure or interruption write everything to the db file
  
  merge_vectors(current_db,user_db);
  UpdateFileContent(user_db);
  exit(1);
}