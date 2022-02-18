#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

/**
 * This is where i will define the datastructure and parser needed for the database
*/

enum Status
{
    SUCCESS,
    FAILURE_ALREADY_EXISTS,
    FAILURE_NOT_EXISTS,
    FAILURE_INVALID_USERNAME,
    FAILURE_INVALID,
    FAILURE_UNKNOWN
};

std::vector<User> user_db;

class User{
    public:
        enum Status add_follower(std::string _username){
            // check if user already following
            for(std::string follower : list_followers){
                if(follower == _username){
                    return FAILURE_ALREADY_EXISTS;
                }
            }
            // check if the user exist
            for(User usr: user_db){
                if(usr.username == _username){
                    // exist, add it to the list of followers
                    list_followers.push_back(_username);
                    return SUCCESS;
                }
            }
 
            return FAILURE_INVALID_USERNAME;
        }
        enum Status remove_follower(std::string _username){
            // check if it is in the list
            for(int i = 0; i < list_followers.size(); ++i){
                if(list_followers[i] == _username){
                    // exist now erase it
                    list_followers.erase(list_followers.begin() + i);
                    return SUCCESS;
                }
            }
            return FAILURE_INVALID_USERNAME;
        }
        enum Status follow_user(std::string _username){
            // check if user already in following list
            for(std::string followee : following_list){
                if(followee == _username){
                    return FAILURE_ALREADY_EXISTS;
                }
            }
            
            // chec if user exist
            for(User usr :  user_db){
                if(usr.username == _username){
                    following_list.push_back(_username);
                    usr.add_follower(this->username);
                    return SUCCESS;
                }
            }
            
            return FAILURE_NOT_EXISTS;
        }
        enum Status unfollow_user(std::string _username){
            // look for the user in the db
            for(int i = 0; i < user_db.size(); ++i){
                if(user_db[i].username == _username){
                    // exist remove it from your following list and remove youself from thier following list
                    following_list.erase(following_list.begin() + i);
                    user_db[i].remove_follower(this->username);
                    return SUCCESS;
                }
            }
            return FAILURE_INVALID_USERNAME;
        }
    private:
        std::string username;
        std::vector<std::string> list_followers;
        std::vector<std::string> following_list;
};

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
            if (!Match(location_) || !Match("{") || !Match(latitude_)) {
              return SetFailedAndReturnFalse();
            }
            size_t name_start = current_;
            while (current_ != db_.size() && db_[current_++] != '"') {
            }
            if (current_ == db_.size()) {
              return SetFailedAndReturnFalse();
            }
            usr->username = std::string(db_.substr(name_start, current_ - name_start - 1));
            // continue to parse the list with usernames and append them to the vector
            
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
    
    void ReadLong(long* l) {
        std::size_t start = current_;
        while (current_ != db_.size() && db_[current_] != ',' && db_[current_] != '}') {
          current_++;
        }
        // It will throw an exception if fails.
        *l = std::stol(db_.substr(start, current_ - start));
    }
    
    bool failed_ = false;
    std::string db_;
    std::size_t current_ = 0;
    const std::string username_ = "\"username\":";
    const std::string following_list = "\"following_list\":";
    const std::string list_followers_ = "\"list_followers\":";
};

