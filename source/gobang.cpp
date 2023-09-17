#include"gobang.hpp"
#define HOST "127.0.0.1"
#define USER "root"
#define PASSWORD "123456"
#define DBNAME "gobang"
#define PORT 3306


void mysql_test(){
    gobang::user_table mysql(HOST, USER, PASSWORD, DBNAME, PORT);
    Json::Value user;
    user["username"] = "xiaohong";
    user["password"] = "111222";
    // mysql.reg(user);
    uint64_t uid = 9;
    // mysql.win(uid);
    mysql.lose(uid);
}

int main()
{
    // mysql_test();

    gobang::gobang_server _server(HOST, USER, PASSWORD, DBNAME);
    _server.start(8004);

    std::cout << "hello gobang!" << std::endl;
    return 0;
}