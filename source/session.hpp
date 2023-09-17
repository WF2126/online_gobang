#ifndef _SESSION_HPP_
#define _SESSION_HPP_
#include"util.hpp"


namespace gobang{
    //会话类
    class session{
    private:
        uint64_t _ssid;//会话id
        uint64_t _uid;//会话关联用户id
        wsserver_t::timer_ptr _tp;//会话关联定时器

    public:
        session(uint64_t ssid,uint64_t uid):_ssid(ssid),_uid(uid),_tp(wsserver_t::timer_ptr()){
            DLOG("USER:%ld SESSION %ld created!",uid,ssid);
        }
        ~session(){
            DLOG("USER:%ld SESSION %ld destroy!",_uid,_ssid);
        }
        uint64_t ssid() { return _ssid; }
        uint64_t uid() { return _uid; }
        void set_timer(const wsserver_t::timer_ptr &tp) { _tp = tp; }
        wsserver_t::timer_ptr get_timer() { return _tp; }
    };

    using session_ptr = std::shared_ptr<session>;
    #define SESSION_FOREVER -1
    #define SESSION_TIMEOUT 30000
    // 会话管理类
    class session_manager{
    private:
        uint64_t _next_ssid;// 会话id分配计数器
        std::mutex _mutex;// 保证_next_ssid操作线程安全
        std::unordered_map<uint64_t, session_ptr> _ss;// 通过ssid查找会话
        wsserver_t *_server;

    public:
        session_manager(wsserver_t *server):_next_ssid(1),_server(server){
            DLOG("session管理器初始化完毕！");
        }
        ~session_manager(){
            DLOG("session管理器即将销毁！");
        }
        // 创建会话 
        session_ptr create_session(uint64_t uid){
            std::unique_lock<std::mutex> lock(_mutex);
            session_ptr sp(new session(_next_ssid, uid));
            _ss.insert(std::make_pair(_next_ssid, sp));
            _next_ssid++;
            return sp;
        }
        // 添加一个已存在的session会话到_ss中
        void append_session(const session_ptr sp){
            std::unique_lock<std::mutex> lock(_mutex);
            _ss.insert(std::make_pair(sp->ssid(), sp));
        }
        // 查找会话（会话id）   
        session_ptr get_session_by_ssid(uint64_t ssid){
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _ss.find(ssid);
            if(it == _ss.end()){
                return session_ptr();
            }
            return it->second;
        }
        // 删除会话（并不会直接调用，而是通过定时任务调用）
        void remove_session(uint64_t ssid){
            std::unique_lock<std::mutex> lock(_mutex);
            _ss.erase(ssid);
        }
        // 设置会话超时时间
        void set_session_expire_time(uint64_t ssid,size_t ms_delay){
            // 首先通过ssid获取session信息
            session_ptr sp = get_session_by_ssid(ssid);
            wsserver_t::timer_ptr tp = sp->get_timer();
            // 1.session永久存在转永久存在
            if(tp.get() == nullptr && ms_delay == SESSION_FOREVER){
                return;
            }
            // 2.session永久存在转临时存在
            else if(tp.get() == nullptr && ms_delay != SESSION_FOREVER){
                tp = _server->set_timer(ms_delay, std::bind(&session_manager::remove_session, this, sp->ssid()));
                sp->set_timer(tp);
                DLOG("%ld 用户session转为临时存在！", sp->uid());
            }
            // 3.session临时存在转永久存在
            else if(tp.get() != nullptr && ms_delay == SESSION_FOREVER){
                // wsserver_t::timer_ptr tp = ssp->get_timer();
                tp->cancel();// 取消当前的定时任务导致任务立即被执行（其实并不能被立即执行哈哈）
                _server->set_timer(0, std::bind(&session_manager::append_session, this, sp));
                sp->set_timer(wsserver_t::timer_ptr());
                DLOG("%ld 用户session转为永久存在！", sp->uid());
            }
            // 4.session临时存在，延迟过期时间
            else if(tp.get() != nullptr && ms_delay != SESSION_FOREVER){
                // wsserver_t::timer_ptr tp = ssp->get_timer();
                tp->cancel(); 
                sp->set_timer(wsserver_t::timer_ptr());
                _server->set_timer(0, std::bind(&session_manager::append_session, this, sp));
                tp = _server->set_timer(ms_delay, std::bind(&session_manager::remove_session, this, sp->ssid()));
                sp->set_timer(tp);
                DLOG("%ld 用户session延迟生命周期！", sp->uid());
            }
        }
    };
}
#endif//session.hpp