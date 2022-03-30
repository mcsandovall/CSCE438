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
#include <map>

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
    Cluster(int id), cid(id){}
    ~Cluster(){ delete master; delete slave}
    int createServer(std::string port, ServerType t){
        switch(t){
            case ServerType::MASTER:
            break;
            case ServerType::CLIENT:
            break;
            case ServerType::SYNCHRONIZER:
            break;
            default:
            return -1; // neither of those worked
        }
    }
};

class SNSCoordinatorImp final : public SNSCoordinator::Service{
    
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
    return 0;
}