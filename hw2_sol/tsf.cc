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
vector<File* > file_db;
vector<string> client_db;

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
    std::cout << "Stub Created on port: " << login_info << std::endl;
  }
  login_info = reply.msg().substr(index+1,reply.msg().size());
  if(login_info != ""){
    ++count;
    int sid = id - 1;
    if(sid == 0) sid = 3;
    fstubs[sid] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
    std::cout << "Stub Created on port: " << login_info << std::endl;
  }
  return count;
}

int Synchronizer::contactSynchronizers(){
  // send a contact for other synchronizers
  Message message;
  message.set_id(id);
  message.set_server_info(server_info);
  Reply reply;
  int sid = id + 1;
  if(sid > 3) sid = 1;
  if(fstubs[sid]){
    ClientContext context;
    Status stat = fstubs[sid]->Contact(&context, message, &reply);
    if(!stat.ok()){
      std::cerr << "Error: Communicating with synchronizer\n";
    }
  }

  sid = id -1;
  if(sid < 1) sid = 3;
  if(fstubs[sid]){
    ClientContext context;
    Status stat = fstubs[sid]->Contact(&context, message, &reply);
    if(!stat.ok()){
      std::cerr << "Error: Communicating with synchronizer\n";
    }
  }
  return 1;
}

void Synchronizer::createStub(const int &id, const string &login_info){
  if(!fstubs[id]){
    fstubs[id] = std::unique_ptr<SNSFSynch::Stub>(SNSFSynch::NewStub(grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
  }
}

void Synchronizer::sendNewUser(const int &id){
  // sends the new user to the synchronizers
  ClientContext context;
  Request request;
  request.set_follower(id);
  Reply reply;
  int sid = id + 1;
  if(sid > 3) sid = 1;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->NewUser(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error sending new user to synchronizer " << sid << std::endl;
    }
    std::cout << reply.msg() << std::endl;
  }

  sid = id -1;
  if(sid < 1) sid = 3;
  if(fstubs[sid]){
    Status stat = fstubs[sid]->NewUser(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error sending new user to synchronizer " << sid << std::endl;
    }
    std::cout << reply.msg() << std::endl;
  }
}

void Synchronizer::sendFollowRequest(const int &from, const int &to){
  // send the follow request to the right cluster
  int sid = (to % 3) + 1;
  if(fstubs[sid]){
    ClientContext context;
    Request request;
    request.set_follower(from);
    request.set_followed(to);
    Reply reply;
    Status stat = fstubs[sid]->Follow(&context, request, &reply);
    if(!stat.ok()){
      std::cerr << "Error: sending follow request to syncher " << sid << std::endl;
    }
    std::cout << reply.msg() << std::endl;
  }
}

void Synchronizer::sendTimelineMsg(const int &ids, const string &post){
  int sid = (ids % 3) + 1;
  if(sid == id){
    std::ofstream ofs(MASTER_DIR + std::to_string(ids) + "_timeline.txt", std::ios_base::app);
    ofs << post + "\n";
    ofs.close();
  }else{
    if(fstubs[sid]){
      ClientContext context;
      TimelineMsg msg;
      msg.set_id(ids);
      msg.set_post(post);
      Reply reply;
      Status stat = fstubs[sid]->Timeline(&context, msg, &reply);
      if(!stat.ok()){
        std::cerr << "Error: sending timeline post to syncher " << sid << std::endl;
      }
    }
  }
}

vector<string> getDirectoryFiles(const string &dire, bool names = false){
  vector<string> content;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir (dire.c_str())) != NULL) {
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
            if(file == "all" || file == "out" || file=="current") continue;
          }
          content.push_back(file);
        }
      }
      //printf ("%s\n", ent->d_name);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    std::cerr << "Cannot open file directory\n";
  }
  return content;
}

int contentExist(const string &content, const vector<string> &vec){
  for(int i = 0; i < vec.size();++i){
    if(vec[i] == content){
      return i;
    }
  }
  return -1;
}

vector<string> makeUnique(const vector<string> &vec){
  vector<string> unique;
  for(string e : vec){
    if(contentExist(e, unique) < 0){
      unique.push_back(e);
    }
  }
  return unique;
}

