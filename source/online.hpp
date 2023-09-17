#ifndef _ONLINE_HPP
#define _ONLINE_HPP
#include"util.hpp"


namespace gobang{
    // 在线用户管理类
    class online_manager{
    private:
        std::mutex _mutex;
        std::unordered_map<uint64_t, wsserver_t::connection_ptr> _game_hall;// 大厅用户id与长连接关系
        std::unordered_map<uint64_t, wsserver_t::connection_ptr> _game_room;// 房间用户id与长连接关系

    public:
        // 1.添加大厅长连接到管理器中
        void enter_game_hall(uint64_t uid, wsserver_t::connection_ptr &conn){
            std::unique_lock<std::mutex> lock(_mutex);
            _game_hall.insert(std::make_pair(uid, conn));
            DLOG("%ld 用户进入游戏大厅！", uid);
        }
        // 2.添加房间长连接到管理器中
        void enter_game_room(uint64_t uid,wsserver_t::connection_ptr& conn){
            std::unique_lock<std::mutex> lock(_mutex);
            _game_room.insert(std::make_pair(uid, conn));
            DLOG("%ld 用户进入游戏房间！", uid);
        }
        // 3.从管理器中移除大厅长连接
        void exit_game_hall(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            _game_hall.erase(uid);
            DLOG("%ld 用户退出游戏大厅！", uid);
        }
        // 4.从管理器中移除房间长连接
        void exit_game_room(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            _game_room.erase(uid);
            DLOG("%ld 用户退出游戏房间！", uid);
        }
        // 5.判断用户是否建立大厅长连接
        bool is_in_game_hall(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _game_hall.find(uid);
            if(it == _game_hall.end())
                return false;
            return true;
        }
        // 6.判断用户是否建立房间长连接
        bool is_in_game_room(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _game_room.find(uid);
            if(it == _game_room.end())
                return false;
            return true;
        }
        // 7.获取用户大厅长连接
        wsserver_t::connection_ptr get_conn_from_hall(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            // 不能调用上面的is_in_game_hall接口，会重复加锁，导致死锁
            auto it = _game_hall.find(uid);
            if(it == _game_hall.end())
                return wsserver_t::connection_ptr();
            return it->second;
        }
        // 8.获取用户房间长连接
        wsserver_t::connection_ptr get_conn_from_room(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            // 不能调用上面的is_in_game_room接口，会重复加锁，导致死锁
            auto it = _game_room.find(uid);
            if(it == _game_room.end())
                return wsserver_t::connection_ptr();
            return it->second;
        }
    };
}
#endif// online.hpp