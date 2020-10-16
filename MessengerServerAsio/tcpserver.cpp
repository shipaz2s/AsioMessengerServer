#include "tcpserver.h"

using ts = TcpServer;

TcpServer::TcpServer(std::string port, std::string ip)
{
    context_ptr = std::make_shared<io_context>();
    resolver_ptr = std::make_shared<resolver>(*context_ptr);
    ac_ptr = std::make_shared<acceptor>(*context_ptr);
    this->ip = ip;
    this->port = port;

    start();
}

ts::~TcpServer(){
    thread_pull.join_all();
}

std::ostream& operator<< (std::ostream &out, ts::MsgForSend & msg){
    unsigned char ch[2];
    ch[0] = msg.msgSize;
    ch[1] = msg.msgSize >> 8;

    out << ch[0] << ch[1] << msg.text;
    return out;
}

std::istream& operator>> (std::istream &in, ts::MsgForSend & msg){
    unsigned char ch[2];
    in >> ch[0] >> ch[1] >> msg.text;
    msg.msgSize = (ch[0] | (ch[1] << 8));
    return in;
}

//private
//acyncs
void ts::accept_handler(const error_code &ec, socket_ptr sock){
    if (ec){

    }
    else{
         string_ptr strBuff = std::make_shared<std::string>();
         boost::asio::async_read(*sock,
                                 boost::asio::dynamic_buffer(*strBuff, sizeof (unsigned short)),
                                 std::bind(&ts::read_header_handler,
                                           this, std::placeholders::_1,
                                           sock, strBuff));
    }
    do_accept();
};

void ts::read_header_handler(const error_code & ec, socket_ptr sock, string_ptr buff){
    if (ec){
        if (ec.value() == 2){
            setUserIpNull(sock);
            sock->shutdown(boost_socket::shutdown_both);
        }
        else {
            std::cout << ec.value() << " " << ec.message() << "\n";
        }
    }
    else {
        unsigned short msgSize;
        msgSize = (unsigned short) ((unsigned char)buff->data()[0] |  (unsigned char)buff->data()[1] << 8);

        string_ptr strBuff = std::make_shared<std::string>();
        boost::asio::async_read(*sock,
                                boost::asio::dynamic_buffer(*strBuff, msgSize),
                                std::bind(&ts::read_handler,
                                          this, std::placeholders::_1,
                                          sock, strBuff));
    }
}

void ts::read_handler(const error_code & ec, socket_ptr sock, string_ptr buff){
    if (ec){
        if (ec.value() == 2){
            setUserIpNull(sock);
            sock->shutdown(boost_socket::shutdown_both);
        }
        else {
            std::cout << ec.value() << " " << ec.message() << "\n";
        }
    }
    else{

        std::vector<std::string> data;
        boost::algorithm::split(data, *buff, boost::algorithm::is_any_of(" "));

        *buff = "";
        int id;
        std::string ip;
        switch (boost::lexical_cast<int>(data[0])) {
        case 1:
            //login
            if (data.size() == 3){
                id = login(data[1], data[2], sock);
            }
            else {
                id = 0;
            }
            do_write("1 " + boost::lexical_cast<std::string>(id), buff, sock);

            break;
        case 2:
            //get friend's ip
            if (data.size() > 1){
                ip = getUserIp(boost::lexical_cast<int>(data[1]), sock);
            }
            do_write("2 " + boost::lexical_cast<std::string>(data[1]) + " " + ip, buff, sock);


            break;
        default:
            //wrong message code;
            //read again

            do_write("10 wrong_message_code", buff, sock);
            break;
        }
    }
}

void ts::write_handler(const error_code & ec, socket_ptr sock, string_ptr buff){
    if (ec){
        std::cout << ec.message() << "\n";
    }
    else{
        //do smth whith buff mb do nothing...
        *buff = "";
        boost::asio::async_read(*sock, boost::asio::dynamic_buffer(*buff, sizeof (unsigned short)),
                                std::bind(&ts::read_header_handler, this, std::placeholders::_1, sock, buff));

    }
}

void ts::do_write(std::string msg, string_ptr buff, socket_ptr sock){
    MsgForSend sendMsg(msg, msg.length());

    std::ostringstream stream;
    stream << sendMsg;
    *buff = stream.str();
    boost::asio::async_write(*sock, boost::asio::dynamic_buffer(*buff),
                            std::bind(&ts::write_handler, this, std::placeholders::_1, sock, buff));
}

