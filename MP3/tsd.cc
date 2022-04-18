/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"
#include "snc.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using snsCoordinator::HeartBeat;
using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

class CServer{
public:
  CServer(const std::string &ip, const int &sid, const std::string &t){
    ip_port = ip;
    id = sid;
    if(t == "master"){
      type = ServerType::MASTER;
    }else{
      type = ServerType::SLAVE;
    }
  }
  void Login(const std::string &cip,const std::string &cp);
  void RequestServers();
  void ContactCoordinator();
  void createServerStub(const std::string &login_info);
  std::unique_ptr<SNSService::Stub> stub_;
  int getID(){return id;}
private:
  std::string ip_port;
  int id;
  ServerType type;
  std::unique_ptr<SNSCoordinator::Stub> cstub;
};

// functions for the server

std::vector<std::string> getFileContent(const std::string &filename){
  std::vector<std::string> content;
  std::ifstream ifs(filename);
  if(!ifs.is_open()){
    std::cerr << "Error opening file " + filename + "\n";
    return content;
  }
  std::string msg;
  while(!ifs.eof()){
    std::getline(ifs, msg);
    content.push_back(msg);
  }
  return content;
}

void getFileMessages(const std::string &filename, std::std::vector<Message> messages){
   std::ifstream ifs(filename);
    if(!ifs.is_open()){
      std::cerr << "Error: Can not open filename " + filename << std::endl;
    }
    
    // get all the post in the file
    Message msg;
    int index = 0, a_index = 0; 
    std::string post, tm, message;
    time_t utc;
    struct std::tm tim;
    Timestamp timestamp;
    std::string username;
    while(!ifs.eof()){
      std::getline(ifs,post);
      if(post.empty())continue;
      while(post[++index] != '-'){}
      username = post.substr(0, index);
      a_index = index + 1;
      while(post[++a_index] != '-'){}
      message = post.substr(index+1, a_index-1);
      tm = post.substr(a_index+1, post.size() - a_index);
      index = 0; // reset the index
      
      msg.set_username(username);
      msg.set_msg(message);
      
      std::istringstream ss(tm);
      ss >> std::get_time(&tim, "%a %b %d %H:%M:%S %Y");
      utc = mktime(&tim);
      timestamp = google::protobuf::util::TimeUtil::TimeTToTimestamp(utc);
      msg.set_allocated_timestamp(&timestamp);
      
      messages.push_back(msg);
      msg.release_timestamp();
    }
}

void CServer::Login(const std::string &cip, const std::string &cp){
  // send the current server information to the coordinator
  cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials())));

  // make a request and send it to the coordinator
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(ip_port);
  request.set_id(id);
  request.set_server_type(type);
  snsCoordinator::Reply reply;
  ClientContext context;

  Status status = cstub->Login(&context, request, &reply);
  if(status.ok()){
    std::cout << "Coordinator reached succesfully\n";
  }else{
    // try again
    Login(cip,cp);
  }
  // check for the flags in the login
  if(reply.msg() == "full"){
    std::cerr << "Error: Cluster already full\n";
    std::exit(0);
  }
  else if (reply.msg() == "demoted") type = ServerType::SLAVE;
}

void CServer::RequestServers(){
  // request the info for the other server and crate a stub
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(ip_port);
  request.set_id(id);
  request.set_server_type(type);
  snsCoordinator::Reply reply;
  ClientContext context;

  Status status = cstub->ServerRequest(&context, request, &reply);
  // check if the message is null
  if(reply.msg() != "NULL"){
    // create stub and establish the bidirectional commuincation
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(reply.msg(), grpc::InsecureChannelCredentials())));
    if(type == ServerType::SLAVE){
      // send message to the master to create stub
      Request request;
      request.set_username(ip_port);
      Reply reply;
      ClientContext context;
      Status status = stub_->Communicate(&context, request, &reply);
      if(!status.ok()){
        std::cerr << "Error contacting the master server\n";
      }
      stub_.reset();
    }
  }
}

void CServer::createServerStub(const std::string &login_info){
  stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
}

