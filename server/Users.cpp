#include "Users.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>
#include <exception>
#include <algorithm>
namespace ygo
{
Users::Users()
{
    LoadDB();


    t1 = std::thread(SaveThread,this);
}
bool Users::validLoginString(std::wstring loginString)
{
    try
    {
        auto pa = splitLoginString(loginString);
        return true;
    }
    catch(std::exception& ex)
    {
        return false;
    }
}
void Users::SaveThread(Users* that)
{
    while(1)
    {
        sleep(20);
        that->usersMutex.lock();
        time_t tempo1,tempo2;
        time(&tempo1);
        that->SaveDB();
        time(&tempo2);
        int delta = tempo2-tempo1;
        that->usersMutex.unlock();
        printf("salvato il DB, ha impiegato %d secondi\n",delta);

    }
}

std::pair<std::wstring,std::wstring> Users::splitLoginString(std::wstring loginString)
{
    //printf("splitto %s\n",loginString.c_str());
    std::wstring username;
    std::wstring password;

    auto found=loginString.find(L'|');
    if(found != std::wstring::npos)
        throw std::exception();

    found=loginString.find(L'$');
    if(found == std::wstring::npos)
        username = loginString;
    else
    {
        username = loginString.substr(0,found);
        password = loginString.substr(found+1,std::wstring::npos);
    }

    //std::wstring legal="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%@![]{}\\/*:.,&-_à‘Ô?Ë+^";
    //found = username.find_first_not_of(legal);
    //if(found != std::wstring::npos)
    //    throw std::exception();

    //if (username.length()<2 || username.length() > 20)
    //    throw std::exception();

    return std::pair<std::wstring,std::wstring> (username,password);
}




std::wstring Users::login(std::wstring loginString)
{
    std::wstring username;
    std::wstring password;
    try
    {
        std::pair<std::wstring,std::wstring> userpass = splitLoginString(loginString);
        username = userpass.first;
        password = userpass.second;
    }
    catch(std::exception& ex)
    {
        username = L"Player";
        password = L"";
    }

    std::lock_guard<std::mutex> guard(usersMutex);

    return login(username,password);
}

std::wstring Users::login(std::wstring username, std::wstring password)
{
    //std::cout<<"Tento il login con "<<username<<" e "<<password<<std::endl;
    std::wstring usernamel=username;

    std::transform(usernamel.begin(), usernamel.end(), usernamel.begin(), ::tolower);
    //std::cout<<"Tento il login con "<<usernamel<<" e "<<password<<std::endl;
    if(users.find(usernamel) == users.end())
    {
        std::cout<<usernamel<<std::endl;
        users[usernamel] = new UserData(username,password);
        time(&users[usernamel]->last_login);
        return username;
    }

    UserData* d = users[usernamel];
    if(usernamel == L"duelista" || usernamel == L"player" || d->password==L"" || d->password==password )
    {
        d->password = password;
        time(&users[usernamel]->last_login);
        return users[usernamel]->username;
    }

    username = getFirstAvailableUsername(username);
    return login(username,password);

}

int Users::getScore(std::wstring username)
{
    std::transform(username.begin(), username.end(), username.begin(), ::tolower);
    std::lock_guard<std::mutex> guard(usersMutex);
    //printf("%s ha %d punti\n",username.c_str(),users[username]->score);
    return users[username]->score;
}


void Users::Victory(std::wstring win, std::wstring los)
{
    std::transform(win.begin(), win.end(), win.begin(), ::tolower);
    std::transform(los.begin(), los.end(), los.begin(), ::tolower);


    std::lock_guard<std::mutex> guard(usersMutex);
    int winscore = users[win]->score;
    int losescore = users[los]->score;
    users[win]->score = winscore + 100*losescore/winscore;
    users[los]->score = losescore - 100*losescore/winscore;
    if(users[los]->score < 1000)
        users[los]->score = 1000;
    //std::cout << win << "ha: "<<users[win]->score;
}
void Users::Victory(std::wstring win1, std::wstring win2,std::wstring los1, std::wstring los2)
{
    std::transform(win1.begin(), win1.end(), win1.begin(), ::tolower);
    std::transform(los1.begin(), los1.end(), los1.begin(), ::tolower);
    std::transform(win2.begin(), win2.end(), win2.begin(), ::tolower);
    std::transform(los2.begin(), los2.end(), los2.begin(), ::tolower);
    std::lock_guard<std::mutex> guard(usersMutex);
    int win1score = users[win1]->score;
    int lose1score = users[los1]->score;
    int win2score = users[win2]->score;
    int lose2score = users[los2]->score;

    int delta = 200*(lose1score+lose2score)/(win1score+win2score);

    users[win1]->score += delta * win1score/(win1score+win2score);
    users[win2]->score += delta * win2score/(win1score+win2score);
    users[los1]->score -= delta * lose1score/(lose1score+lose2score);
    users[los2]->score -= delta * lose2score/(lose1score+lose2score);
    if(users[los1]->score < 1000)
        users[los1]->score = 1000;

    if(users[los2]->score < 1000)
        users[los2]->score = 1000;

}




Users* Users::getInstance()
{
    static Users u;
    return &u;
}
std::wstring Users::getFirstAvailableUsername(std::wstring base)
{
    /*base = base.substr(0,17);
    for(int i = 1; i < 1000; i++)
    {
        std::ostringstream ostr;
        ostr << i;

        std::wstring possibleUsername = base+ostr.str();
        if(users.find(possibleUsername) == users.end())
        {
            return possibleUsername;
        }
    }*/
    return L"Player";
}

void Users::SaveDB()
{
    //return;
    std::ofstream inf("users.txt");
    for(auto it = users.cbegin(); it!=users.cend(); ++it)
    {
        inf<<it->second->username<<"|"<<it->second->password<<"|"<<it->second->score;
        inf << "|"<<it->second->last_login<<std::endl;
    }
}
void Users::LoadDB()
{
    std::cout<<"LoadDB"<<std::endl;
    std::wifstream inf("users.txt");
    std::wstring username;
    while(!std::getline(inf, username, '|').eof())
    {
        std::wstring password;
        std::wstring iscore;
        std::wstring slast_login;
        unsigned int score;
        time_t last_login;
        std::getline(inf, password, '|');
        std::getline(inf, iscore,'|');
        std::getline(inf,slast_login);
        score = stoi(iscore);
        last_login = stoul(slast_login);
        //std::cout<<"adding user "<<username<<" pass: "<<password<< "score: "<<score<<std::endl;
        if(!validLoginString(username+L"$"+password))
            continue;

        splitLoginString(username+L"$"+password);
        UserData* ud = new UserData(username,password,score);
        ud->last_login = last_login;
        std::transform(username.begin(), username.end(), username.begin(), ::tolower);
        users[username] = ud;
    }

}

}

