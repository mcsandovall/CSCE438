#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include "sns.grpc.pb.h"
#include <grpc++/grpc++.h>

using csce438::Message;
using google::protobuf::Timestamp;

/**
 * This is where i will define the datastructure and parser needed for the database
*/

class User{
    public:
    
        User(std::string _un) : username(_un){
            list_followers.push_back(_un);
            following_list.push_back(_un);
        }
        User(){} // does nothing but start the array
        std::string add_follower(std::string _username){
            // check if user already following
            for(std::string follower : list_followers){
                if(follower == _username){
                    return "FAILURE_ALREADY_EXISTS";
                }
            }
            list_followers.push_back(_username);
            return "SUCCESS";
        }
        std::string remove_follower(std::string _username){
            // check if it is in the list
            for(int i = 0; i < list_followers.size(); ++i){
                if(list_followers[i] == _username){
                    // exist now erase it
                    list_followers.erase(list_followers.begin() + i);
                    return "SUCCESS";
                }
            }
            return "FAILURE_INVALID_USERNAME";
        }
        std::string follow_user(std::string _username){
            // check if user already in following list
            for(std::string followee : following_list){
                if(followee == _username){
                    return "FAILURE_ALREADY_EXISTS";
                }
            }
            
            following_list.push_back(_username);
            
            return "SUCCESS";
        }
        std::string unfollow_user(std::string _username){
            for(int i = 0; i < following_list.size();++i){
                if(following_list[i] == _username){
                    following_list.erase(following_list.begin() + i);
                    return "SUCCESS";
                }
            }
            
            return "FAILURE_INVALID_USERNAME";
        }
        void make_post(Message &_post){posts.push_back(_post);}
        void add_unseenPost(Message &_post){unseen_post.push_back(_post);}
        void set_username(std::string name){
            username = name;
            list_followers.push_back(name), following_list.push_back(name);
        }
        std::string get_username(){return username;}
        std::vector<std::string> getFollowingList(){return following_list;}
        std::vector<std::string> getListOfFollwers(){return list_followers;}
        std::vector<Message> * getPosts(){return &posts;}
        std::vector<Message> * getUnseenPosts(){return &unseen_post;}
        bool inTimeline = true;
    private:
        std::string username;
        std::vector<std::string> list_followers;
        std::vector<std::string> following_list;
        std::vector<Message> posts;
        std::vector<Message> unseen_post;
};

// function to get the content of the file
std::string getDbFileContent(std::string filename){
    std::ifstream db_file(filename);
    if(!db_file.is_open()){
        std::ofstream file(filename);
        file.close();
        return "";
    }
    
    std::stringstream db;
    db << db_file.rdbuf();
    return db.str();
}

// simple parser for the json file with all the users
class Parser{
    public:
        explicit Parser(const std::string &db) : db_(db){
            // remove all the spaces
            db_.erase(std::remove_if(db_.begin(), db_.end(), isspace), db_.end());
            if (!Match("[")) {
              SetFailedAndReturnFalse();
            }
        }
        
        bool Finished() { return current_ >= db_.size(); }
        
        bool TryParseOne(User * usr) {
            if (failed_ || Finished() || !Match("{")) {
              return SetFailedAndReturnFalse();
            }
           
            if(!Match(username_) || !Match("\"")){
                return SetFailedAndReturnFalse();
            }
            size_t name_start = current_;
            while (current_ != db_.size() && db_[current_++] != '"') {
            }
            if (current_ == db_.size()) {
              return SetFailedAndReturnFalse();
            }
            usr->set_username(std::string(db_.substr(name_start, current_ - name_start - 1)));
            // continue to parse the list with usernames and append them to the vector

            if(!Match(",") || !Match(following_list_) || !Match("[")){
                return SetFailedAndReturnFalse();
            }
            std::string followee;
            while(current_ != db_.size() && db_[current_] != ']'){
                if(!Match("\"")){
                    return SetFailedAndReturnFalse();
                }
                name_start = current_;
                while (current_ != db_.size() && db_[current_] != '"') {++current_;};
                followee = db_.substr(name_start, current_ - name_start);
                usr->follow_user(followee);
                if(!Match("\"") && !Match(",")){++current_;}
                if(db_[current_] == ']'){break;}
                ++current_;
            }
            ++current_;
            if(!Match(",") || !Match(list_followers_) || !Match("[")){
                return SetFailedAndReturnFalse();
            }
            std::string follower;
            while(current_ != db_.size() && db_[current_] != ']'){
                if(!Match("\"")){
                    return SetFailedAndReturnFalse();
                }
                name_start = current_;
                while (current_ != db_.size() && db_[current_] != '"') {++current_;};
                follower = db_.substr(name_start, current_ - name_start);
                usr->add_follower(follower);
                if(!Match("\"") && !Match(",")){++current_;}
                if(db_[current_] == ']'){break;}
                ++current_;
            }
            ++current_;
            if (!Match("},")) {
                if (db_[current_ - 1] == ']' && current_ == db_.size()) {
                    return true;
                }
                return SetFailedAndReturnFalse();
            }
            return true;
        }
        
    private:
    // helper functions
    bool SetFailedAndReturnFalse() {
        failed_ = true;
        return false;
    }
    
    bool Match(const std::string& prefix) {
        bool eq = db_.substr(current_, prefix.size()) == prefix;
        current_ += prefix.size();
        return eq;
    }
    
    bool failed_ = false;
    std::string db_;
    std::size_t current_ = 0;
    const std::string username_ = "\"username\":";
    const std::string following_list_ = "\"following_list\":";
    const std::string list_followers_ = "\"list_followers\":";
};

