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
    Synchronizer(const std::string &si, const int &_id) : server_info(si), id(_id){}
    int reachCoordinator(const std::string &cip,const std::string &cp);
    int contactSynchronizer();
    void createStub(const int &id, const std::string &login_info);
    void sendNewUser(const int &id);
    void sendFollowRequest(const int &from, const int &to);
    void sendTimelineMsg(const int &id, const string &msg);
private:
    int id;
    std::string server_info;
    std::unique_ptr<SNSCoordinator::Stub> cstub;
    std::map<int, std::unique_ptr<SNSFSynch::Stub>> fstubs; // syncher stubs
};

Synchronizer * myf = 0;
string MASTER_DIR;
std::vector<File* > file_db;
vector<string> user_db;

// finds file in the database
File * findFile(const string &filename){
  for(File* f : file_db){
    if(f->uname == filename){
      return f;
    }
  }
  return nullptr;
}

vector<string> getFileContent(const string &filename){
  vector<string> content;
  std::ifstream ifs(filename);
  if(!ifs.is_open()){
    std::cerr << "Error opening file " + filename + "\n";
    return content;
  }
  string msg;
  while(!ifs.eof()){
    std::getline(ifs, msg);
    content.push_back(msg);
  }
  return content;
}

vector<string> getFollowingList(const string &uname){
  return getFileContent(MASTER_DIR + uname + "_following.txt");
}

vector<string> getFollowersList(const string &uname){
  return getFileContent(MASTER_DIR + uname + "_followers.txt");
}

vector<string> getOutPost(const string &uname){
  return getFileContent(MASTER_DIR + uname + "_out.txt");
}

int contentExist(const vector<string> &vec, const string &content){
  for(int i = 0; i < vec.size(); ++i){
    if(vec[i] == content){
      return i;
    }
  }
  return -1;
}

vector<string> getUniqueValues(const vector<string> &file1, const vector<string> file2){
  vector<string> unique;
  for(string c : file1){
    if(contentExist(file2, c) == -1){
      unique.push_back(c);
    }
  }
  return unique;
}

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

int Synchronizer::contactSynchronizer(){
  // get the ports from the coordinator and create the stub from their id
  ClientContext context;
  snsCoordinator::Request request;
  request.set_requester(RequesterType::SERVER);
  request.set_port_number(server_info);
  request.set_id(id);
  request.set_server_type(ServerType::SYNCHRONIZER);
  snsCoordinator::Reply reply;

  Status stat = cstub->ServerRequest(&context, request, &reply);
  if(!stat.ok()) return -1;
  // get the server id and the login info from the reply
  std::string msg(reply.msg());
  if(msg == "-") return 0; // no other sync
  // split the msg in two by the dash -
  int index = msg.find('-');
  std::string synch1 = msg.substr(0,index);
  std::string synch2 = msg.substr(index+1, msg.size());
  if(synch1 != ""){
    // get the id of the synch and create the stub
    int sid = synch1[0]; // its the id for the synch
    std::string s_info = synch1.substr(2, synch1.size());
    createStub(sid, s_info);
  }
  if(synch2 != ""){
    int sid = synch2[0]; // its the id for the synch
    std::string s_info = synch2.substr(2, synch2.size());
    createStub(sid, s_info);
  }
  return 1;
}

void Synchronizer::createStub(const int &id, const std::string &login_info){
  // make the stub at that index of the map
  fstubs[id] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
}

void Synchronizer::sendNewUser(const int &id){
  // send the new user to the other synch if it exist
  ClientContext context;
  Request request;
  request.set_follower(id);
  Reply reply;
  int sid = id + 1;
  if(sid == 4) sid = 1;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->NewUser(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error: sending new user to synchronizer";
    }
  }
  sid = id - 1;
  if(sid == 0) sid = 3;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->NewUser(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error: sending new user to synchronizer";
    }
  }
}

void Synchronizer::sendFollowRequest(const int &from, const int &to){
  ClientContext context;
  Request request;
  request.set_follower(from);
  request.set_followed(to);
  Reply reply;
  Status stat = fstubs[(to % 3) + 1]->Follow(&context, request, &reply);
  if(!stat.ok()){
    std::cerr<< "Error: sending Follow request\n";
  }
}

void Synchronizer::sendTimelineMsg(const int &id, const string &msg){
  ClientContext context;
  TimelineMsg tmsg;
  tmsg.set_id(id);
  tmsg.set_post(msg);
  Reply reply;
  Status status = fstubs[(id % 3) + 1]->Timeline(&context, tmsg, &reply);
  if(!status.ok()){
    std::cerr << "Error: sending timeline message\n";
  }
}

