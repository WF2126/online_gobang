#ifndef _ROOM_HPP_
#define _ROOM_HPP_
#include"util.hpp"
#include"online.hpp"
#include"db.hpp"
#define BOARD_ROW 15
#define BOARD_COL 15


// 游戏状态
enum class room_statu
{
    GAME_OVER = 0,
    GAME_PLAY = 1
};
// 棋子颜色
enum class chess_color
{
    CHESS_NONE = 0,
    CHESS_WHITE = 1,
    CHESS_BLACK = 2
};
namespace gobang{
    // 房间类 
    class room{
    private:
        size_t _player_count;// 房间中玩家数量
        uint64_t _rid;// 房间id--由房间管理器分配
        uint64_t _cur_uid;// 当前轮到的下棋用户id
        uint64_t _white_uid;// 白棋用户id
        uint64_t _black_uid;// 黑棋用户id
        room_statu _rstatu;// 当前房间游戏状态
        std::vector<std::vector<chess_color>> _board; // 棋盘数据
        online_manager *_om;// 在线用户管理模块的句柄
        user_table *_ut;// 用户数据管理模块的句柄
    public:
        room(uint64_t rid, online_manager *om, user_table *ut) :
             _player_count(0),
             _rid(rid),
             _om(om),
             _ut(ut),
             _rstatu(room_statu::GAME_PLAY),
             _board(BOARD_ROW, std::vector<chess_color>(BOARD_COL, chess_color::CHESS_NONE))       
        {
            DLOG("ROOM %ld created!", rid);
        }
        ~room()
        {
            DLOG("ROOM %ld destroy!", _rid);
        }
        // 获取房间id
        uint64_t id() { return _rid; }
        // 设置白棋玩家
        void set_white(uint64_t uid)
        {
            _white_uid = uid;
            _player_count++;
        }
        // 设置黑棋玩家
        void set_black(uint64_t uid)
        {
            _cur_uid = _black_uid = uid;
            _player_count++;
        }
        // 获取白棋用户id
        uint64_t get_white() { return _white_uid; }
        // 获取黑棋用户id
        uint64_t get_black() { return _black_uid; }
        // 返回房间玩家数量
        size_t player_count() { return _player_count; }
        // 是否为五个棋子连成一条直线
        bool is_five(int row, int col, int roff, int coff, chess_color color)
        {
            int count = 1;
            int search_row = row + roff;
            int search_col = col + coff;
            while (_board[search_row][search_col] == color)
            {
                count++;
                search_row += roff;
                search_col += coff;
            }
            search_row = row - roff;
            search_col = row - coff;
            while (_board[search_row][search_col] == color)
            {
                count++;
                search_row -= roff;
                search_col -= coff;
            }
            return count >= 5;
        }
        // 判断是否胜利
        uint64_t check_win(int row, int col, chess_color color)
        {
            // 任意方向五子连珠即可获胜（横向，纵向，正斜，反斜）
            if (is_five(row, col, 0, 1, color) || 
                is_five(row, col, 1, 0, color) || 
                is_five(row, col, -1, 1, color) || 
                is_five(row, col, -1, -1, color))
                return color == chess_color::CHESS_WHITE ? _white_uid : _black_uid;
            return 0;
        }
        // 下棋处理|chess_req--请求|chess_resp--响应
        Json::Value handle_chess(Json::Value &chess_req)
        {
            Json::Value chess_resp = chess_req;
            // 1.当前下棋请求，判断是否轮到了当前用户
            if (chess_req["uid"].isNull() || chess_req["row"].isNull() || chess_req["col"].isNull())
            {
                chess_resp["result"] = false;
                chess_resp["reason"] = "走棋信息不完整";
                return chess_resp;
            }
            uint64_t uid = chess_req["uid"].asUInt64();
            if (uid != _cur_uid)
            {
                chess_resp["result"] = false;
                chess_resp["reason"] = "等待对方走棋";
                return chess_resp;
            }
            // 2.当前下棋位置请求，判断当前位置是否可以下棋
            int row_pos = chess_req["row"].asInt();
            int col_pos = chess_req["col"].asInt();
            if (_board[row_pos][col_pos] != chess_color::CHESS_NONE)
            {
                chess_resp["result"] = false;
                chess_resp["reason"] = "当前位置已有棋子";
                return chess_resp;
            }
            // 3.将borad指定行列进行置位
            chess_color cur_color = uid == _white_uid ? chess_color::CHESS_WHITE : chess_color::CHESS_BLACK;
            _board[row_pos][col_pos] = cur_color;
            _cur_uid = uid == _white_uid ? _black_uid : _white_uid;
            // 4.判断是否胜利
            uint64_t winner_uid = check_win(row_pos, col_pos, cur_color);
            chess_resp["result"] = true;
            chess_resp["winner"] = (Json::UInt64)winner_uid;
            // std::cout << winner_uid << std::endl;
            if (winner_uid != 0)
                chess_resp["reason"] = "恭喜你！胜利了";
            return chess_resp;

        }
        // 聊天处理
        Json::Value handle_chat(Json::Value &chat_req)
        {
            Json::Value chat_resp = chat_req;
            if (chat_req["message"].isNull())
            {
                chat_resp["result"] = false;
                chat_resp["reason"] = "请输入聊天消息!";
                return chat_resp;
            }
            std::string msg = chat_req["message"].asString();
            if (msg.find("辣鸡") != std::string::npos)
            {
                chat_resp["result"] = false;
                chat_resp["reason"] = "消息中存在敏感词!";
                return chat_resp;
            }
            return chat_resp;
        }
        // 退出
        void handle_exit(uint64_t uid)
        {
            // 1.不同房间状态的不同处理：游戏中，则对方获胜；游戏结束，不做特殊处理
            Json::Value json_resp;
            DLOG("%ld玩家退出游戏房间",uid);
            if (_rstatu == room_statu::GAME_PLAY)
            {
                json_resp["optype"] = "put_chess";
                json_resp["room_id"] = (Json::UInt64)_rid;
                json_resp["uid"] = (Json::UInt64)uid;
                json_resp["row"] = (Json::UInt64)-1;
                json_resp["col"] = (Json::UInt64)-1;
                json_resp["result"] = true;
                json_resp["reason"] = "对方掉线 不战而胜 嘿嘿~ ";
                json_resp["winner"] = (Json::UInt64)(uid == _white_uid ? _black_uid : _white_uid);
                uint64_t win_id = json_resp["winner"].asUInt64();
                uint64_t lose_id = win_id == _white_uid ? _black_uid : _white_uid;
                _ut->win(win_id);
                _ut->lose(lose_id);
                _rstatu = room_statu::GAME_OVER;
                broadcast(json_resp);
            }
            // 2.房间中玩家数量--
            _player_count--;
            // 3.修改房间状态
            _rstatu = room_statu::GAME_OVER;
            return;
        }
        // 返回响应
        void handle_request(Json::Value &req)
        {
            Json::Value resp = req;
            if (req["optype"].isNull() || req["room_id"].isNull())
            {
                resp["result"] = false;
                resp["reason"] = "请求信息格式错误";
                broadcast(resp);
                return;
            }
            // 1.判断当前请求是否与房间匹配
            uint64_t rid = req["room_id"].asUInt64();
            if (rid != _rid)
            {
                resp["result"] = false;
                resp["reason"] = "房间信息不匹配";
                broadcast(resp);
                return;
            }
            // 2.判断房间中是否玩家都在线
            if (_om->is_in_game_room(_white_uid) == false)
            {
                DLOG("%ld用户不在游戏房间！",_white_uid);
                return handle_exit(_white_uid);
            }
            else if (_om->is_in_game_room(_white_uid) == false)
            {
                DLOG("%ld用户不在游戏房间！",_black_uid);
                return handle_exit(_black_uid);
            }
            // 3.根据不同的optype决定调用那个处理函数，修改数据库玩家信息,得到处理结果
            if (req["optype"].asString() == "put_chess")
            {
                // DLOG("put chess!");
                resp = handle_chess(req);
                if (resp["winner"].asUInt64() != 0)
                {
                    uint64_t wid = resp["winner"].asUInt64();
                    uint64_t lid = wid == _white_uid ? _black_uid : _white_uid;
                    _ut->win(wid);
                    _ut->lose(lid);
                    _rstatu = room_statu::GAME_OVER;
                }
            }
            else if (req["optype"].asString() == "put_chat")
            {
                resp = handle_chat(req);
            }
            // 4.广播处理结果给所有玩家
            broadcast(resp);
        }
        // 处理完毕之后，将相应信息广播给房间所有玩家
        void broadcast(Json::Value &resp)
        {
            std::string resp_str;
            json_util::serialize(resp, &resp_str);
            // DLOG("%s", resp_str.c_str());
            wsserver_t::connection_ptr w_conn = _om->get_conn_from_room(_white_uid);
            if (w_conn.get() != nullptr)
            {
                w_conn->send(resp_str);
            }
            wsserver_t::connection_ptr b_conn = _om->get_conn_from_room(_black_uid);
            if (b_conn.get() != nullptr)
            {
                b_conn->send(resp_str);
            }
            return;
        }
    };

