// Implementation of the coordinator 
/**
 * @author: Mario Sandoval
 * @course: CSCE 438 MP3
*/

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

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
using snsCoordinator::Request;
using snsCoordinator::Reply;
using snsCoordinator::SNSService;
using snsCoordinator::HeartBeat;
using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

// implemntation of server class to hold info
class CServer{
private:
    int sid;
    std::string port_no;
    bool active;
    ServerType type;
public:
    CServer(int id, std::string port, ServerType t) : sid(id), port_no(port), active(true), type(t) {}
    bool isActive(){return active;}
    void changeStatus(){ active = !active;}
    int getID{return sid;}
    std::string getPort(){return port_no;}
    ServerType getType(){return type;}
};

// cluster class to include both types of servers and give functionality to the coordinator
class Cluster{
private:
int cid; // cluster id
CServer *master, *slave, *synchronizer;
public:
    Cluster(int id), cid(id), master(nullptr), slave(nullptr), synchronizer(nullptr){}
    ~Cluster(){ delete master; delete slave}
    int createServer(std::string port, ServerType t){
        switch(t){
            case ServerType::MASTER:
                if(master){return -1;} // already exist 
                master = new CServer(cid, port, t);
            break;
            case ServerType::CLIENT:
                if(slave){return -1;}
                slave = new CServer(cid, port, t);
            break;
            case ServerType::SYNCHRONIZER:
                if(synchronizer){return -1;}
                synchronizer = new CServer(cid, port, t);
            break;
            default:
            return -1; // neither of those worked
        }
        return 0;
    }
    std::string getServer(){
        if(!master || !slave){ return "";} //check if the servers have been created
        
        if(master->isActive()){
            return master->getPort();
        }else if(slave->isActive()){
            return slave->getPort();
        }
        return ""; // if neither of servers are active
    }
    std::string getFollowerSynchronizer(){return synchronizer->getPort();}
};

// vector that contains the clusters depending on the id
std::map<int, Cluster> cluster_db;

class SNSCoordinatorImp final : public SNSCoordinator::Service{
    
    Status Login(ServerContext* context, const Request* request, Reply* reply){
        // log in the requester 
        
        // check the type of request then handle accordinly
        switch(request.requester()){
            case RequesterType::SERVER:
                // check the id
                int sid = request.id();
                // check the server type
                switch(request.server_type()){
                    // create a new instance of the type of server
                    case ServerType::MASTER:
                    
                    break;
                    case ServerType::SLAVE;
                    break;
                    case ServerType::SYNCHRONIZER:
                    break;
                    default:
                    reply.set_msg("REQUEST FAILED");
                }
            break;
            case RequesterType::CLIENT:
                // check the id and return the port number
                int cid = request.id();
                cid = (cid % 3) + 1;
                reply.set_msg(cluster_db[cid].getServer());
            break;
            default:
            return Status::OK;
        }
        return Status::OK;
    }
    
    Status ServerCommunicate(ServerContext* context, ServerReaderWriter<HeartBeat, HeartBeat>* stream) override{
        // create a thread each time there is a server connected to check their status
        
    }
};

void RunServer(std::string port_no){
    std::string server_address = "0.0.0.0:"+port_no;
    SNSCoordinatorImp service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Coordinator listening on " << server_address << std::endl;

    server->Wait();
}
// database for the clusters 
int main(int argc, char** argv){
    // populate the clusters
    for(int i = 1; i < 4;++i){
        cluster_db[i] = Cluster(i);
    }
    
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt) {
          case 'p':
              port = optarg;break;
          default:
          std::cerr << "Invalid Command Line Argument\n";
        }
    }
    RunServer(port);

}