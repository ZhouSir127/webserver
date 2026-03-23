#include "config.h"

Config::Config(int argc, char*argv[])
:PORT(9006), LOGWrite(0), TRIGMode(0),sql_num(8), thread_num(8), close_log(0){
    int opt;
    const char *str = "p:l:m:s:t:c:";
    while ((opt = getopt(argc, argv, str)) != -1)
        switch (opt){
            case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
            case 'l':
            {
                LOGWrite = atoi(optarg);
                break;
            }
            case 'm':
            {
                TRIGMode = atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            default:
            {
                break;
            }
        }
}
