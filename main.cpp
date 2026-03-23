#include "config.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    char * user = "root";
    char* passwd = "root";
    char* databasename = "qgydb";

    //命令行解析
    Config config(argc, argv);

    WebServer server(config.PORT, 
            config.listenET,bool connectET,
            user,passwd, databasename,config.sql_num,  
                config.TRIGMode,  ,  config.thread_num, 
                config.close_log);
    
    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}