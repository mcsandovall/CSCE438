#include <ctime>
#include <thread>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <chrono>

#include "server.h" 

#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include "sns.grpc.pb.h"
#include "snc.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using snsCoordinator::HeartBeat;
using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

#define DB_PATH "user_db.json"

class CServer{
public:
  CServer(const std::string &port_no, const int &id, const std::string &t){
    if(t == "master"){
      type = ServerType::MASTER;
    }else{
      type = ServerType::SLAVE;
    }
  }
  // functions to talk with the coordinator
  int contactCoordinator(std::string &cip, std::string &cp);
  void messageCoordinator();
private:
  std::string port;
  int id;
  ServerType type;
  std::unique_ptr<SNSCoordinator::Stub> cstub;
  snsCoordinator::Request createRequest();
};

//Function implementation for the Server class
snsCoordinator::Request CServer::createRequest(){
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(port);
  request.set_id(id);
  request.set_server_type(type);
  return request;
}

HeartBeat createHeartBeat(int &id, ServerType &type){
  HeartBeat hb;
  hb.set_sid(id);
  hb.set_s_type(type);
  google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
  timestamp->set_seconds(time(NULL));
  timestamp->set_nanos(0);
  hb.set_allocated_timestamp(timestamp);
  return hb;
}

//First message with the coordinator to send all current info
int CServer::contactCoordinator(std::string &cip, std::string &cp){
  // create a stub to handle the communication w the coordinator
  std::string login_info = cip + ":" + cp;
  cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));

  // create a request and send it to the coorindator
  snsCoordinator::Request request = createRequest();
  ClientContext context;
  snsCoordinator::Reply reply;

  Status status = cstub->Login(&context, request, &reply);
  // check the possibilities from the coordinator
  if(reply.msg() == "full") return -1; // the cluster is full
  if(reply.msg() == "demoted") type = ServerType::SLAVE; // the server got demoted from master
  return 0; // success
}

void CServer::messageCoordinator(){
  // make threads to handle communication with the coordinator
  ClientContext context;
  HeartBeat hbt;
  std::shared_ptr<ClientWriter<HeartBeat>> stream(
            cstub->ServerCommunicate(&context, &hbt));
  // writes messages to the coordinator make every 5s to increase perfromace
  std::thread writer([stream](int id,ServerType type){
    HeartBeat hb;
    while(true){
      hb = createHeartBeat(id, type);
      stream->Write(hb);
      // sleep for 5 seconds before sending another msg
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    stream->WritesDone();
  }, this->id, this->type);

  // non blocking thread to continue process
  writer.detach();
}


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
        if(follower != usr->get_username()){ // add the message to followers queue
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
  
  std::string port = "3010", cip, cp, t;
  int id;
  // check the flags
  if(argc < 11){std::cerr << "Error: Not Enough Arguments"; return -1;}
  for(int i = 1; i < argc; ++i){
    if(std::strcmp(argv[i], "-cip") == 0){cip = argv[i+1];}
    if(std::strcmp(argv[i], "-cp") == 0){cp = argv[i+1];}
    if(std::strcmp(argv[i], "-p") == 0){port = argv[i+1];}
    if(std::strcmp(argv[i], "-id") == 0){id = std::stoi(argv[i+1]);}
    if(std::strcmp(argv[i], "-t") == 0){t = argv[i+1];}
  }
  // create the server
  CServer mys(port, id, t);
  // contact the coordinator
  if(mys.contactCoordinator(cip, cp) < 0){
    std::cerr << "Error contacting coordinator\n";
    return -1;
  }
  // establish the conection with the coordinator
  mys.messageCoordinator();
  //RunServer(port);

  return 0;
}

void termination_handler(int sig){
  // in case of the server failure or interruption write everything to the db file
  
  merge_vectors(current_db,user_db);
  UpdateFileContent(user_db);
  exit(1);
}
