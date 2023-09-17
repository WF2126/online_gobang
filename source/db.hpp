#ifndef _DB_HPP_
#define _DB_HPP_
#include"util.hpp"


// 封装数据库表的访问操作--针对每一张表设计一个类，来管理表中数据的访问
// 用户信息：注册，登录，信息获取，胜利，失败
namespace gobang{
    // 用户信息表操作
    class user_table{
    private:
        MYSQL *_mysql_hdl;

    public: 
        user_table(const std::string& host,const std::string& user,
                   const std::string& pass,const std::string& dbname,
                   uint16_t sql_port = 3306) : _mysql_hdl(nullptr)
        {
            _mysql_hdl = gobang::mysql_util::mysql_create(host, user, pass, dbname, sql_port);
            assert(_mysql_hdl);
        }
        ~user_table() {
            gobang::mysql_util::mysql_destroy(_mysql_hdl);
            _mysql_hdl = nullptr;
        }
        // 新增用户：用户名 密码 分数--100 比赛场次--0 胜利场次--0
        bool reg(const Json::Value& user){
            if(user["username"].isNull() || user["password"].isNull()){
                ELOG("user register data error!\n");
                return false;
            }
            char buf[4096];
            sprintf(buf, "insert into user values(null,'%s',password('%s'),100,0,0);", user["username"].asCString(), user["password"].asCString());
            return gobang::mysql_util::mysql_exec(_mysql_hdl, buf);
        }
        // 用户名密码验证并获取详细信息
        bool login(Json::Value& user){
            if(user["username"].isNull() || user["password"].isNull()){
                ELOG("user login data error!\n");
                return false;
            }

            char buf[4096];
            sprintf(buf, "select id,score,total_count,win_count from user where username='%s' and password=password('%s');", user["username"].asCString(), user["password"].asCString());
            bool ret = gobang::mysql_util::mysql_exec(_mysql_hdl, buf);
            if(ret == false){
                return false;
            }
            MYSQL_RES *res = mysql_store_result(_mysql_hdl);
            if(res == nullptr){
                ELOG("login: store result error: %s", mysql_errno(_mysql_hdl));
                return false;
            }
            int num = mysql_num_rows(res);
            if(num != 1){
                std::cout << num << std::endl;
                ELOG("login: not only have one query result!");
                mysql_free_result(res);
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(res);
            user["id"] = (Json::UInt64)std::stol(row[0]);
            user["score"] = (Json::UInt64)std::stol(row[1]);
            user["total_count"] = (Json::UInt64)std::stol(row[2]);
            user["win_count"] = (Json::UInt64)std::stol(row[3]);
            mysql_free_result(res);
            return true;
        }
        // 通过用户id获取用户信息
        bool get_user_by_uid(uint64_t uid,Json::Value *user){
            char buf[4096];
            sprintf(buf, "select username,score,total_count,win_count from user where id=%d;", uid);
            bool ret = gobang::mysql_util::mysql_exec(_mysql_hdl, buf);
            if(ret == false){
                return false;
            }

            MYSQL_RES *res = mysql_store_result(_mysql_hdl);
            if(res == nullptr){
                ELOG("get user: store result error: %s", mysql_errno(_mysql_hdl));
                return false;
            }
            int num = mysql_num_rows(res);
            if(num != 1){
                std::cout << num << std::endl;
                ELOG("get user: not only have one query result!");
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            (*user)["id"] = (Json::UInt64)uid;
            (*user)["username"] = row[0];
            (*user)["score"] = (Json::UInt64)std::stol(row[1]);
            (*user)["total_count"] = (Json::UInt64)std::stol(row[2]);
            (*user)["win_count"] = (Json::UInt64)std::stol(row[3]);
            mysql_free_result(res);
            return true;
        }
        // 获胜：分数+10 比赛场次+1 胜利场次+1
        bool win(uint64_t uid){
            char buf[4096];
            sprintf(buf, "update user set score=score+10,total_count=total_count+1,win_count=win_count+1 where id=%d;", uid);
            return gobang::mysql_util::mysql_exec(_mysql_hdl, buf);
        }
        // 失败：分数-10 比赛场次+1 胜利场次不变
        bool lose(uint64_t uid){
            char buf[4096];
            sprintf(buf, "update user set score=score-10,total_count=total_count+1 where id=%d;", uid);
            return gobang::mysql_util::mysql_exec(_mysql_hdl, buf);
        }
    };
}
#endif//db.hpp