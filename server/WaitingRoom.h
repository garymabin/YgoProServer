#ifndef _WAITING_ROOM_H_
#define _WAITING_ROOM_H_
#include "NetServerInterface.h"
#include <list>

namespace ygo
{

class WaitingRoom:public CMNetServerInterface
{
private:
    bool handleChatCommand(DuelPlayer* dp,unsigned short* msg);
    static int minSecondsWaiting;
    static int maxSecondsWaiting;
    event* cicle_users;
    static void cicle_users_cb(evutil_socket_t fd, short events, void* arg);
    void updateObserversNum();
public:
    DuelPlayer* ExtractBestMatchPlayer(DuelPlayer*);
    WaitingRoom(RoomManager*roomManager,GameServer*);
    ~WaitingRoom();

    void ExtractPlayer(DuelPlayer* dp);
    void InsertPlayer(DuelPlayer* dp);
    void LeaveGame(DuelPlayer* dp);
    void JoinGame(char* data, DuelPlayer* dp);
    void HandleCTOSPacket(DuelPlayer* dp, char* data, unsigned int len);


};


}

#endif
