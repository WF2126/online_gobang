#ifndef _COMMON_H_
#define _COMMON_H_
#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <mutex>
#include <thread>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
using namespace std;
namespace gobang
{
    typedef websocketpp::server<websocketpp::config::asio> wsserver_t; // 用来定义websocket描述符
}
#endif // common.h