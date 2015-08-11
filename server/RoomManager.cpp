
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
    keepAliveEvent = event_new(net_evbase, 0, EV_TIMEOUT | EV_PERSIST, KeepAlive, this);
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

void RoomManager::KeepAlive(evutil_socket_t fd, short events, void* arg)
{
    RoomManager*that = (RoomManager*) arg;
    that->RemoveDeadRooms();
    that->FillAllRooms();
}


int RoomManager::GetNumPlayers()
{
    int risultato = 0;
    risultato += waitingRoom->getNumPlayers();
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        risultato += (*it)->getNumPlayers();
    }
    return risultato;
}

CMNetServer* RoomManager::GetFirstAvailableRoom(unsigned char mode)
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
    return CreateRoom(mode);
}

bool RoomManager::InsertPlayer(DuelPlayer*dp)
{

    //tfirst room
    CMNetServer* netServer = GetFirstAvailableRoom();
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

    if(room->state != CMNetServer::State::WAITING)
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
    CMNetServer* netServer = GetFirstAvailableRoom(mode);
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
    room->InsertPlayer(dp);
    return true;
}

static inline bool CompareHostInfo(HostInfo *pInfo1, HostInfo * pInfo2)
{
    return pInfo1->mode == pInfo2->mode && pInfo1->rule == pInfo2->rule
     && pInfo1->start_lp == pInfo2->start_lp && pInfo1->no_check_deck == pInfo2->no_check_deck
     && pInfo1->no_shuffle_deck == pInfo2->no_shuffle_deck && pInfo1->enable_priority == pInfo2->enable_priority 
     && pInfo1->start_hand == pInfo2->start_hand && pInfo1->draw_count == pInfo2->draw_count
     && pInfo1->time_limit == pInfo2->time_limit && pInfo1->lflist == pInfo2->lflist;
}
    

CMNetServer* RoomManager::FindRoom(HostInfo *pInfo, const char* name)
{
    int i = 0;
    log(INFO,"try to find specific room\n");
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;
        if(CompareHostInfo(&p->duel_mode->host_info, pInfo) && (strcmp(name, p->serverName.c_str()) == 0))
        {
            return *it;
        }
        i++;
    } 
    return nullptr;
}

CMNetServer* RoomManager::GetFirstAvailableRoom()
{
    int i = 0;
    for(auto it =elencoServer.begin(); it!=elencoServer.end(); ++it)
    {
        CMNetServer *p = *it;
        if(p->state == CMNetServer::State::WAITING)
        {
            return *it;
        }
        i++;
    }
    return CreateRoom(MODE_SINGLE);
}

CMNetServer* RoomManager::CreateRoom(unsigned char mode)
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

CMNetServer* RoomManager::CreateRoom(HostInfo *pInfo, const char *name)
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

void RoomManager::RemoveDeadRooms()
{

    int i=0;
    for(auto it =elencoServer.begin(); it!=elencoServer.end();)
    {
        CMNetServer *p = *it;

        if(p->state == CMNetServer::State::DEAD)
        {
            log(INFO,"remove dead room %d\n",i);
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
