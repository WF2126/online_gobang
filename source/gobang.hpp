#ifndef _GOBANG_HPP_
#define _GOBANG_HPP_
#include"matcher.hpp"


namespace gobang{
    // 网络服务器模块
    class gobang_server{
    private:
        std::string _webroot;
        wsserver_t _server;// web服务器操作符
        user_table _ut;
        online_manager _om;
        room_manager _rm;
        session_manager _sm;
        matcher_manager _mm;
    public:
        gobang_server(const std::string& host,const std::string& user,
                      const std::string& pass,const std::string& dbname,
                      uint16_t sql_port = 3306,const std::string& webroot = "./webroot"):
                      _webroot(webroot),
                      _ut(host,user,pass,dbname,sql_port),
                      _rm(&_om,&_ut),_sm(&_server),_mm(&_rm,&_om,&_ut)                     
        {
            // 设置日志等级
            _server.set_access_channels(websocketpp::log::alevel::none);
            // 初始化asio调度器
            _server.init_asio();
            _server.set_reuse_addr(true);
            // 设置回调函数
            _server.set_open_handler(std::bind(&gobang_server::onopen, this, std::placeholders::_1));
            _server.set_close_handler(std::bind(&gobang_server::onclose, this, std::placeholders::_1));
            _server.set_message_handler(std::bind(&gobang_server::onmessage, this, std::placeholders::_1,std::placeholders::_2));
            _server.set_http_handler(std::bind(&gobang_server::onhttp, this, std::placeholders::_1));
        }
        void start(int srv_port = 80){
            // 设置监听端口
            _server.listen(srv_port);
            // 获取新建连接
            _server.start_accept();
            // 启动服务器
            _server.run();
        }