void ts::do_accept(){
    socket_ptr sckt = std::make_shared<socket_ptr::element_type>(ac_ptr->get_executor());
    ac_ptr->async_accept(*sckt, std::bind(&ts::accept_handler, this, std::placeholders::_1, sckt));
    context_ptr->run();
}

//other handlers
int ts::login(const std::string & login, const std::string & password, socket_ptr sock){
    try {
        int id = 0;
        mysqlx::Session * ses = dbSessions[boost::this_thread::get_id()];

        std::string tmp = "SELECT id FROM messenger.users "
                          "WHERE login = '" + login
                          + "' AND password = sha2('" + password + "',256);";
        auto result = ses->sql(tmp).execute();

        mysqlx::Row row = result.fetchOne();
        if (!row.isNull()){
            id = boost::lexical_cast<int>(row[0]);
        }

        if (id){
            setUserIp(id, sock);
        }
        return  id;
    } catch (mysqlx::Error &error) {
        std::cout << error.what() << std::endl;
        return 0;
    }
}

void ts::setUserIpNull(socket_ptr sock){
    mysqlx::Session * sess = dbSessions[boost::this_thread::get_id()];
    //getting ip by sock->remote_endpoint().address().to_string()
    sess->sql("UPDATE messenger.users_online SET ip = 'NULL'"
              "WHERE ip = '" + sock->remote_endpoint().address().to_string() + "';").execute();
}

void ts::setUserIp(int id, socket_ptr sock){
    mysqlx::Session * sess = dbSessions[boost::this_thread::get_id()];
    //getting ip by sock->remote_endpoint().address().to_string()
    sess->sql("UPDATE messenger.users_online SET ip = "
              "'" + sock->remote_endpoint().address().to_string() + "' "
              "WHERE user_id = " + boost::lexical_cast<std::string>(id) + ";").execute();
}

std::string ts::getUserIp(int id, socket_ptr sock){
    std::string client_ip = sock->remote_endpoint().address().to_string();
    if (isUserAuthorized(client_ip)){
        return checkUserStatus(id);
    }
    else
        return "NULL";
}

bool ts::isUserAuthorized(std::string &ip){
    mysqlx::Session * sess = dbSessions[boost::this_thread::get_id()];
    auto result = sess->sql("SELECT EXISTS (SELECT ip "
              "FROM messenger.users_online "
              "WHERE ip = '"+ ip +"') isExist;").execute();
    mysqlx::Row row = result.fetchOne();
    if ( boost::lexical_cast<int>(row[0]) ){
        return true;
    }
    else {
        return false;
    }
    return false;
}

std::string ts::checkUserStatus(int id){
    mysqlx::Session * sess = dbSessions[boost::this_thread::get_id()];
    auto result = sess->sql("SELECT ip FROM messenger.users_online "
                            "WHERE user_id = " + boost::lexical_cast<std::string>(id) + ";").execute();
    mysqlx::Row row = result.fetchOne();
    if (!row.isNull()){
        return boost::lexical_cast<std::string>(row[0]);
    }
    else{
        return "NULL";
    }
}

//public

bool ts::start(){
    std::cout << "Starting server on " << ip << ":" << port << "\n";
    query resolver_query(ip, port);
    endpoint end_point = *resolver_ptr->resolve(resolver_query);
    ac_ptr = std::make_shared<acceptor>(*context_ptr);
    ac_ptr->open(end_point.protocol());
    ac_ptr->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    ac_ptr->bind( { {}, boost::lexical_cast<unsigned short>(port)} );
    ac_ptr->listen(boost::asio::socket_base::max_connections);

    context_ptr->post(boost::bind(&ts::do_accept, this));
    for (unsigned int i=0; i < boost::thread::hardware_concurrency(); ++i){
        thread_pull.create_thread(boost::bind(&ts::run_thread, this,  context_ptr ) );
    }

    std::cout << "Server started\n";

    return true;
}

bool ts::stop(){
    std::lock_guard<std::mutex> lock(main_mutex);
    std::cout << "Server stopping\n";

    ac_ptr->cancel(last_error_code);
    ac_ptr->close(last_error_code);

    context_ptr->stop();
    thread_pull.join_all();

    std::cout << "Server stoped\n";

    return true;
}

void ts::run_thread(io_context_ptr context_ptr){
    dbSessions[boost::this_thread::get_id()] = new mysqlx::Session("localhost", 33060, "pasha", "0000");
    mysqlx::Session * ses = dbSessions[boost::this_thread::get_id()];
    assert(ses);
    context_ptr->run();
}
