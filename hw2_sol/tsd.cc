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
#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <iomanip>
#include <thread>
#include <stdio.h>
#include <sstream>
#include <sys/stat.h>
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

enum RequestType{
  Login,
  Follow,
  Timeline,
};

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

void getFileMessages(const std::string &filename, std::vector<Message> &messages){
    std::ifstream ifs(filename);
    if(!ifs.is_open()){
      std::cerr << "Error: Can not open filename " + filename << std::endl;
      return;
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
      while(post[++index] != ';'){}
      username = post.substr(0, index);
      a_index = index + 1;
      while(post[++a_index] != ';'){}
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
  void sendRequest(const Request *r, RequestType t);
private:
  std::string ip_port;
  int id;
  ServerType type;
  std::unique_ptr<SNSCoordinator::Stub> cstub;
  Request copyRequest(const Request* r);
};

Request CServer::copyRequest(const Request * r){
  Request request;
  request.set_username(r->username());
  for(std::string a: r->arguments()){
    request.add_arguments(a);
  }
  return request;
}

void CServer::sendRequest(const Request * r, RequestType t){
  if(!stub_ || type != ServerType::MASTER) return;
  ClientContext context;
  Request request = copyRequest(r);
  Reply reply;
  if(t == RequestType::Login){
    Status stat = stub_->Login(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error sending login request ";
    }
    std::cout << reply.msg() << std::endl;
  }else if (t == RequestType::Follow){
    Status stat = stub_->Follow(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error sending login request ";
    }
    std::cout << reply.msg() << std::endl;
  }else if (t == RequestType::Timeline){
    Status stat = stub_->CopyTimeline(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error sending login request ";
    }
    std::cout << reply.msg() << std::endl;
  }
}

void CServer::Login(const std::string &cip,const std::string &cp){
  // create the stub
  std::string login_info = cip + ":" + cp;
  cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));

  ClientContext context;
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(ip_port);
  request.set_id(id);
  request.set_server_type(type);
  snsCoordinator::Reply reply;

  Status stat = cstub->Login(&context, request, &reply);
  if(!stat.ok()){
    std::cerr << "Coordinator can not be reached\n";
    std::exit(0);
  }
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
    }
  }
}

void CServer::createServerStub(const std::string &login_info){
  stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
  std::cout << "Connection established at: " << login_info << std::endl;
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

CServer* mys;
std::string CW_DIR;

void createFiles(const std::string &username){
  std::ofstream user_timeline(username + "_timeline.txt", std::ios_base::app);
  if(!user_timeline.is_open()){
    std::cerr << "Error: openning timeline file for" + username + "\n";
  }
  std::ofstream user_following(username + "_following.txt", std::ios_base::app);
  if(!user_following.is_open()){
    std::cerr << "Error: Opening following file for " + username + "\n";
  }
  std::ofstream user_followers(username + "_followers.txt", std::ios_base::app);
  if(!user_followers.is_open()){
    std::cerr << "Error: opening followers file for " + username + "\n";
  }
  user_timeline.close();
  user_following.close();
  user_followers.close();
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

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    // get the content from the all_users.txt file
    Client user = client_db[find_user(request->username())];
    // get the content from file
    std::vector<std::string> allusers = getFileContent("all_users.txt");

    for(std::string u : allusers){
      list_reply->add_all_users(u);
    }

    std::vector<std::string> followers = getFileContent(user.username + "_followers.txt");
    for(std::string f : followers){
      list_reply->add_followers(f);
    }

    // int index = 0;
    // for(Client c : client_db){
    //   list_reply->add_all_users(c.username);
    // }
    // std::vector<Client*>::const_iterator it;
    // for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
    //   list_reply->add_followers((*it)->username);
    // }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    if(mys->stub_) mys->sendRequest(request, RequestType::Follow);
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);

    // add the person the following.txt
    std::ofstream ofs(username1 + "_following.txt", std::ios_base::app);
    ofs << username2 + "\n";
    ofs.close(); 

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
    if(mys->stub_) mys->sendRequest(request,RequestType::Login);
    Client c;
    std::string username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      createFiles(username);
      // add the user to all_users.txt
      std::ofstream ofs("all_users.txt");
      ofs << username + "\n";
      ofs.close();
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
    Client *c;

    // send an inital message to get the username
    std::string username;
    if(stream->Read(&message)){
      username = message.username();
      int user_index = find_user(username);
      c = &client_db[user_index];
    }

    // make a thread to send all the post from the file
    std::thread write([stream](Client* c){
      while(c->connected){
        std::vector<Message> posts;
        getFileMessages(c->username + "_timeline.txt", posts);
        for(int i = 0; i < posts.size() && i < 20; ++i){
          stream->Write(posts[i]);
        }
      }
    },c);

    while(stream->Read(&message)){
      // read message and add it to the out.txt file
      std::ofstream ofs("out.txt", std::ios_base::app);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time+";"+message.username()+";"+message.msg();
      ofs << fileinput;
      ofs.close();

      // send the request to the slave
      if(mys->stub_){
        // make a request and add the time and shit to it
        Request request;
        request.set_username(message.username());
        request.add_arguments(fileinput);
        mys->sendRequest(&request, RequestType::Timeline);
      }
    }

  //   while(stream->Read(&message)) {
  //     std::string username = message.username();
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

  Status CopyTimeline(ServerContext * context, const Request* request, Reply* reply){
    // this mirrors the actions taken by the messages sent in the timeline
    // username to open file
    std::ofstream ofs(request->username() + "_timeline.txt");
    std::string input = request->arguments(0);
    ofs << input;
    ofs.close();
    return Status::OK;
  }
};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
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
  
  std::string host = "0.0.0.0",port = "3030", cip="0.0.0.0", cp="3010", id="1", t="master";
  // if(argc < 11){
  //   std::cerr << "Error: Not enough Arguments\n";
  //   std::exit(0);
  // }
  int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:c:i:t:")) != -1){
        switch(opt) {
          case 'h':
            cip = optarg;break;
          case 'p':
            cp = optarg;break;
          case 'c':
              port = optarg;break;
          case 'i':
            id = optarg;break;
          case 't':
            t = optarg;break;
          default:
          std::cerr << "Invalid Command Line Argument\n";
        }
    }
  // for(int i = 1; i < argc; ++i){
  //   if(std::strcmp(argv[i], "-cip") == 0) cip =  argv[i+1];
  //   if(std::strcmp(argv[i], "-cp") == 0) cp = argv[i+1];
  //   if(std::strcmp(argv[i], "-p") == 0) port = argv[i+1];
  //   if(std::strcmp(argv[i], "-id") == 0) id = argv[i+1];
  //   if(std::strcmp(argv[i], "-t") == 0) t = argv[i+1];
  // }
  mys = new CServer(host+":"+port, std::stoi(id), t);
  mys->Login(cip, cp);
  mys->RequestServers();
  //mys->ContactCoordinator();

  std::string dir = t + "_" + id;

  if(mkdir(dir.c_str(), 0777) < 0){
    std::cerr << "Error: Directory Already Exist\n";
  }

  // get the current working directory 
  CW_DIR = get_directory() + "/" + dir;

  // change direcotry
  if(chdir(CW_DIR.c_str()) < 0){
    std::cerr << "Error: Changing Direcotry\n";
    return -1;
  }

  // make the all users txt
  std::ofstream ofs("all_users.txt");
  ofs.close();
  
  // start the server communication
  RunServer(port);

  return 0;
}
