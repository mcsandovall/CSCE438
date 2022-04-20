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

using std::string;
using std::cout;
using std::endl;

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
using snsCoordinator::HeartBeat;
using snsCoordinator::ServerType;
using snsCoordinator::RequesterType;
using snsCoordinator::SNSCoordinator;

class Cluster{
private:
int cid; // cluster id
string master_port, slave_port, synchronizer_port;
public:
    Cluster() : cid(0), master_port(""), slave_port(""), synchronizer_port(""){}
    Cluster(const int &id) : cid(id), master_port(""), slave_port(""), synchronizer_port(""){}
    ~Cluster(){}
    int assignPort(string port, ServerType t){
        // the port is assgined upon availibility 1 success 0 fail
        switch (t)
        {
        case ServerType::MASTER:
            if(master_port != ""){return 0;}
            master_port = port;
            break;
        case ServerType::SLAVE:
            if(slave_port != ""){return 0;}
            slave_port = port;
            break;
        case ServerType::SYNCHRONIZER:
            if(synchronizer_port != ""){return 0;}
            synchronizer_port = port;
            break;
        default:
            break;
        }
        return 1;
    }
    string getServer(){
        // check if the server exist
        if(master_port == "" && slave_port == ""){return "";}
        if(master_port != ""){return master_port;}
        else return slave_port;
    }
    string getMaster(){
        return ((master_port == "") ? "" : master_port);
    }
    string getSlave(){
        return ((slave_port == "") ? "" : slave_port);
    }
    string getSynchronizer(){
        return ((synchronizer_port == "") ? "" : synchronizer_port);
    }

    void changeServerStatus(ServerType t){
        // change the status of the server
        if(t == ServerType::MASTER){
            // if the server is the master change the slave to master and set slave to null
            master_port = slave_port;
            slave_port.clear();
        }else{
            // erase the slave port 
            slave_port.erase();
        }
    }
};

// vector that contains the clusters depending on the id
std::map<int, Cluster> cluster_db;

class SNSCoordinatorImp final : public SNSCoordinator::Service{
    Status Login(ServerContext* context, const Request* request, Reply* reply) override{
        // log in the server 
        reply->set_msg("SUCCESS");
        // check the id
        int sid = request->id();
        // get the type of server
        ServerType type = request->server_type();
        // get the port
        string port = request->port_number();
        switch (type)
        {
        case ServerType::MASTER:
            // check if master port is assgined
            if(!cluster_db[sid].assignPort(port, type)){
                // if assgined check slave port
                if(!cluster_db[sid].assignPort(port, ServerType::SLAVE)){
                    // return the cluster is full
                    reply->set_msg("full");
                }else{
                    // then the master is already filled, demoted
                    reply->set_msg("demoted");
                }
            }
            std::cout << "Master" << sid << " connected on port: " + cluster_db[sid].getMaster() << std::endl;
            break;
        case ServerType::SLAVE:
            // check if slave port is already used
            if(!cluster_db[sid].assignPort(port, type)){
                // then return the port is already used
                reply->set_msg("full");
            }
            std::cout << "Slave" << sid << " connected on port: " << port << std::endl;
            break;
        case ServerType::SYNCHRONIZER:
            if(!cluster_db[sid].assignPort(port, type)){
                reply->set_msg("full");
            }
            std::cout << "Synchronizer" << sid << " connected on port: " << cluster_db[sid].getSynchronizer() << std::endl;
            break;
        default:
            break;
        }
        return Status::OK;
    }

    // Request for a server
    Status ServerRequest(ServerContext* context, const Request* request, Reply* reply) override{
        // return the server ip port for client and other server for cluster
        if(request->requester() == RequesterType::SERVER){
            // get the id and the server type
            int sid = request->id();
            ServerType t = request->server_type();
            string server, message = "";
            switch (t)
            {
                case ServerType::MASTER:
                    server = cluster_db[sid].getSlave();
                    
                    if(server == "") message ="NULL";
                    else message = server;

                    reply->set_msg(message);
                    break;
                case ServerType::SLAVE:
                    // get the master and the synchronizer
                    server = cluster_db[sid].getMaster();
                    if(server == "") message = "NULL";
                    else message = server;

                    reply->set_msg(message);
                    break;
                case ServerType::SYNCHRONIZER:
                    // get the other synchronizer port
                    int synch;
                    synch = sid + 1;
                    if(synch > 3) synch = 1;
                    server = cluster_db[synch].getSynchronizer();
                    if(server != "") message = server;

                    message += "-";

                    synch = sid - 1;
                    if(synch == 0) synch = 3;
                    server = cluster_db[synch].getSynchronizer();
                    if(server != "") message += server;

                    reply->set_msg(message);
                    break;
                default:
                    break;
            }
        }else{ // client
            // check the id and return the master/ slave for that cluster
            int cid = request->id();
            cid = (cid % 3) + 1;
            reply->set_msg(cluster_db[cid].getServer());
        }
        return Status::OK;
    }

    Status ServerCommunicate(ServerContext* context, const HeartBeat* hb, Reply* reply) override{
        // send only one message form the server and get the reply
        return Status::OK;
    }
};

void RunServer(std::string port_no){
    string server_address = "0.0.0.0:"+port_no;
    SNSCoordinatorImp service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Coordinator listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv){
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
    return 0;
}