CServer * mys; // global variable of server
std::string CW_DIR; 

HeartBeat createHeartBeat(const ServerType &t, const int &id){
  // this function creates a heartbeat for the server
  HeartBeat hb;
  hb.set_sid(id);
  hb.set_s_type(t);
  google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
  timestamp->set_seconds(time(NULL));
  timestamp->set_nanos(0);
  hb.set_allocated_timestamp(timestamp);
  return hb;
}

void CServer::ContactCoordinator(){
  // send a constant communication with the coordinator
  ClientContext context;
  HeartBeat hb;
  
  // create a detached thread that send the coordinator a message every 10s
  std::shared_ptr<ClientWriter<HeartBeat>> stream(cstub->ServerCommunicate(&context, &hb));
  std::thread writer([stream](const ServerType t,const int id){
    while(true){
      // send message to the coordinator every 10s
      HeartBeat h = createHeartBeat(t, id);
      stream->Write(h);
      sleep(10);
    }
    stream->WritesDone();
  }, type, id);

  writer.detach();
}

//Vector that stores every client that has been created
std::vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

void createFiles(const std::string &username){
  std::ofstream user_timeline(username + "_timeline.txt");
  if(!user_timeline.is_open()){
    std::cerr << "Error: openning timeline file for" + username + "\n";
  }
  std::ofstream user_following(username + "_following.txt");
  if(!user_following.is_open()){
    std::cerr << "Error: Opening following file for " + username + "\n";
  }
  std::ofstream user_followers(username + "_followers.txt");
  if(!user_followers.is_open()){
    std::cerr << "Error: opening followers file for " + username + "\n";
  }
  user_timeline.close();
  user_following.close();
  user_followers.close();
}

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    if(mys->stub_){ // send the same request to the slave
      Request req = *(Request*) request;
      ClientContext cont;
      Reply rep;
      Status stat = mys->stub_->Follow(&cont, req, &rep);
      if(!stat.ok()){
        std::cerr << "Follow request not sent to slave\n";
      }
      if(rep.msg() != "Follow Successful"){
        std::cerr << rep.msg() + "\n";
      }
    }
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    // append the name to following
    std::ofstream file(username1 + "_following.txt", std::ios_base::app);
    file << username2 + " ";
    file.close();

    // we wont be able to check the users name
    reply->set_msg("Follow Successful");

  //   int join_index = find_user(username2);
  //   if(join_index < 0 || username1 == username2)
  //     reply->set_msg("unkown user name");
  //   else{
  //     Client *user1 = &client_db[find_user(username1)];
  //     Client *user2 = &client_db[join_index];
  //     if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	// reply->set_msg("you have already joined");
  //       return Status::OK;
  //     }
  //     user1->client_following.push_back(user2);
  //     user2->client_followers.push_back(user1);
  //     reply->set_msg("Follow Successful");
  //   }
    return Status::OK; 
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    if(mys->stub_){
      // send the login message to the slave
      Request req = *(Request*) request;
      Reply rep;
      ClientContext con;
      Status stat = mys->stub_->Login(&con, req, &rep);
      if(!stat.ok() || rep.msg() != "Login Successful!"){
        std::cerr << "Error sending login to slave\n";
      }
    }
    Client c;
    std::string username = request->username();

    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      // create the files for the user
      createFiles(username);
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	reply->set_msg(msg);
        user->connected = true;
      }
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    std::string username;
    Client *c;

    if(stream->Read(&message)){
      username = message.username();
    }

    int index = find_user(username);
    c = &client_db[index];

    // TODO: make a thread to check the user timeline file and write to the stream
    std::thread writer([stream](Client* c){
      while(c->connected){
        std::string filename = c->username + "_timeline.txt";
        std::std::vector<Message> posts;
        getFileMessages(filename, posts);
        
        if(posts.size() > 0){
          // send all the post from the file
          for(int i = 0; i < posts.size(); ++i){
            stream->Write(posts[i]);
          }
        }
      }
      stream->WritesDone();
    },c);
    // TODO: while loop that reads messages and puts them in the out.txt file
    while(stream->Read(&message)){
      // read message and add it to the out.txt file
      std::ofstream ofs("out.txt", std::ios_base::app);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time+"-"+message.username()+"-"+message.msg()+"\n";
      ofs << fileinput;
      ofs.close();
    }

  //   while(stream->Read(&message)) {
  //     username = message.username();
  //     int user_index = find_user(username);
  //     c = &client_db[user_index];
 
  //     //Write the current message to "username.txt"
  //     std::string filename = username+".txt";
  //     std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
  //     google::protobuf::Timestamp temptime = message.timestamp();
  //     std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
  //     std::string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
  //     //"Set Stream" is the default message from the client to initialize the stream
  //     if(message.msg() != "Set Stream")
  //       user_file << fileinput;
  //     //If message = "Set Stream", print the first 20 chats from the people you follow
  //     else{
  //       if(c->stream==0)
  //     	  c->stream = stream;
  //       std::string line;
  //       std::vector<std::string> newest_twenty;
  //       std::ifstream in(username+"following.txt");
  //       int count = 0;
  //       //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
  //       while(getline(in, line)){
  //         if(c->following_file_size > 20){
	//     if(count < c->following_file_size-20){
  //             count++;
	//       continue;
  //           }
  //         }
  //         newest_twenty.push_back(line);
  //       }
  //       Message new_msg; 
 	// //Send the newest messages to the client to be displayed
	// for(int i = 0; i<newest_twenty.size(); i++){
	//   new_msg.set_msg(newest_twenty[i]);
  //         stream->Write(new_msg);
  //       }    
  //       continue;
  //     }
  //     //Send the message to each follower's stream
  //     std::vector<Client*>::const_iterator it;
  //     for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
  //       Client *temp_client = *it;
  //     	if(temp_client->stream!=0 && temp_client->connected)
	//   temp_client->stream->Write(message);
  //       //For each of the current user's followers, put the message in their following.txt file
  //       std::string temp_username = temp_client->username;
  //       std::string temp_file = temp_username + "following.txt";
	// std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	// following_file << fileinput;
  //       temp_client->following_file_size++;
	// std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
  //       user_file << fileinput;
  //     }
  //   }
    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }
  
  Status Communicate(ServerContext* context, const Request* request, Reply* reply) override {
    // create a new stub with the login information held in username
    std::string login_info = request->username();
    mys->createServerStub(login_info);
    return Status::OK;
  }
};

