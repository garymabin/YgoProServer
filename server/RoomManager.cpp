
#include "GameServer.h"
#include "RoomManager.h"
#include "Statistics.h"
#include "debug.h"
namespace ygo
{



void RoomManager::setGameServer(GameServer* gs)
{
    gameServer = gs;

    net_evbase = gs->net_evbase;
    timeval timeout = {5, 0};
    keepAliveEvent = event_new(net_evbase, 0, EV_TIMEOUT | EV_PERSIST, keepAlive, this);
    waitingRoom = new WaitingRoom(this,gs);
    event_add(keepAliveEvent, &timeout);
}

RoomManager::RoomManager()
{
    waitingRoom=0;
}
RoomManager::~RoomManager()
{
    event_free(keepAliveEvent);
    delete waitingRoom;
}


void RoomManager::keepAlive(evutil_socket_t fd, short events, void* arg)
{
    RoomManager*that = (RoomManager*) arg;
    that->removeDeadRooms();
    that->FillAllRooms();
}


int RoomManager::getNumPlayers()
{
    int risultato = 0;
    risultato += waitingRoom->getNumPlayers();
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        risultato += (*it)->getNumPlayers();
    }
    return risultato;
}

CMNetServer* RoomManager::getFirstAvailableRoom(unsigned char mode)
{
    int i = 0;
    log(INFO,"analizzo la lista server\n");
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;

        if(p->state == CMNetServer::State::WAITING && p->mode == mode)
        {
            log(INFO,"ho scelto il server %d\n",i);
            return *it;
        }
        i++;
    }
    log(INFO,"Server non trovato, creo uno nuovo \n");
    return createRoom(mode);
}

bool RoomManager::InsertPlayer(DuelPlayer*dp)
{

    //tfirst room
    CMNetServer* netServer = getFirstAvailableRoom();
    if(netServer == nullptr)
    {
        waitingRoom->InsertPlayer(dp);
        waitingRoom->SendMessageToPlayer(dp, L"服务器满员");
        //gameServer->DisconnectPlayer(dp);
        return false;
    }
    dp->netServer=netServer;
    netServer->InsertPlayer(dp);
    FillRoom(netServer);
    return true;
}

bool RoomManager::FillRoom(CMNetServer* room)
{

    if(room->state!= CMNetServer::State::WAITING)
        return true;

    for(DuelPlayer* base = room->getFirstPlayer(); room->state!= CMNetServer::State::FULL;)
    {
        DuelPlayer* dp = waitingRoom->ExtractBestMatchPlayer(base);
        if(dp == nullptr)
            return false;
        dp->netServer=room;
        room->InsertPlayer(dp);

    }
    return true;
}

bool RoomManager::FillAllRooms()
{
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;
        if(p->state == CMNetServer::State::WAITING )
        {
            bool result = FillRoom(p);
            if(!result)
                return false;
        }
    }
    return true;

}

bool RoomManager::InsertPlayerInWaitingRoom(DuelPlayer*dp)
{
    //true is success

    CMNetServerInterface* netServer = waitingRoom;
    if(netServer == NULL)
    {
        gameServer->DisconnectPlayer(dp);
        return false;
    }
    dp->netServer = netServer;
    return true;
}

bool RoomManager::InsertPlayer(DuelPlayer*dp,unsigned char mode)
{

    //true is success
    CMNetServer* netServer = getFirstAvailableRoom(mode);
    if(netServer == nullptr)
    {
        waitingRoom->InsertPlayer(dp);
        waitingRoom->SendMessageToPlayer(dp, L"服务器满员");
        return false;
    }

    dp->netServer=netServer;
    netServer->InsertPlayer(dp);
    FillRoom(netServer);
    return true;
}

bool RoomManager::CreateOrJoinRoom(DuelPlayer*dp, HostInfo *pInfo, const char* name)
{
    CMNetServer* room = FindRoom(pInfo, name);
    if (room == nullptr)
    {
        room = CreateRoom(pInfo, name);
    }
    dp->netServer = room;
    netServer->InsertPlayer(dp);
}

static inline bool compareHostInfo(HostInfo *pInfo1, HostInfo * pInfo2)
{
    return memcmp(pInfo1, pInfo2, sizeof(HostInfo)) == 0;
}
    

CMNetServer* RoomManager::FindRoom(HostInfo *pInfo, const char* name)
{
    int i = 0;
    log(INFO,"try to find specific room\n");
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;
        if(compareHostInfo(&p->duel_mode->host_info, pInfo) && (strcmp(name, p->serverName.c_str()) == 0))
        {
            return *it;
        }
        i++;
    } 
    return nullptr;
}

CMNetServer* RoomManager::getFirstAvailableRoom()
{
    int i = 0;
    log(INFO,"analizzo la lista server\n");
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;
        if(p->state == CMNetServer::State::WAITING)
        {
            log(INFO,"ho scelto il server %d\n",i);
            return *it;
        }
        i++;
    }
    log(INFO,"Server non trovato, creo uno nuovo \n");
    return createRoom(MODE_SINGLE);
}

CMNetServer* RoomManager::createRoom(unsigned char mode)
{
    if(elencoServer.size() >= 500)
    {
        return nullptr;
    }
    CMNetServer *netServer = new CMNetServer(this, gameServer, mode);

    elencoServer.push_back(netServer);

    Statistics::getInstance()->setNumRooms(elencoServer.size());

    return netServer;
}

CMNetServer* RoomManager::createRoom(HostInfo *pInfo, const char *name)
{
    if(elencoServer.size() >= 500)
    {
        return nullptr;
    }
    CMNetServer *netServer = new CMNetServer(this, gameServer, pInfo->mode, name, pInfo);

    elencoServer.push_back(netServer);

    Statistics::getInstance()->setNumRooms(elencoServer.size());

    return netServer;
}

void RoomManager::removeDeadRooms()
{

    int i=0;
    //log(INFO,"analizzo la lista server e cerco i morti\n");
    for(auto it =elencoServer.begin(); it!=elencoServer.end();)
    {
        CMNetServer *p = *it;

        if(p->state == CMNetServer::State::DEAD)
        {
            log(INFO,"elimino il server %d\n",i);
            delete (*it);
            it=elencoServer.erase(it);
        }
        else
        {
            ++it;
        }

        i++;
    }
    Statistics::getInstance()->setNumRooms(elencoServer.size());
}

}
