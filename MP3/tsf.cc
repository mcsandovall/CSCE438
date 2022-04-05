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
#include <thread>
#include <chrono>

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

// implentation for the follower synchronizer

class Synchronizer{
public:
    Synchronizer(const std::string &p, const int &_id) : port(p), id(_id){}
    int reachCoordinator(const std::string &cip,const std::string &cp);
private:
    int id;
    std::string port;
    std::unique_ptr<SNSCoordinator::Stub> cstub;
};

int Synchronizer::reachCoordinator(const std::string &cip, const std::string &cp){
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
    mys.reachCoordinator(cip, cp);
}