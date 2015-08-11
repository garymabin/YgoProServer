#include "netserver.h"
#include "single_duel.h"
#include "tag_duel.h"
#include "GameServer.h"
#include "RoomManager.h"
#include "debug.h"
#include "Users.h"
namespace ygo
{
CMNetServer::CMNetServer(RoomManager* roomManager, GameServer* gameServer, unsigned char mode)
    :CMNetServerInterface(roomManager,gameServer),mode(mode),duel_mode(0)
{
    HostInfo info;
    info.rule=0;
    info.mode=mode;
    info.draw_count=1;
    info.no_check_deck=false;
    info.start_hand=5;
    info.lflist=1;
    info.time_limit=300;
    info.start_lp=8000;
    info.enable_priority=false;
    info.no_shuffle_deck=false;
    createGame("random", &info);
}

CMNetServer::CMNetServer(RoomManager* roomManager, GameServer* gameServer, unsigned char mode, const char* name, HostInfo* pInfo)
    :CMNetServerInterface(roomManager,gameServer),mode(mode),duel_mode(0)
{
    createGame(name, pInfo);
}

void CMNetServer::SendPacketToPlayer(DuelPlayer* dp, unsigned char proto)
{
    CMNetServerInterface::SendPacketToPlayer(dp,proto);
    if(proto == STOC_DUEL_START)
        clientStarted();

}
void CMNetServer::SendBufferToPlayer(DuelPlayer* dp, unsigned char proto, void* buffer, size_t len)
{
    CMNetServerInterface::SendBufferToPlayer(dp,proto,buffer,len);
    if(proto == STOC_GAME_MSG)
    {
        unsigned char* wbuf = (unsigned char*)buffer;
        if(wbuf[0] == MSG_WIN)
        {
            last_winner =wbuf[1];
        }
        if(wbuf[0] == MSG_START && dp->type != NETPLAYER_TYPE_OBSERVER)
        {
            for(auto it = players.cbegin(); it!=players.cend(); ++it)
            {
                if(it->first->type == NETPLAYER_TYPE_OBSERVER)
                    continue;
                wchar_t name[20];
                wchar_t buffer[256];
                BufferIO::CopyWStr(it->first->name, name,20);
                int score = Users::getInstance()->getScore(std::wstring(name));
                swprintf(buffer, 256, L"%S 有 %d 点积分",name,score);
                SendMessageToPlayer(dp,buffer);
            }
        }
    }
}

void CMNetServer::auto_idle_cb(evutil_socket_t fd, short events, void* arg)
{
    CMNetServer* that = (CMNetServer*)arg;
    log(INFO,"auto idle_cb\n");
    if(that->state != FULL)
        return;
    for(auto it = that->players.cbegin(); it!=that->players.cend(); ++it)
    {
        if(it->first->type != NETPLAYER_TYPE_OBSERVER && !(it->second.isReady))
        {
            that->SendMessageToPlayer(it->first, L"由于长时间没有准备，您被移动到观战者");
            that->toObserver(it->first);

        }
    }
}

void CMNetServer::clientStarted()
{
    if(state==FULL)
        setState(PLAYING);
}


void CMNetServer::setState(State state)
{
    this->state=state;
}

void CMNetServer::destroyGame()
{
    if(duel_mode)
    {
        if(state != DEAD)
        {
            auto tempPlayers = players;
            for(auto it =tempPlayers.cbegin(); it!=tempPlayers.cend(); ++it)
            {
                LeaveGame(it->first);
            }
        }
        duel_mode->EndDuel();
        event_del(duel_mode->etimer);
        event_free(duel_mode->etimer);
        delete duel_mode;
        duel_mode=0;
    }
    if(auto_idle)
    {
        event_free(auto_idle);
        auto_idle=0;
    }

}

int CMNetServer::getMaxDuelPlayers()
{
    int maxplayers = 2;
    if(mode == MODE_TAG)
        maxplayers=4;
    return maxplayers;
}



void CMNetServer::playerConnected(DuelPlayer *dp)
{
    if(players.find(dp)==players.end())
        players[dp] = DuelPlayerInfo();
    numPlayers=players.size();

    log(INFO,"giocatori connessi:%d\n",numPlayers);
    updateServerState();
}

int CMNetServer::getNumDuelPlayers()
{
    int n=0;

    for(auto it = players.cbegin(); it!=players.cend(); ++it)
    {
        if(it->first->type != NETPLAYER_TYPE_OBSERVER)
            n++;
    }
    return n;
}

void CMNetServer::updateServerState()
{
    if(auto_idle)
        event_del(auto_idle);

    if(getNumDuelPlayers() < getMaxDuelPlayers() && state == FULL)
    {
        setState(WAITING);
        log(INFO,"server not full\n");
    }
    if(numPlayers==0 &&state != DEAD)//&& (state==ZOMBIE || state == WAITING))
    {
        log(INFO,"server vuoto. addio, morto\n");

        destroyGame();
        setState(DEAD);
    }
    if(getNumDuelPlayers() >= getMaxDuelPlayers() && state == WAITING)//
    {
        log(INFO,"server full\n");
        setState(FULL);
        timeval timeout = {10, 0};
        event_add(auto_idle, &timeout);
    }

}

void CMNetServer::playerDisconnected(DuelPlayer* dp )
{
    if(players.find(dp) != players.end())
        players.erase(dp);
    numPlayers = players.size();

    log(INFO,"current players:%d\n",numPlayers);
    updateServerState();
}
void CMNetServer::DuelTimer(evutil_socket_t fd, short events, void* arg)
{
    CMNetServer* that = (CMNetServer* )arg;


    if(that->state != PLAYING)
        return;

    if(that->mode == MODE_SINGLE || that->mode == MODE_MATCH)
        SingleDuel::SingleTimer(fd,events,that->duel_mode);
    else if(that->mode == MODE_TAG)
        TagDuel::TagTimer(fd,events,that->duel_mode);

}
void CMNetServer::createGame(const char* name, HostInfo *pInfo)
{
    event_base* net_evbase=roomManager->net_evbase;
    auto_idle = event_new(net_evbase, 0, EV_TIMEOUT , CMNetServer::auto_idle_cb, this);
    if(mode == MODE_SINGLE)
        duel_mode = new SingleDuel(false);
    else if(mode == MODE_MATCH)
        duel_mode = new SingleDuel(true);
    else if(mode == MODE_TAG)
        duel_mode = new TagDuel();

    duel_mode->etimer = event_new(net_evbase, 0, EV_TIMEOUT | EV_PERSIST, DuelTimer, this);

    BufferIO::CopyWStr(L"", duel_mode->name, 20);
    BufferIO::CopyWStr(L"", duel_mode->pass, 20);

    memcpy(&duel_mode->host_info, pInfo, sizeof(HostInfo));
    serverName.append(name);

    unsigned int hash = 1;
    for(auto lfit = deckManager._lfList.begin(); lfit != deckManager._lfList.end(); ++lfit)
    {
        if(duel_mode->host_info.lflist == lfit->hash)
        {
            hash = duel_mode->host_info.lflist;
            break;
        }
    }
    if(hash == 1)
        duel_mode->host_info.lflist = deckManager._lfList[0].hash;
    duel_mode->setNetServer(this);
    setState(WAITING);
    numPlayers=0;

}
void CMNetServer::DisconnectPlayer(DuelPlayer* dp)
{
    log(INFO,"DisconnectPlayer called\n");

    auto bit = players.find(dp);
    if(bit != players.end())
    {
        dp->netServer=nullptr;
        gameServer->DisconnectPlayer(dp);
    }
    playerDisconnected(dp);
    dp->netServer=0;
}

void CMNetServer::ExtractPlayer(DuelPlayer* dp)
{
    //it removes the player from the duel without disconnecting its tcp connection
    log(INFO,"ExtractPlayer called\n");
    playerDisconnected(dp);
    LeaveGame(dp);
}
void CMNetServer::InsertPlayer(DuelPlayer* dp)
{

    //it inserts forcefully the player into the server
    log(INFO,"InsertPlayer called\n");
    playerConnected(dp);
    CTOS_JoinGame csjg;
    csjg.version = PRO_VERSION;
    csjg.gameid = 0;
    BufferIO::CopyWStr("", csjg.pass, 20);
    dp->game=0;
    dp->type=0xff;
    dp->netServer = this;
    duel_mode->JoinGame(dp, &csjg, false);

    duel_mode->host_player=NULL;
}


void CMNetServer::LeaveGame(DuelPlayer* dp)
{
    unsigned char oldstate = dp->state;
    unsigned char oldtype = dp->type;

    if(state != ZOMBIE && dp->game == duel_mode)
        duel_mode->LeaveGame(dp);
    else
        DisconnectPlayer(dp);

    if(players.find(dp)!=players.end())
    {
        //this is a bug in tagduel
        log(BUG,"player left but the duel didn't call DisconnectPlayer\n");
        DisconnectPlayer(dp);
    }

    if(oldtype != NETPLAYER_TYPE_OBSERVER && state == PLAYING)
    {
        log(BUG,"player left but the game is still playing\n");
        setState(ZOMBIE);
        for(auto it=players.cbegin(); it!= players.cend(); ++it)
        {
            if(it->first != dp)
                SendPacketToPlayer(it->first, STOC_DUEL_END);
        }
    }

}

void CMNetServer::StopBroadcast()
{
}

void CMNetServer::StopListen()
{
}

void CMNetServer::Victory(unsigned char winner)
{
    DuelPlayer* _players[4];
    for(auto it = players.cbegin(); it!= players.cend(); ++it)
    {
        if(it->first->type <= NETPLAYER_TYPE_PLAYER4)
            _players[it->first->type] = it->first;
    }
    if(mode == MODE_SINGLE || mode == MODE_MATCH)
    {
        wchar_t win[20], lose[20];

        BufferIO::CopyWStr(_players[winner]->name,win,20);
        BufferIO::CopyWStr(_players[1-winner]->name,lose,20);
        //log(INFO,"SingleDuel, winner: %s, loser: %s\n",win,lose);
        std::wstring wins(win), loses(lose);
        Users::getInstance()->Victory(wins,loses);
    }

    if(mode == MODE_TAG)
    {
        wchar_t win1[20], win2[20], lose1[20],lose2[20];
        if(winner <= NETPLAYER_TYPE_PLAYER2)
        {
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER1]->name,win1,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER2]->name,win2,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER3]->name,lose1,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER4]->name,lose2,20);
        }
        else
        {
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER3]->name,win1,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER4]->name,win2,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER1]->name,lose1,20);
            BufferIO::CopyWStr(_players[NETPLAYER_TYPE_PLAYER2]->name,lose2,20);

        }
        Users::getInstance()->Victory(std::wstring(win1),std::wstring(win2),std::wstring(lose1),std::wstring(lose2));
    }
}


