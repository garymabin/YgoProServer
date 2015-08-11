
#ifndef ROOMMANAGER_H
#define ROOMMANAGER_H
#include <list>
#include <mutex>
#include "WaitingRoom.h"
namespace ygo {

class GameServer;
class RoomManager
{
private:
    event* keepAliveEvent;

    WaitingRoom* waitingRoom;
    void RemoveDeadRooms();
    bool FillRoom(CMNetServer* room);
    bool FillAllRooms();
    CMNetServer* CreateRoom(unsigned char mode);
    CMNetServer* CreateRoom(HostInfo *pInfo, const char* name);
    CMNetServer* FindRoom(HostInfo *pInfo, const char* name);
    static void KeepAlive(evutil_socket_t fd, short events, void* arg);
public:
    event_base* net_evbase;
    std::list<CMNetServer *> elencoServer;
    GameServer* gameServer;
    void setGameServer(ygo::GameServer*);

public:
    RoomManager();
    ~RoomManager();
    bool InsertPlayerInWaitingRoom(DuelPlayer*dp);
    bool InsertPlayer(DuelPlayer*dp);
    bool InsertPlayer(DuelPlayer*dp,unsigned char mode);
    bool CreateOrJoinRoom(DuelPlayer*dp, HostInfo *pInfo, const char* name);
    CMNetServer* GetFirstAvailableRoom();
    CMNetServer* GetFirstAvailableRoom(unsigned char mode);
    int GetNumPlayers();
};

}
#endif