void ParseDB(std::string &db, std::vector<User> * _user_db){
    if(db == ""){return;}
    _user_db->clear();
    std::string db_content(db);
    db_content.erase(
    std::remove_if(db_content.begin(), db_content.end(), isspace),db_content.end());
    
    Parser parser(db_content);
    User usr;
    while (!parser.Finished()) {
        _user_db->push_back(usr);
        if (!parser.TryParseOne(&_user_db->back())) {
          std::cout << "Error parsing the db file" << std::endl;
          _user_db->clear();
          break;
        }
    }
}

void UpdateFileContent(std::vector<User> &user_db){
    if(user_db.size() == 0){return;}
    // this function will update the content of the file after the server was active
    std::ofstream db_file("user_db.json");
    if(!db_file.is_open()){
        std::cout << "Error couldnt not open database file" << std::endl;
        return;
    }
    db_file.clear();
    std::string file_content = "[";
    for (int i  = 0; i < user_db.size();++i){
        file_content += "{\n";
        for(int i = 0; i < 4; ++i){
            file_content += " ";
        }
        file_content += "\"username\" : \"" + user_db[i].get_username() + "\",\n";
        for(int i = 0; i < 4; ++i){
            file_content += " ";
        }
        file_content += "\"following_list\" : [";
        std::vector<std::string> following_list = user_db[i].getFollowingList();
        for(int i = 0; i < following_list.size(); ++i){
            file_content += "\"" + following_list[i] + "\"";
            if(i != (following_list.size()-1)){
                file_content+= ",";
            }
        }
        file_content += "],\n";
        for(int i = 0; i < 4; ++i){
            file_content += " ";
        }
        file_content += "\"list_followers\" : [";
        std::vector<std::string> list_following = user_db[i].getListOfFollwers();
        for(int i = 0; i < list_following.size(); ++i){
            file_content += "\"" + list_following[i] + "\"";
            if(i != (list_following.size()-1)){
                file_content+= ",";
            }
        }
        file_content += "]\n";
        if(i != user_db.size()-1){
            file_content += "},";
        }else{
            file_content += "}]";
        }
    }
    db_file << file_content;
    db_file.close();
}

User * findUser(std::string &username, std::vector<User> &db){ // find the user in the dabatase
    if(db.size() == 0){return nullptr;}
    
    for(int i = 0; i < db.size();++i){
        if(db[i].get_username() == username){
            return &(db[i]);
        }
    }
    return nullptr; // return nullptr user not found
}

void loadPosts(std::string &c_username, User * c_usr){
  std::ifstream ifs(c_username + ".txt");
  if(!ifs.is_open())return;
  
  // get all the post in the file
  Message msg;
  int index = 0; 
  std::string post, tm, message;
  Timestamp * timestamp;
  while(!ifs.eof()){
    std::getline(ifs,post);
    if(post.empty())continue;
    while(post[++index] != '-'){}
    
    message = post.substr(0, index-1);
    tm = post.substr(0, post.size() - index);
    index = 0; // reset the index
    
    msg.set_username(c_username);
    msg.set_msg(message);
    timestamp = new Timestamp();
    google::protobuf::util::TimeUtil::FromString(tm, timestamp);
    msg.set_allocated_timestamp(timestamp);
    
    c_usr->add_unseenPost(msg);
    msg.release_timestamp();
  }
}

// quicksort to sort out the messages from the user 
int partition(std::vector<Message> &arr, int start, int end){
    Timestamp pivot = arr[start].timestamp();
    
    int count = 0;
    for(int i = start+1; i <= end;++i){
        if(arr[i].timestamp() <= pivot){
            ++count;
        }
    }
    
    int pivotIndex = start + count;
    std::swap(arr[start], arr[pivotIndex]);
    
    int i = start, j = end;
    while(i < pivotIndex && j > pivotIndex){
        while (arr[i].timestamp() <= pivot) {
            i++;
        }
 
        while (arr[j].timestamp() > pivot) {
            j--;
        }
 
        if (i < pivotIndex && j > pivotIndex) {
            std::swap(arr[i++], arr[j--]);
        }
    }
    return pivotIndex;
}

void QuickSort(std::vector<Message> &arr, int start, int end){
    if(start >= end){
        return;
    }
    
    int p = partition(arr,start,end);
    
    QuickSort(arr,start,p-1);
    QuickSort(arr,p+1,end);
}
// get the recent post from all the followees
void getRecentPosts(User * usr){
    // get the 20 most recent post from the users following
    // them inlcuded
    
    // at most 20 post from people they follow and add it to a vector
    std::vector<Message> * all_post;
    // for(std::string followed : usr->getFollowingList()){
    //     loadPosts(followed, all_post);
    // }
    std::cout << "gets here "<< std::endl;
    // sort the vector with respect to the time
    //QuickSort(*all_post,0, all_post->size()-1);
    
    // add unseen post to the user unseen post vector, with index 20 being most recent
    usr->getUnseenPosts()->clear();
    int index = all_post->size() - 21;
    if(index < 0){index = 0;}
    
    for(index; index < all_post->size(); ++index){
        usr->add_unseenPost(all_post->at(index));
    }
}

std::vector<User> merge_vectors(std::vector<User> &current_db, std::vector<User> &global_db){
    std::vector<User> all_users;
    User * user;
    // check the global db and see if the user exist
    for(User usr :  global_db){
        std::string u_name = usr.get_username();
        if((user = findUser(u_name, current_db)) != nullptr){
            // the user exist in both, get current since its most recent
            all_users.push_back(*user);
        }else{// else the user doesnt exist in the current, thus add it from the current user
            all_users.push_back(usr);
        }
    }
    return all_users;
}