vector<string> getDiffernce(const vector<string> &oldv, const vector<string> &newv){
  vector<string> difference;
  for(string d : newv){
    if(contentExist(d,oldv) < 0){
      difference.push_back(d);
    }
  }
  return difference;
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

class SNSFSynchImp final : public  SNSFSynch::Service {
  Status Contact(ServerContext* context, const Message* message, Reply* reply) override{
    // creates a stub for syncher
    int sid = message->id();
    string log_info = message->server_info();
    std::cout << "Synchronizer " << sid << " reached on login: " << log_info <<std::endl;
    if(myf) myf->createStub(sid, log_info);
    reply->set_msg("Contacting Successful");
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override{
    // open the file for the follower
    string follower = std::to_string(request->follower());
    string followed = std::to_string(request->followed());
    std::ofstream ofs(MASTER_DIR +followed + "_followers.txt", std::ios_base::app);
    ofs << follower + "\n";
    ofs.close();
    reply->set_msg("Follow Successful");
    return Status::OK;
  }

  Status Timeline(ServerContext* context, const TimelineMsg* lrequest, Reply* reply) override{
    // add the timeline msg to the timeline
    string user = std::to_string(lrequest->id());
    std::ofstream ofs(MASTER_DIR + user+"_timeline.txt", std::ios_base::app);
    ofs << lrequest->post() + "\n";
    ofs.close();
    reply->set_msg("Timeline Successful");
    return Status::OK;
  }

  Status NewUser(ServerContext* context, const Request* request, Reply* reply) override{
    // add the user to all users txt
    string user = std::to_string(request->follower());
    std::ofstream ofs(MASTER_DIR+"all_users.txt", std::ios_base::app);
    ofs << user + "\n";
    ofs.close();
    reply->set_msg("New User Successful");
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

File * findFile(const string &filename){
  for(File* f : file_db){
    if(f->uname == filename){
      return f;
    }
  }
  return nullptr;
}

void updateAllUsers(Synchronizer* sync){
  // find the file and compare
  //std::cout << "Updating all_users....\n";
  string filename = MASTER_DIR + "all_users.txt";
  File * f = findFile(filename);
  if(!f){
    // doesnt exist make a new one
    vector<string> content = makeUnique(getDirectoryFiles(MASTER_DIR, true));
    string tm = get_updateTime(filename);
    f = new File(filename, tm);
    f->content = content;
    file_db.push_back(f);

    
    // send the content to the other synchronizers
    std::ofstream ofs(filename, std::ios_base::app);
    for(string u : content){
      if(u=="")continue;
      ofs << u +"\n";
      sync->sendNewUser(std::stoi(u));
    }
    ofs.close();
    return;
  }

  // else the file exist then get the differnce from the current file and the on db
  vector<string> content = makeUnique(getDirectoryFiles(MASTER_DIR, true));
  content = getDiffernce(f->content, content);
  // update the current file and send the differnce to other syncs
  std::ofstream ofs(filename, std::ios_base::app);
  for(string u : content){
    if(u=="")continue;
    std::cout << u << std::endl;
    if(contentExist(u,f->content) == -1){
      f->content.push_back(u);
      ofs << u + "\n";
      sync->sendNewUser(std::stoi(u));
    }
  }
  ofs.close();
}

void updateFollowingFiles(const vector<string> &fnames, Synchronizer * sync){
  //std::cout << "Updating following files....\n";
  // check if the file else make a new file
  for(string uname : fnames){
    if(uname=="")continue;
    string filename = MASTER_DIR + uname + "_following.txt";
    File * f = findFile(filename);
    if(!f){ // make a new file
      vector<string> following = getFileContent(filename);
      string tm = get_updateTime(filename);
      f = new File(filename, tm);
      f->content = following;
      file_db.push_back(f);
      for(string f : following){
        if(f=="")continue;
        sync->sendFollowRequest(std::stoi(uname), std::stoi(f));
      }
    }else{
      // compare the content of the file
      vector<string> content = getFileContent(filename);
      content = getDiffernce(f->content, content);
      for(string c : content){
        if(c=="")continue;
        f->content.push_back(c);
        sync->sendFollowRequest(std::stoi(uname), std::stoi(c));
      }
    }
  }
}

string getUsernameFromPost(const string & post){
  int i =0;
  while(post[i++] != ';'){}
  string username(post.substr(i,1));
  return username;
}

void updateTimeline(const vector<string> unames, Synchronizer* sync){
  //std::cout << "Updating Timeline...\n";
  // get the content from out.txt
  vector<string> content = getFileContent(MASTER_DIR +"out.txt");
  std::ofstream ofs(MASTER_DIR + "out.txt");
  ofs.close();
  for(string post : content){
    if(post=="")continue;
    string u = getUsernameFromPost(post);
    // get the followers file and send it to all of them
    vector<string> followers = getFileContent(MASTER_DIR +u + "_followers.txt");
    for(string f : followers){
      if(f=="")continue;
      sync->sendTimelineMsg(std::stoi(f), post);
    }
  }
}

void fsynchWorker(Synchronizer * fsync){
  // here run a detached thread that checks the file manipulation
  while(fsync){ // while the program is going
    std::cout << "Cycle running\n";
    // get all the files in the direcotry
    vector<string> fnames = getDirectoryFiles(MASTER_DIR, true);

    // get the info from the all users and get the unique values
    updateAllUsers(fsync);

    // get the update from the following txt files and follow the people
    updateFollowingFiles(fnames, fsync);

    // update the timeline if any
    updateTimeline(fnames, fsync);

    //sleep
    sleep(10);
  }
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

  // make the run server into a thread an detach it
  std::thread server(RunServer, host +":"+port);
  server.detach();

  myf = new Synchronizer(host + ":" + port, std::stoi(id)); 
  if(!myf->reachCoordinator(cip, cp)){
      std::cerr << "Synchronizer: Coordinator Unreachable\n";
      std::exit(EXIT_FAILURE);
  } 

  // contact the other coordinators
  int count;
  if((count = myf->createSynchronizers()) < 0){
    std::cerr << "Error: Reacheing other synchronizers\n";
  }
  std::cout << "Synchronizers reached: " << count << std::endl;

  if(myf->contactSynchronizers() < 0){
    std::cerr << "Error: Contacting Sychronizers\n";
  }

  // // make a thread that checks files and calls the other synchronizers
  // // std thread synchworker().detach()
  // std::thread worker(fsynchworker, myf);
  // worker.detach();
  fsynchWorker(myf);
  // std::thread worker(fsynchWorker, myf);
  // worker.detach();

  // run the server
  //RunServer(host + ":" + port);
  return 0;
}