#pragma once
#ifndef TCPSERVER_H
#define TCPSERVER_H

//boost
#include <boost/asio.hpp>
#include <boost/thread.hpp>
//#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

//std
#include <memory>
#include <iostream>
#include <string>
#include <fstream>
#include <mutex>
#include <cassert>

#include <mysql-cppconn-8/mysqlx/xdevapi.h>

static std::mutex main_mutex;

class TcpServer
{

typedef boost::asio::io_context io_context;
typedef std::shared_ptr<boost::asio::io_context> io_context_ptr;
typedef boost::asio::ip::tcp::endpoint endpoint;
typedef boost::asio::ip::tcp::acceptor acceptor;
typedef std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
typedef boost::asio::ip::tcp::resolver resolver;
typedef boost::asio::ip::tcp::resolver::query query;
typedef std::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;
typedef boost::asio::ip::tcp::socket boost_socket;
typedef boost::system::error_code error_code;
typedef boost::thread_group thread_group;
typedef std::shared_ptr<std::string> string_ptr;


public:

    std::string port;
    std::string ip;

protected:
    error_code last_error_code;
    io_context_ptr context_ptr;
    acceptor_ptr ac_ptr;
    thread_group thread_pull;
    std::shared_ptr<resolver> resolver_ptr;

    std::map<boost::thread::id, mysqlx::Session *> dbSessions;

public:
    struct MsgForSend {
        unsigned short msgSize;
        std::string text;

        MsgForSend(std::string txt = "", unsigned short size = 0):
            msgSize(size), text(txt)
        {
        }

        void operator() (std::string txt, unsigned short size){
            text = txt;
            msgSize = size;
        }

        friend std::ostream& operator<< (std::ostream &out, MsgForSend & msg);
        friend std::istream& operator>> (std::istream &in, MsgForSend & msg);

    };

public:
    TcpServer(std::string port = "2323", std::string ip = "127.0.0.1");
    ~TcpServer();

    bool start();
    bool stop();

private:
    void do_accept();
    void accept_handler(const error_code & ec, socket_ptr sock);
    void read_handler(const error_code & ec, socket_ptr sock, string_ptr buff);
    void read_header_handler(const error_code & ec, socket_ptr sock, string_ptr buff);
    void write_handler(const error_code & ec, socket_ptr sock, string_ptr buff);

    void do_write(std::string msg, string_ptr buff, socket_ptr sock);

    void setUserIp(int id, socket_ptr sock);
    void setUserIpNull(socket_ptr sock);
    int login(const std::string &login, const std::string &password, socket_ptr sock);
    std::string getUserIp(int id, socket_ptr sock);
    bool isUserAuthorized(std::string & ip);
    std::string checkUserStatus(int id);

    void run_thread(io_context_ptr context_ptr);
};

#endif // TCPSERVER_H
