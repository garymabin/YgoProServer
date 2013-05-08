#ifndef _USERS_H_
#define _USERS_H_

#include <string>
#include "debug.h"
#include <map>
#include <ctime>
#include <mutex>
#include <thread>
namespace ygo
{

struct UserData
{
    std::wstring username;
    std::wstring password;
    unsigned int score;
    time_t  last_login;
    UserData(std::wstring username,std::wstring password):username(username),password(password),score(1000){}
UserData():username(L"Player"),password(L""),score(1000){}
    UserData(std::wstring username,std::wstring password,unsigned int score):username(username),password(password),score(score){}
};



class Users
{
    private:
    std::thread t1;
    std::pair<std::wstring,std::wstring> splitLoginString(std::wstring);
    bool validLoginString(std::wstring);
    static void SaveThread(Users*);
    std::mutex usersMutex;
    Users();
    std::map<std::wstring,UserData*> users;
    std::wstring getFirstAvailableUsername(std::wstring base);
    void LoadDB();
    void SaveDB();
    public:
    int getScore(std::wstring username);
    static Users* getInstance();
    std::wstring login(std::wstring);
    std::wstring login(std::wstring,std::wstring);
    void Victory(std::wstring, std::wstring);
    void Victory(std::wstring, std::wstring,std::wstring, std::wstring);

};

}
#endif