// function that adds all the file names to a vector
void get_filenames(std::vector<std::string> &vec, bool names=false){
  DIR *dir;
  struct dirent *ent;
  vec.clear();
  if ((dir = opendir (MASTER_DIR.c_str())) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      std::string file(ent->d_name);
      if(file.size() > 4){
        // check if it says txt at the end
        if(file[file.size()-1] == 't' && file[file.size()-2] == 'x' && file[file.size()-3] == 't'){
          file = file.substr(0, file.size()-4);
          if(names){
            int index = file.find('_');
            file = file.substr(0,index);
          }
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
string get_updateTime(const std::string &filename){
  struct stat sb;
  if(stat(filename.c_str(), &sb) == -1){
    std::cerr << "Error getting stat\n";
    return "";
  }

  std::stringstream ss(ctime(&sb.st_ctime));
  int i = 0; 
  std::string s;
  while(ss >> s && i++ != 3){}
  return s;
}

// this returns the unique values added to the file
vector<string> updateFile(const string &filename){
  File * f = findFile(filename);
  vector<string> content;
  if(!f){ // file doesnt exist create the file and add the content
    string updt = get_updateTime(filename);
    f = new File(filename, updt);
    f->content = getFileContent(filename);
    file_db.push_back(f);
    return f->content;
  }
  string updt = get_updateTime(filename);
  if((*f) < updt){
    content = getFileContent(filename);
    return getUniqueValues(content,f->content);
  }
  return content;
}

string getUsernameFromMessage(const string &msg){
  int index = msg.find(':');
  string uname = msg.substr(index+2, msg.size());
  index = uname.find(':');
  uname = uname.substr(0,index);
  return uname;
}

void updateAllUsersFile(Synchronizer* synch,const vector<string> &content){
  File * f = findFile("all_users.txt");
  if(!f){
    // make an all users file and get the time
    std::ofstream ofs("all_users.txt");
    ofs.close();
    string updt = get_updateTime("all_users.txt");
    f = new File("all_users.txt",updt);
    f->content = content;
    file_db.push_back(f);
    return;
  }
  // add the unique content of the file and add it to the file
  vector<string> unique = getUniqueValues(content, f->content);
  for(string u : unique){
    f->content.push_back(u);
  }
}

void monitorFollowingFiles(Synchronizer* synch,const vector<string> &unames){
  vector<string> unique;
  for(string f : unames){
    unique = updateFile(MASTER_DIR + f + "_following.txt");
    // send a follow request to all the synchronizers
    for(string u : unique){
      synch->sendFollowRequest(std::stoi(f), std::stoi(u));
    }
  }
}

void monitorTimelineChanges(Synchronizer* synch,const vector<string> &unames){
  // monitor th out.txt file, get the changes and send them to the timeline
  vector<string> messages = getFileContent(MASTER_DIR + "out.txt");
  for(string msg : messages){
    // get the username and send it to the followers
    string uname = getUsernameFromMessage(msg);
    // send the message to all his followers
    vector<string> followers = getFollowersList(uname);
    for(string f : followers){
      synch->sendTimelineMsg(std::stoi(uname), msg);
    }
  }
  std::ofstream ofs(MASTER_DIR + "out.txt");
  ofs.clear();
  ofs.close();
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

  Status Timeline(ServerContext* context, const TimelineMsg* lrequest, Reply* reply) override{
    // handles timeline modifying for user
    // get the user from the message and add the file to their timeline
    int user = lrequest->id();
    std::ofstream file(MASTER_DIR + std::to_string(user) + "_timeline.txt");
    file << lrequest->post() + "\n";
    file.close();
    return Status::OK;
  }

  Status NewUser(ServerContext* context, const Request* request, Reply* reply) override{
    // add new user to the all user list
    std::ofstream file("all_users.txt", std::ios_base::app);
    file << request->follower() << "\n";
    file.close();
    reply->set_msg("Success");
    return Status::OK;
  }
};

void fsynchworker(Synchronizer * synch){
  while(true){
    // get the names of all the files
    vector<string> fnames;
    get_filenames(fnames, true);
    // update the all user file
    updateAllUsersFile(synch, fnames);
    // check if there was a change in the following files
    monitorFollowingFiles(synch, fnames);
    // check changes in the timeline
    monitorTimelineChanges(synch, fnames);
    // sleep for 30s
    sleep(30);
  }
}

void RunServer(std::string server_address){
    SNSFSynchImp service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Coordinator listening on " << server_address << std::endl;

    server->Wait();
}

string get_directory(){
  char buff[256];
  getcwd(buff, 256);
  string ret(buff);
  return ret;
}

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
  MASTER_DIR = get_directory() + "/master" + std::to_string(id) + "/";

  myf = new Synchronizer(host + ":" + port, id); 
  if(!myf->reachCoordinator(cip, cp)){
      std::cerr << "Synchronizer: Coordinator Unreachable\n";
      std::exit(EXIT_FAILURE);
  } 
  // contact the other coordinators
  if(myf->contactSynchronizer() == 0){
    std::cout << "No synchronizers available at this time\n";
  }

  // make a thread that checks files and calls the other synchronizers
  // std thread synchworker().detach()
  std::thread worker(fsynchworker, myf);
  worker.detach();

  // run the server
  RunServer(host + ":" + port);
}