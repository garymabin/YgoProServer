
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
        void removeDeadRooms();
        bool FillRoom(CMNetServer* room);
        bool FillAllRooms();
        CMNetServer* createRoom(unsigned char mode);
        CMNetServer* createRoom(HostInfo *pInfo, const char* name);
        bool CreateOrJoinRoom(DuelPlayer*dp, HostInfo *pInfo, const char* name)
        CMNetServer* FindRoom(HostInfo *pInfo, const char* name);
        static void keepAlive(evutil_socket_t fd, short events, void* arg);
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
        CMNetServer* getFirstAvailableRoom();
        CMNetServer* getFirstAvailableRoom(unsigned char mode);
        int getNumPlayers();
    };




}
#endif
