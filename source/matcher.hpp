#ifndef _MATCHER_HPP
#define _MATCHER_HPP
#include"util.hpp"
#include"online.hpp"
#include"room.hpp"
#include"session.hpp"
#define MAX_MATCH_QUEUE 100


namespace gobang{
    // 匹配队列
    class match_queue{
    private:
        std::list<uint64_t> _q;// 匹配队列，可能会删除中间数据，所以使用链表
        std::mutex _mutex;
        std::condition_variable _cond;// 使用条件变量实现阻塞，队列中元素个数小于2的时候进行阻塞
    public:
        match_queue() {}
        // 判断阻塞队列中玩家数量是否大于2
        bool match_cond() {
            return _q.size() >= 2;
        }
        // 入队
        void Push(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            _q.push_back(uid);
            // 每次有玩家进入匹配队列后唤醒进程
            _cond.notify_all();
        }
        // 出队
        void Pop(uint64_t *uid1, uint64_t *uid2) {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, std::bind(&match_queue::match_cond,this));
            *uid1 = _q.front();
            _q.pop_front();
            *uid2 = _q.front();
            _q.pop_front();
        }
        // 移除指定元素
        void Erase(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            _q.remove(uid);
        }
    };

    // 匹配管理模块
    class matcher_manager
    {
    private:
        match_queue _q_bronze;  // 青铜阻塞队列
        match_queue _q_diamond; // 钻石
        match_queue _q_king;    // 王者
        std::thread _t_bronze;  // 青铜线程描述符
        std::thread _t_diamond; // 钻石
        std::thread _t_king;    // 王者
        room_manager *_rm;
        online_manager *_om;
        user_table *_ut;
    public:
        matcher_manager(room_manager *rm, online_manager *om, user_table *ut) : 
            _rm(rm), _om(om), _ut(ut), 
            _t_bronze(std::thread(&matcher_manager::handler_match,this,&_q_bronze)),
            _t_diamond(std::thread(&matcher_manager::handler_match,this,&_q_diamond)),
            _t_king(std::thread(&matcher_manager::handler_match,this,&_q_king)) {}
        // 判断队列中元素是否可以进行匹配
        bool handler_match(match_queue *queue){
            while(1){
                // 失败--陷入休眠
                // 成功--则出队两个数据--玩家id
                uint64_t uid1, uid2;
                queue->Pop(&uid1,&uid2);
                // 判断两个玩家是否都在线（任意一个不在线，就需要把另一个重新加入队列）
                wsserver_t::connection_ptr conn1, conn2;
                conn1 = _om->get_conn_from_hall(uid1);
                if (conn1.get() == nullptr)
                { // uid1掉线了，把uid2加入阻塞队列
                    append_matcher(uid2);
                    DLOG("uid:%d玩家下线了，重新匹配",uid1);
                    continue;
                }
                conn2 = _om->get_conn_from_hall(uid2);
                if(conn2.get() == nullptr){
                    append_matcher(uid1);
                    DLOG("uid:%d玩家下线了，重新匹配",uid2);
                    continue;
                }
                // 为两个玩家创建游戏房间
                room_ptr rp = _rm->create_room(uid1, uid2);
                if(rp.get() == nullptr){
                    append_matcher(uid1);
                    append_matcher(uid2);
                    DLOG("创建房间失败，重新开始匹配");
                    continue;
                }
                // 给两个玩家发送匹配成功响应
                DLOG("%d--%d匹配成功",uid1,uid2);
                Json::Value resp;
                resp["optype"] = "match_success";
                resp["result"] = true;
                std::string body;
                json_util::serialize(resp, &body);
                conn1->send(body);
                conn2->send(body);
            }
        }
        // 添加用户到阻塞队列中
        bool append_matcher(uint64_t uid){
            // 获取用户信息
            Json::Value user_json;
            bool ret = _ut->get_user_by_uid(uid, &user_json);
            if (ret == false){
                DLOG("没找到用户信息：%ld", uid);
                return false;
            }
            int score = user_json["score"].asInt();
            // 根据不同分数加入到不同的阻塞队列
            if (score < 200){
                _q_bronze.Push(uid);
            }
            else if (score >= 200 && score < 500){
                _q_diamond.Push(uid);
            }
            else{
                _q_king.Push(uid);
            }
            return true;
        }
        // 从匹配队列中移除移除玩家
        void remove_match(uint64_t uid){
            _q_bronze.Erase(uid);
            _q_diamond.Erase(uid);
            _q_king.Erase(uid);
        }
    };
}
#endif//matcher.hpp