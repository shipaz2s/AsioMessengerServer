#include <string>
#include <iostream>
#include <map>

#include "tcpserver.h"

void controlFnc(TcpServer & server){
    std::string instruction;
    std::map<std::string, int32_t> instructions;
    instructions["exit"] = 1;
    instructions["quit"] = 1;
    instructions["start"] = 2;
    instructions["pause"] = 3;
    instructions["stop"] = 4;
    instructions["restart"] = 5;

    for (;;) {
        std::unique_lock<std::mutex> lock(main_mutex);
        std::cin >> instruction;
        main_mutex.unlock();
        switch (instructions[instruction]) {
        case 1 :
            //exit
            server.stop();
            return;
        case 2 :
            //start server
            server.start();
            break;
        case 4:
            server.stop();
            break;
        default:
            main_mutex.lock();
            std::cout << "Wrong instruction.\n";
            break;
        }
    }
}

int main()
{
    //mysqlx::Session sess1("mysqlx://pasha:0000@localhost:33060");
    //mysqlx::Schema db= sess.getSchema("messenger");
    //mysqlx::Collection myColl = db.getCollection("users");

    //std::cout << myDocs.fetchOne();

//    sess.sql("USE messenger").execute();
//    auto result = sess.sql("SELECT * from users").execute();
//    mysqlx::Row row;
//    row = result.fetchOne();
//    while (!row.isNull()){
//        std::cout << row[0] << " " << row[1] << " " << row[2] << std::endl;
//        row = result.fetchOne();
//    }


    TcpServer server("2323", "192.168.55.104");
    controlFnc(server);
    return 0;
}