        // 通过Cookie获取会话信息
        session_ptr get_session_by_cookie(wsserver_t::connection_ptr& conn){
            // 获取Cookie头部字段
            std::string cookie_str = conn->get_request_header("Cookie");
            if(cookie_str.empty()){
                return session_ptr();
            }
            // Cookie: SSID=xxx; path=/; max-age=xxx
            // 解析Cookie内容，查找SSID
            std::vector<std::string> cookie_arry;
            str_util::split(cookie_str, "; ", &cookie_arry);
            std::string ssid_str;
            for(auto str : cookie_arry){
                std::vector<std::string> sub_arry;
                str_util::split(str, "=", &sub_arry);
                if(sub_arry.size() != 2)
                    continue;
                if (sub_arry[0] == "SSID")
                    ssid_str = sub_arry[1];
            }
            if(ssid_str.empty())
                return session_ptr();
            uint64_t ssid = std::stol(ssid_str);
            // 通过ssid查找会话信息
            session_ptr sp = _sm.get_session_by_ssid(ssid);
            if(sp.get() == nullptr)
                return session_ptr();
            return sp;
        }
        // 错误信息处理
        void handler_error(wsserver_t::connection_ptr& conn,websocketpp::http::status_code::value coder,const std::string& err){
            Json::Value resp_json;
            std::string resp_body;
            conn->set_status(coder);
            resp_json["result"] = false;
            resp_json["reason"] = err;
            json_util::serialize(resp_json, &resp_body);
            conn->set_body(resp_body);
            conn->append_header("Content-Type", "application/json");
        }
        // 用户注册请求处理函数
        void reg(wsserver_t::connection_ptr& conn){
            // 1.获取请求正文，拿到用户信息，进行反序列化
            std::string req_body = conn->get_request_body();
            Json::Value req_json;
            Json::Value resp_json;
            bool ret = json_util::unserialize(req_body, &req_json);
            if(ret == false){
                //bad_request客户端请求的语法错误，服务器无法理解
                return handler_error(conn, websocketpp::http::status_code::bad_request,"请求信息格式错误！");
            }
            if(req_json["username"].isNull() || req_json["password"].isNull()){
                return handler_error(conn, websocketpp::http::status_code::bad_request,"用户名密码不能为空！");
            }
            // 2.向数据库插入用户信息
            ret = _ut.reg(req_json);
            if(ret == false){
                return handler_error(conn, websocketpp::http::status_code::bad_request, "用户名已经被占用！");
            }
            resp_json["result"] = true;
            std::string resp_body;
            json_util::serialize(resp_json, &resp_body);
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(resp_body);
            conn->append_header("Content-Type", "application/json");
        }
        // 登录验证请求处理函数
        void login(wsserver_t::connection_ptr& conn){
            // 1.获取请求正文，进行反序列化，得到用户名密码
            std::string req_body = conn->get_request_body();
            Json::Value req_json;
            Json::Value resp_json;
            bool ret = json_util::unserialize(req_body, &req_json);
            if(ret == false){
                return handler_error(conn, websocketpp::http::status_code::bad_request, "登录信息格式错误！");
            }
            if(req_json["username"].isNull() || req_json["password"].isNull()){
                return handler_error(conn, websocketpp::http::status_code::bad_request, "用户名密码不能为空！");
            }
            // 2.通过数据管理模块进行登陆验证，并获取详细用户信息
            ret = _ut.login(req_json);
            if(ret == false){
                return handler_error(conn, websocketpp::http::status_code::bad_request, "用户名或密码错误！");
            }
            // 3.为客户端创建session
            session_ptr sp = _sm.create_session(req_json["id"].asUInt64());
            if(sp.get() == nullptr){
                return handler_error(conn, websocketpp::http::status_code::bad_request, "会话创建失败！");
            }
            _sm.set_session_expire_time(sp->ssid(), SESSION_TIMEOUT);
            // 4.进行响应
            resp_json["result"] = true;
            std::string resp_body;
            json_util::serialize(resp_json, &resp_body);
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(resp_body);
            std::string cookie = "SSID=" + std::to_string(sp->ssid());;//设置cookie,存储session的id
            conn->append_header("Content-Type","application/json");
            conn->append_header("Set-Cookie", cookie);  
        }
        // 用户信息获取请求处理函数
        void userinfo(wsserver_t::connection_ptr &conn)
        {
            //1.获取Cookie信息，在Cookie中找到会话id，通过会话id找会话
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr)
            {
                return handler_error(conn, websocketpp::http::status_code::forbidden, "获取用户会话失败");
            }
            // 2.从数据库获取用户详细信息
            Json::Value user_info;
            bool ret = _ut.get_user_by_uid(sp->uid(),&user_info);
            if(ret == false)
            {
                return handler_error(conn, websocketpp::http::status_code::forbidden, "获取用户信息失败");
            }
            // 3.进行序列化响应
            std::string resp_body;
            json_util::serialize(user_info, &resp_body);
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(resp_body);
            conn->append_header("Content-Type", "application/json");
            // 4.延迟会话生命周期
            _sm.set_session_expire_time(sp->ssid(), SESSION_TIMEOUT);
        }
        // 静态资源文件请求处理函数
        void handle_file(wsserver_t::connection_ptr &conn)
        {
            websocketpp::http::parser::request req = conn->get_request();
            std::string realpath = _webroot + req.get_uri();
            std::string body;
            if(realpath.back() == '/')
                realpath += "login.html";
            bool ret = file_util::read(realpath, &body);
            if(ret == false)
            {
                return handler_error(conn, websocketpp::http::status_code::not_found, "请求的资源不存在！");//not_found您所请求的资源无法找到404
            }
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(body);
        }
        // 游戏大厅长连接建立
        void hall_onopen(wsserver_t::connection_ptr conn){
            //1. 登录验证--判断当前客户端是否已经成功登录
            Json::Value resp;
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                resp["result"] = false;
                resp["reason"] = "玩家未登录!";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                DLOG("%s", body.c_str());
                return;
            }
            // 2. 判断当前客户端是否是重复登录
            if(_om.is_in_game_hall(sp->uid())){
                resp["result"] = false;
                resp["reason"] = "玩家重复登陆！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                DLOG("%s", body.c_str());
                return;
            }
            // 游戏大厅：
            // 1.玩家id与长连接映射关系加入在线用户管理模块
            _om.enter_game_hall(sp->uid(), conn);
            // 2.将当前客户端的session设置为永久存在
            _sm.set_session_expire_time(sp->ssid(), SESSION_FOREVER);
            // 3.给客户端回复hall_ready
            resp["result"] = true;
            resp["reason"] = "hall_ready!";
            std::string body;
            json_util::serialize(resp, &body);
            conn->send(body);
        }
        // 游戏大厅长连接关闭
        void hall_onclose(wsserver_t::connection_ptr conn){
            Json::Value resp;
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                return;
            }
            // 从匹配队列中移除玩家
            // _mm.remove_match(sp->uid());
            // 从在线用户管理模块移除玩家
            _om.exit_game_hall(sp->uid());
            // 将玩家对应session修改为临时存在
            _sm.set_session_expire_time(sp->ssid(), SESSION_TIMEOUT);
        }
        // 游戏大厅长连接信息返回
        void hall_onmessage(wsserver_t::connection_ptr conn,wsserver_t::message_ptr msg){
            // 获取会话信息
            Json::Value resp;
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                resp["result"] = false;
                resp["reason"] = "玩家未登录！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                return;
            }
            // 1.获取请求信息，进行反序列化
            std::string req_body = msg->get_payload();// 获取对有效负载字符串的引用
            Json::Value req_json;
            bool ret = json_util::unserialize(req_body, &req_json);
            if(ret == false){
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "请求信息解析失败！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                return;
            }
            // 2.根据不同请求类型进行不同的处理
            // 2.1match_start 开始匹配
            // 2.2match_stop 匹配停止
            if(req_json["optype"].isNull()){
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "请求信息格式失败！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                return;
            }
            if(req_json["optype"].asString() == "match_start"){
                _mm.append_matcher(sp->uid());
                resp["optype"] = "match_start";
                resp["result"] = true;
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
            }
            else if(req_json["optype"].asString() == "match_stop"){
                _mm.remove_match(sp->uid());
                resp["optype"] = "match_stop";
                resp["result"] = true;
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
            }
            else{
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "请求信息类型错误！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
            }
        }
        // 连接建立成功回调函数,用来识别当前连接用的
        // 游戏房间长连接建立
        void room_onopen(wsserver_t::connection_ptr conn){
            // 1.验证是否登录
            Json::Value resp;
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "玩家未登录！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                // DLOG("%s", body.c_str());
                return;
            }
            if(_om.is_in_game_room(sp->uid()) || _om.is_in_game_hall(sp->uid())){
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "玩家重复登录！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                // DLOG("%s", body.c_str());
                return;
            }
            // 2.验证是否有对应房间
            room_ptr rp = _rm.get_room_by_uid(sp->uid());
            if(rp.get() == nullptr){
                resp["optype"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "玩家没有游戏房间！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                return;
            }
            // 将玩家与长连接映射关系加入到在线用户管理
            _om.enter_game_room(sp->uid(), conn);
            // 3.将session转为永久存在
            _sm.set_session_expire_time(sp->ssid(), SESSION_FOREVER);
            // 4.向玩家响应房间准备完毕信息
            resp["optype"] = "room_ready";
            resp["room_id"] = (Json::UInt64)rp->id();
            resp["self_id"] = (Json::UInt64)sp->uid();
            resp["white_id"] = (Json::UInt64)rp->get_white();
            resp["black_id"] = (Json::UInt64)rp->get_black(); 
            std::string body;
            json_util::serialize(resp,&body);
            conn->send(body);
        }
        // 游戏房间长连接关闭
        void room_onclose(wsserver_t::connection_ptr conn){
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                return;
            }
            _om.exit_game_room(sp->uid());
            _rm.player_exit_room(sp->uid());
            _sm.set_session_expire_time(sp->ssid(), SESSION_TIMEOUT);
        }
        // 游戏房间长连接信息返回
        void room_onmessage(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg){
            // 获取用户会话信息
            Json::Value resp;
            session_ptr sp = get_session_by_cookie(conn);
            if(sp.get() == nullptr){
                resp["result"] = false;
                resp["reason"] = "玩家未登录！";
                std::string body;
                json_util::serialize(resp, &body);
                conn->send(body);
                // DLOG("%s", body.c_str());
                return;
            }
            // 获取房间信息
            room_ptr rp = _rm.get_room_by_uid(sp->uid());
            if(rp.get() == nullptr){
                resp["result"] = "unknow";
                resp["result"] = false;
                resp["reason"] = "玩家没有对应的房间！";
                std::string body;
                json_util::serialize(resp,&body);
                conn->send(body);
                DLOG("%s", body.c_str());
                return;
            }
            std::string req_body = msg->get_payload();
            Json::Value req_json;
            json_util::unserialize(req_body, &req_json);
            rp->handle_request(req_json);
        }
        // 连接建立回调函数
        void onopen(websocketpp::connection_hdl hdl){
            // 区分长连接类型：游戏大厅/游戏房间
            wsserver_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall"){
                hall_onopen(conn);
            }
            else if(uri == "/room"){
                room_onopen(conn);
            }
        }
        // 连接关闭回调函数
        void onclose(websocketpp::connection_hdl hdl){
            wsserver_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall"){
                hall_onclose(conn);
            }
            else if(uri == "/room"){
                room_onclose(conn);
            }
        }
        // 连接信息回调函数
        void onmessage(websocketpp::connection_hdl hdl,wsserver_t::message_ptr msg){
            wsserver_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if(uri == "/hall"){
                hall_onmessage(conn, msg);
            }
            else if(uri == "/room"){
                room_onmessage(conn, msg);
            }
        }
        // 调用http回调函数
        void onhttp(websocketpp::connection_hdl hdl){
            // connection_hdl用于唯一标识连接的句柄
            wsserver_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
            // 获取请求对象，直接访问请求对象
            websocketpp::http::parser::request req = conn->get_request();
            // 返回请求方法
            std::string method = req.get_method();
            // 获取连接uri
            std::string uri = req.get_uri();
            if(method == "POST"){
                if(uri == "/reg")
                    return reg(conn);
                else if(uri == "/login")
                    return login(conn);
            }
            else if(method == "GET"){
                if(uri == "/userinfo")
                    return userinfo(conn);
                handle_file(conn);
            }
            else{
                return conn->set_status(websocketpp::http::status_code::method_not_allowed);//客户端请求中的方法被禁止405
            }
        }
    };
}
#endif// gobang.hpp