/*
    项目中使用到但是并没有归类的一些子模块
    mysql实用类：创建并初始化句柄，销毁句柄，执行sql语句
    json实用类：json序列化和反序列化
    file实用类：文件数据的读取
    string实用类：字符串切割
*/
#ifndef _UTIL_HPP_
#define _UTIL_HPP_
#include "common.hpp"
#include "log.hpp"

namespace gobang
{
    // mysql实用类：创建并初始化句柄，销毁句柄，执行sql语句
    class mysql_util
    {
    public:
        // 创建mysql句柄，并进行初始化
        static MYSQL *mysql_create(const std::string &host, const std::string &user,
                                   const std::string &pass, const std::string &dbname,
                                   uint16_t sql_port = 3306)
        {
            // 初始化句柄
            MYSQL *mysql = mysql_init(nullptr);
            if (mysql == nullptr)
            {
                ELOG("create mysql error\n");
                return nullptr;
            }
            // 连接服务器
            if (mysql_real_connect(mysql, host.c_str(), user.c_str(), pass.c_str(), dbname.c_str(), sql_port, nullptr, 0) == nullptr)
            {
                ELOG("connect mysql server failed: %s\n", mysql_error(mysql));
                mysql_close(mysql);
                return nullptr;
            }
            // 设置客户端字符集
            if (mysql_set_character_set(mysql, "utf8") != 0)
            {
                ELOG("set mysql client character failed: %s\n", mysql_error(mysql));
                mysql_close(mysql);
                return nullptr;
            }
            // 选择数据库mysql_select_db(mysql,"gobang");
            return mysql;
        }

        // mysql句柄销毁
        static void mysql_destroy(MYSQL *mysql)
        {
            if (mysql)
                mysql_close(mysql);
            return;
        }

        // 执行sql语句
        static bool mysql_exec(MYSQL *mysql, const std::string &sql)
        {
            int ret = mysql_query(mysql, sql.c_str());
            if (ret != 0)
            {
                ELOG("sql: %s query error: %s\n", sql.c_str(), mysql_error(mysql));
                return false;
            }
            return true;
        }
    };

    // json实用类：json序列化和反序列化
    class json_util
    {
    public:
        // 序列化
        static bool serialize(const Json::Value &root, std::string *out)
        {
            // 构建StreamWriter
            Json::StreamWriterBuilder swb;
            swb["indentation"] = "";
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
            std::stringstream ss;
            // 将Json::Value对象root有格式的输出到输出流对象ss中
            int ret = sw->write(root, &ss);
            if (ret != 0)
            {
                ELOG("json serialize failed!");
                return false;
            }
            // 将stringstream流对象ss中的数据交给string类对象out
            *out = ss.str();
            return true;
        }

        // 反序列化
        static bool unserialize(const std::string &out, Json::Value *root)
        {
            // 构建CharReader
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
            std::string err;
            // 将字符串out中的数据解析到Json::Value对象root中去
            bool ret = cr->parse(out.c_str(), out.c_str() + out.size(), root, &err);
            if (ret == false)
            {
                ELOG("json unserialize failed: %s", err.c_str());
                return false;
            }
            return true;
        }
    };

    // file实用类：文件数据的读取
    class file_util
    {
    public:
        static bool read(const std::string &file_name, std::string *body)
        {
            std::ifstream ifs(file_name, std::ios::binary | std::ios::in);
            if (ifs.is_open() == false)
            {
                ELOG("open file: %s failed\n", file_name.c_str());
                return false;
            }
            ifs.seekg(0, std::ios::end);
            size_t flen = ifs.tellg(); // 获取当前位置相对于文件起始位置偏移量
            ifs.seekg(0, std::ios::beg);
            body->resize(flen);
            ifs.read(&(*body)[0], flen);
            if (ifs.good() == false)
            {
                ELOG("read file conect failed\n");
                ifs.close();
                return false;
            }
            ifs.close();
            return true;
        }
    };

    // string实用类：字符串切割
    class str_util
    {
    public:
        // src--原始字符串   sep--分隔符     arry--目标对象
        static size_t split(const std::string &src, const std::string &sep, std::vector<std::string> *arry)
        {
            size_t pos, start = 0;
            while (start < src.size())
            {
                pos = src.find(sep, start);
                if (pos == std::string::npos)// 判断查找是否结束
                {
                    arry->push_back(src.substr(start));
                    break;
                }
                if (pos == start)// 判断是否为空子串
                {
                    start = pos + sep.size();
                    continue;
                }
                arry->push_back(src.substr(start, pos - start));
                start = pos + sep.size();
            }
            return arry->size();
        }
    };
}
#endif // util.hpp