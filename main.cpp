#include<string>
#include<iostream>
#include"./webserver/webserver.h"





int main(int argc,char** argv){

    std::string dbuser = "root";
    std::string dbpasswd = "123";
    std::string dbname = "mydb";

    webserver  server;
   
    server.parse_arg(dbuser,dbpasswd,dbname,argc,argv);
    
    server.init_log();

    server.init_mysqlpoll();

    server.init_redispoll();
    
    server.init_threadpoll();

    server.init_listen();

    server.init_timer();

    server.printSetting();
    
    server.events_loop();


    return 0;
}