    using room_ptr = std::shared_ptr<room>;
    // 房间管理类
    class room_manager
    {
    private:
        uint64_t _next_rid; // 房间分配id
        std::mutex _mutex;
        std::unordered_map<uint64_t, room_ptr> _rooms;  // 房间id与房间信息建立映射
        std::unordered_map<uint64_t, uint64_t> _ur_map; // 用户id与房间id
        online_manager *_om;//在线用户啊管理模块句柄
        user_table *_ut;//用户数据管理模块句柄

    public:
        room_manager(online_manager *om, user_table *ut) : _next_rid(1), _om(om), _ut(ut) 
        {
            DLOG("房间管理模块初始化完毕！");
        }
        ~room_manager() 
        { 
            DLOG("房间管理模块即将销毁！"); 
        }
        // 1.创建游戏房间
        room_ptr create_room(uint64_t uid1, uint64_t uid2)  
        {
            std::unique_lock<std::mutex> lock(_mutex);
            room_ptr rp(new room(_next_rid, _om, _ut));
            rp->set_white(uid1);
            rp->set_black(uid2);
            _rooms.insert(std::make_pair(_next_rid, rp));
            _ur_map.insert(std::make_pair(uid1, _next_rid));
            _ur_map.insert(std::make_pair(uid2, _next_rid));
            _next_rid++;
            return rp;
        }
        // 2.通过房间id查找房间
        room_ptr get_room_by_rid(uint64_t rid)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _rooms.find(rid);
            if (it == _rooms.end())
            {
                return room_ptr();
            }
            return it->second;
        }
        // 3.通过用户id查找房间
        room_ptr get_room_by_uid(uint64_t uid)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _ur_map.find(uid);
            if (it == _ur_map.end()) 
            {
                return room_ptr();
            }
            // return get_room_by_rid(it->second);//不能调用上面的函数，有可能因为两次加锁导致产生死锁--还没试过，可以试一下
            uint64_t rid = it->second;
            auto its = _rooms.find(rid);
            if (its == _rooms.end())
            {
                return room_ptr();
            }
            return its->second;
        }
        // 没有玩家了，移除房间
        void remove_room(uint64_t rid)
        {
            room_ptr rp = get_room_by_rid(rid);
            std::unique_lock<std::mutex> lock(_mutex);
            _rooms.erase(rid);
            _ur_map.erase(rp->get_black());
            _ur_map.erase(rp->get_white());
        }
        // 4.房间中玩家退出房间的处理，玩家数量为0则销毁房间
        void player_exit_room(uint64_t uid)
        {
            DLOG("%ld 玩家即将退出房间", uid);
            room_ptr rp = get_room_by_uid(uid);
            if (rp.get() == nullptr)
            {
                DLOG("没有找到房间信息");
                return;
            }
            rp->handle_exit(uid);
            if (rp->player_count() == 0)
            {
                DLOG("%ld 房间玩家为0，销毁房间！",uid);
                remove_room(rp->id());
            }
        }
    };
}
#endif// room.hpp