void RunServer(std::string server_address) {
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

std::string get_directory(){
  char buff[256];
  getcwd(buff, 256);
  std::string ret(buff);
  return ret;
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  std::string host =  "0.0.0.1"; // localhost
  std::string cip, cp, t;
  int id;
  if(argc < 11){
    std::cerr << "Not enough arguments\n";
    std::exit(0);
  }

  for(int i = 1; i < argc; ++i){
    if(std::strcmp(argv[i], "-cip")) cip =  argv[i+1];
    if(std::strcmp(argv[i], "-cp")) cp = argv[i+1];
    if(std::strcmp(argv[i], "-p")) port = argv[i+1];
    if(std::strcmp(argv[i], "-id")) id = std::stoi(argv[i+1]);
    if(std::strcmp(argv[i], "-t")) t = argv[i+1];
  }

  mys =  new CServer(host+":"+port, id, t);
  // Login with the coordinator
  mys->Login(cip, cp);
  // request for the other server slave/master
  mys->RequestServers();
  // create communication with the coordinator
  mys->ContactCoordinator();

  // make the new directory
  std::string dir = t + "_" + std::to_string(id);

  if((mkdir(dir.c_str(), 0777)) < 0){
    std::cerr << "Error making directory \n";
    std::exit(0);
  }

  // gte current direcotry
  CW_DIR = get_directory() + dir;

  // change the directory
  if((chdir(CW_DIR.c_str())) < 0){
    std::cerr << "Error changing direcotry\n";
    std::exit(0);
  }
  
  // connect to the coordinator first
  RunServer(host + ":" + port);

  return 0;
}