void CMNetServer::StopServer()
{
    //the duel asked me to stop
    log(INFO,"netserver server diventato zombie\n");
    if(state==PLAYING)
    {
        Victory(last_winner);
    }
    setState(ZOMBIE);
    updateServerState();
}



void CMNetServer::toObserver(DuelPlayer* dp)
{

    log(INFO,"to observer\n");
    duel_mode->ToObserver(dp);
    playerReadinessChange(dp,false);
    updateServerState();
}


void CMNetServer::HandleCTOSPacket(DuelPlayer* dp, char* data, unsigned int len)
{
    char* pdata = data;


    unsigned char pktType = BufferIO::ReadUInt8(pdata);


    if(state==ZOMBIE)
    {
        log(INFO,"pacchetto ricevuto per uno zombie, ignorato\n");
        return;
    }

    if( players.end() == players.find(dp))
    {
        log(INFO,"BUG: handlectospacket ha ricevuto un pacchetto per un utente inesistente \n");
        return;
    }

    if((pktType != CTOS_SURRENDER) && (pktType != CTOS_CHAT) && (dp->state == 0xff || (dp->state && dp->state != pktType)))
        return;
    switch(pktType)
    {
    case CTOS_RESPONSE:
    {
        if(!dp->game || !duel_mode->pduel)
            return;
        duel_mode->GetResponse(dp, pdata, len > 64 ? 64 : len - 1);
        break;
    }
    case CTOS_TIME_CONFIRM:
    {
        if(!dp->game || !duel_mode->pduel)
            return;
        duel_mode->TimeConfirm(dp);
        break;
    }
    case CTOS_CHAT:
    {
        if(!dp->game)
            return;
        duel_mode->Chat(dp, pdata, len - 1);
        break;
    }
    case CTOS_UPDATE_DECK:
    {
        if(!dp->game)
            return;
        duel_mode->UpdateDeck(dp, pdata);
        break;
    }
    case CTOS_HAND_RESULT:
    {
        if(!dp->game)
            return;
        CTOS_HandResult* pkt = (CTOS_HandResult*)pdata;
        dp->game->HandResult(dp, pkt->res);
        break;
    }
    case CTOS_TP_RESULT:
    {
        if(!dp->game)
            return;
        CTOS_TPResult* pkt = (CTOS_TPResult*)pdata;
        dp->game->TPResult(dp, pkt->res);
        break;
    }

    case CTOS_LEAVE_GAME:
    {
        if(!duel_mode)
            break;
        LeaveGame(dp);
        break;
    }
    case CTOS_SURRENDER:
    {
        if(!duel_mode)
            break;
        duel_mode->Surrender(dp);
        break;
    }
    case CTOS_HS_TODUELIST:
    {
        if(!duel_mode || duel_mode->pduel)
            break;
        playerReadinessChange(dp,false);
        duel_mode->ToDuelist(dp);
        updateServerState();


        /*  to avoid a bug in the XXXduel.cpp
            when a user clicks on "toduelist" his information
            about other user's readiness is not updated
        */
        for(auto it = players.cbegin(); it!=players.cend(); ++it)
        {
            if(it->first != dp && it->first->type != NETPLAYER_TYPE_OBSERVER && it->second.isReady)
            {
                STOC_HS_PlayerChange scpc;
                scpc.status = (it->first->type << 4) |PLAYERCHANGE_READY;

                SendPacketToPlayer(dp, STOC_HS_PLAYER_CHANGE, scpc);
                log(INFO,"sent player change to p\n");
            }
        }



        break;
    }
    case CTOS_HS_TOOBSERVER:
    {
        if(!duel_mode || duel_mode->pduel)
            break;
        toObserver(dp);
        break;
    }
    case CTOS_HS_READY:
    case CTOS_HS_NOTREADY:
    {
        if(!duel_mode || duel_mode->pduel)
            break;
        playerReadinessChange(dp,CTOS_HS_NOTREADY - pktType);

        duel_mode->PlayerReady(dp, CTOS_HS_NOTREADY - pktType);
        duel_mode->host_player = dp;
        duel_mode->StartDuel(dp);
        duel_mode->host_player=NULL;
        break;
    }
    case CTOS_HS_KICK:
    {
        if(!duel_mode || duel_mode->pduel)
            break;
        CTOS_Kick* pkt = (CTOS_Kick*)pdata;
        duel_mode->PlayerKick(dp, pkt->pos);
        break;
    }
    case CTOS_HS_START:
    {
        if(!duel_mode || duel_mode->pduel)
            break;

        break;
    }
    }
}

}

