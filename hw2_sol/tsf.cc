#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <queue>
#include <map>
#include <sys/stat.h>

#include "snc.grpc.pb.h"
#include "snf.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;

using std::vector;
using std::string;

using grpc::Status;
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

using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

using snsFSynch::Request;
using snsFSynch::Message;
using snsFSynch::Reply;
using snsFSynch::TimelineMsg;
using snsFSynch::SNSFSynch;

// structure to hold the file information
struct File{
  File(const string &u, const string &lpdt) : uname(u), lastUpdate(lpdt) {}
  std::string uname;
  std::string lastUpdate;
  vector<string> content;
  bool operator <(const File &file2){
    struct tm tm;
    strptime(lastUpdate.c_str(), "%H:%M:%S", &tm);
    time_t tml = mktime(&tm);
    strptime(file2.lastUpdate.c_str(), "%H:%M:%S", &tm);
    time_t tml2 = mktime(&tm);
    return (difftime(tml,tml2) < 0);
  }
  bool operator <(const string &time){
    struct tm tm;
    strptime(lastUpdate.c_str(), "%H:%M:%S", &tm);
    time_t tml = mktime(&tm);
    strptime(time.c_str(), "%H:%M:%S", &tm);
    time_t tml2 = mktime(&tm);
    return (difftime(tml,tml2) < 0);
  }
};

// implentation for the follower synchronizer

class Synchronizer{
public:
    Synchronizer(const string &si, const int &_id) : server_info(si), id(_id){}
    int reachCoordinator(const string &cip,const std::string &cp);
    int createSynchronizers();
    int contactSynchronizers();
    void createStub(const int &id, const string &login_info);
    void sendNewUser(const int &id);
    void sendFollowRequest(const int &from, const int &to);
    void sendTimelineMsg(const int &id, const string &msg);
private:
    int id;
    std::string server_info;
    std::unique_ptr<SNSCoordinator::Stub> cstub;
    std::map<int, std::unique_ptr<SNSFSynch::Stub>> fstubs; // syncher stubs
};

Synchronizer * myf;
string MASTER_DIR;
std::vector<File* > file_db;
vector<string> user_db;

int Synchronizer::reachCoordinator(const string &cip, const string &cp){
  string login_info = cip +":"+cp;
  cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));


  ClientContext context;
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(server_info);
  request.set_id(id);
  request.set_server_type(ServerType::SYNCHRONIZER);
  snsCoordinator::Reply reply;
  Status stat = cstub->Login(&context, request, &reply);
  if(!stat.ok()) return 0;

  return 1;
}

int Synchronizer::createSynchronizers(){
  // get the ip ports from the coordinator
  ClientContext context;
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(server_info);
  request.set_id(id);
  request.set_server_type(ServerType::SYNCHRONIZER);
  snsCoordinator::Reply reply;

  Status stat = cstub->ServerRequest(&context, request, &reply);
  if(!stat.ok()){
    std::cerr << "Error: Getting Synchronizers\n";
    return -1;
  }

  // parse the reply to get the synchronizers ip
  int index = reply.msg().find('-');
  int count = 0;
  string login_info = reply.msg().substr(0,index);
  if(login_info != ""){
    ++count;
    int sid = id + 1;
    if(sid > 3) sid = 1;
    fstubs[sid] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
  }
  login_info = reply.msg().substr(index+1,reply.msg().size());
  if(login_info != ""){
    ++count;
    int sid = id - 1;
    if(sid == 0) sid = 3;
    fstubs[sid] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
  }
  return count;
}

int Synchronizer::contactSynchronizers(){
  // send a contact for other synchronizers
  ClientContext context;
  Message message;
  message.set_id(id);
  message.set_server_info(server_info);
  Reply reply;
  int sid = id + 1;
  if(sid > 3) sid = 1;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->Contact(&context, message, &reply);
    if(!stat.ok()){
      std::cerr << "Error: Communicating with synchronizer\n";
    }
  }
  sid = id -1;
  if(sid < 1) sid = 3;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->Contact(&context, message, &reply);
    if(!stat.ok()){
      std::cerr << "Error: Communicating with synchronizer\n";
    }
  }
  return 1;
}

class SNSFSynchImp final : public  SNSFSynch::Service {
  Status Contact(ServerContext* context, const Message* message, Reply* reply) override{
    // creates a stub for syncher
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override{
    return Status::OK;
  }

  Status Timeline(ServerContext* context, const TimelineMsg* lrequest, Reply* reply) override{
    return Status::OK;
  }

  Status NewUser(ServerContext* context, const Request* request, Reply* reply) override{
    return Status::OK;
  }
};

void RunServer(std::string server_address){
    SNSFSynchImp service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Synchronizer listening on " << server_address << std::endl;

    server->Wait();
}

string get_directory(){
  char buff[256];
  getcwd(buff, 256);
  string ret(buff);
  return ret;
}

int main(int argc, char** argv){
    string host = "0.0.0.0",cip ="0.0.0.0", cp="3010", port="3050", id="1";

    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:c:i:")) != -1){
        switch(opt) {
            case 'h':
            cip = optarg;break;
            case 'p':
            cp = optarg;break;
            case 'c':
                port = optarg;break;
            case 'i':
            id = optarg;break;
            default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }
  MASTER_DIR = get_directory() + "/master_" + id + "/";

  myf = new Synchronizer(host + ":" + port, std::stoi(id)); 
  if(!myf->reachCoordinator(cip, cp)){
      std::cerr << "Synchronizer: Coordinator Unreachable\n";
      std::exit(EXIT_FAILURE);
  } 

  // contact the other coordinators
  if(myf->createSynchronizers() < 0){
    std::cerr << "Error: Reacher other synchronizers\n";
  }

  if(myf->contactSynchronizers() < 0){
    std::cerr << "Error: Contacting Sychronizers\n";
  }

  // // make a thread that checks files and calls the other synchronizers
  // // std thread synchworker().detach()
  // std::thread worker(fsynchworker, myf);
  // worker.detach();

  // run the server
  RunServer(host + ":" + port);
}