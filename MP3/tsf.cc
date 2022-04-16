#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <sys/stat.h>

#include "snc.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;

using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using snsCoordinator::Request;
using snsCoordinator::Reply;
using snsCoordinator::HeartBeat;
using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

// structure to hold the file information
struct File{
  std::string uname;
  std::string lastUpdate;
  bool operator <(const std::string &time2){
    struct tm tm, tm2;
    strptime(lastUpdate.c_str(), "%H:%M:%S", &tm);
    strptime(time2.c_str(), "%H:%M:%S", &tm2);
    time_t tml = mktime(&tm), tml2 = mktime(&tm2);
    return (difftime(tml2,tml) < 0);
  }
};


// implentation for the follower synchronizer

class Synchronizer{
public:
    Synchronizer(const std::string &p, const int &_id) : port(p), id(_id){}
    virtual int reachCoordinator(const std::string &cip,const std::string &cp);
private:
    int id;
    std::string port;
    std::unique_ptr<SNSCoordinator::Stub> cstub;
};

int Synchronizer::reachCoordinator(const std::string &cip, const std::string &cp) override{
    std::string login_info = cip + ":" + cp;
    cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));

    // create a request
    Request request;
    request.set_requester(RequesterType::SERVER);
    request.set_port_number(port);
    request.set_id(id);
    request.set_server_type(ServerType::SYNCHRONIZER);

    ClientContext context;
    Reply reply;

    Status status = cstub->Login(&context, request, &reply);
    if(reply.msg() == "full") return 0;
    return 1; // success
}

// function that adds all the file names to a vector
void get_filenames(std::vector<string> &vec, const std::string &directory){
  DIR *dir;
  struct dirent *ent;
  vec.clear();
  if ((dir = opendir (directory.c_str())) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      std::string file(ent->d_name);
      if(file.size() > 4){
        // check if it says txt at the end
        if(file[file.size()-1] == 't' && file[file.size()-2] == 'x' && file[file.size()-3] == 't'){
          file = file.substr(0, file.size()-4);
          vec.push_back(file);
        }
      }
      //printf ("%s\n", ent->d_name);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    std::cerr << "Cannot open file directory\n";
  }
}

// function to get the time the file was last updated
std::string get_updateTime(const std::string &filename){
  struct stat sb;
  if(stat(filename.c_str(), &sb) == -1){
    std::cerr << "Error getting stat\n";
  }

  std::stringstream ss(ctime(&sb.st_ctime));
  int i = 0; 
  string s;
  while(ss >> s && i++ != 3){}
  return s;
}

int main(int argc, char** argv){
    if(argc < 9){
        std::cerr << "Error: Not enough arguments\n";
        return -1;
    }
    std::string cip = "localhost", cp, port;
    int id;
    for(int i = 0; i < argc; ++i){
        if(std::strcmp(argv[i], "-cip") == 0) cip = argv[i+1];
        if(std::strcmp(argv[i], "-cp") == 0) cp = argv[i+1];
        if(std::strcmp(argv[i], "-p") == 0) port = argv[i+1];
        if(std::strcmp(argv[i], "-id") == 0) id = std::stoi(argv[i+1]);
    }
    Synchronizer mys(port, id); 
    if(!mys.reachCoordinator(cip, cp)){
        std::cerr << "Synchronizer: Coordinator Communicator\n";
        std::exit(EXIT_FAILURE);
    }
    
}