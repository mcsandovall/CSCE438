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
using snsFSynch::ListRequest;
using snsFSynch::SNSFSynch;

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
    Synchronizer(const std::string &si, const int &_id) : server_info(si), id(_id){}
    int reachCoordinator(const std::string &cip,const std::string &cp);
    int contactSynchronizer();
    void createStub(const int &id, const std::string &login_info);
private:
    int id;
    std::string server_info;
    std::unique_ptr<SNSCoordinator::Stub> cstub;
    std::map<int, std::unique_ptr<SNSFSynch::Stub>> fstubs; // syncher stubs
};

struct Client{
  int id;
  std::vector<Client*> followed;
  std::vector<Client*> following;
};

Synchronizer * myf = 0;
std::string MASTER_DIR;
std::vector<Client> client_db;

int Synchronizer::reachCoordinator(const std::string &cip, const std::string &cp){
    std::string login_info = cip + ":" + cp;
    cstub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));

    // create a request
    snsCoordinator::Request request;
    request.set_requester(RequesterType::SERVER);
    request.set_port_number(server_info);
    request.set_id(id);
    request.set_server_type(ServerType::SYNCHRONIZER);

    ClientContext context;
    snsCoordinator::Reply reply;

    Status status = cstub->Login(&context, request, &reply);
    if(reply.msg() == "full") return 0;
    return 1; // success
}

void Synchronizer::createStub(const int &id, const std::string &login_info){
  // make the stub at that index of the map
  fstubs[id] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
}
// function that adds all the file names to a vector
void get_filenames(std::vector<std::string> &vec, const std::string &directory){
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
  std::string s;
  while(ss >> s && i++ != 3){}
  return s;
}

// function to get the names of all user who follow certain give user
std::vector<std::string> getFollowers(const std::string user){
  std::vector<std::string> followers;
  std::vector<std::string> users;
  get_filenames(users, MASTER_DIR);

  for(int i = 0; i < client_db.size(); ++i){
    // check if the id appears in the following list
    for(Client* c : client_db[i].following){
      if(std::to_string(c->id) == user){
        followers.push_back(std::to_string(c->id));
      }
    }
  }
  return followers;
}


class SNSFSynchImp final : public  SNSFSynch::Service {
  Status Contact(ServerContext* context, const Message* message, Reply* reply) override{
    // creates a stub for syncher
    myf->createStub(message->id(), message->server_info());
    reply->set_msg("Success");
    std::cout << "Stub create for synchronizer: " << message->id() << std::endl;
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override{
    // handles follow between cluster
    // get the follower id
    int follower = request->follower();
    int followed = request->followed();
    // modify the followed file and add the follower
    std::string dir = MASTER_DIR + "/" + std::to_string(followed) + "_followers.txt";
    std::ofstream file(dir, std::ios_base::app);
    file << std::to_string(follower) + "\n";
    file.close();
    reply->set_msg("Success");
    return Status::OK;
  }

  Status Timeline(ServerContext* context, const ListRequest* lrequest, Reply* reply) override{
    // handles timeline modifying for user
    // get a vector with all the users who follow the list id (username)
    // getFollowers();
    std::vector<std::string> followers = getFollowers(std::to_string(lrequest->id()));
    // get all the messages from the list
    std::queue<std::string> messages;
    for(std::string msg : lrequest->post()){
      messages.push(msg);
    }
    // add all the messages one by one to the list of followers
    while(messages.size() != 0){
      std::string msg = messages.front();
      messages.pop();
      for(std::string f : followers){
        std::string dir = MASTER_DIR + f + "_timeline.txt";
        std::ofstream file(dir, std::ios_base::app);
        file << msg + "\n";
      }
    }
    reply->set_msg("Success");
    return Status::OK;
  }

  Status NewUser(ServerContext* context, const Request* request, Reply* reply) override{
    // add new user to the all user list
    std::ofstream file("all_users.txt", std::ios_base::app);
    file << request->follower() << "\n";
    file.close();
    return Status::OK;
  }
};

int main(int argc, char** argv){
    if(argc < 9){
        std::cerr << "Error: Not enough arguments\n";
        return -1;
    }
    std::string cip, cp, port, host = "0.0.0.1";
    int id;
    for(int i = 0; i < argc; ++i){
        if(std::strcmp(argv[i], "-cip") == 0) cip = argv[i+1];
        if(std::strcmp(argv[i], "-cp") == 0) cp = argv[i+1];
        if(std::strcmp(argv[i], "-p") == 0) port = argv[i+1];
        if(std::strcmp(argv[i], "-id") == 0) id = std::stoi(argv[i+1]);
    }
    myf = new Synchronizer(host + ":" + port, id); 
    if(!myf->reachCoordinator(cip, cp)){
        std::cerr << "Synchronizer: Coordinator Unreachable\n";
        std::exit(EXIT_FAILURE);
    }
}