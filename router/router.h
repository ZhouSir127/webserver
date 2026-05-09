#ifndef ROUTER_H
#define ROUTER_H
#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
#include <mavsdk/mavsdk.h>
#include <mavsdk/system.h>
#include <mavsdk/plugin_base.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mutex>
#include <atomic>
#include "../user/user.h"
#include "../args.h"

class Message;

class Router{

private:
    using HttpHandler = std::function<void(Message*)>;

    User user;
    const std::string&root;
    std::atomic<bool> isConnecting{false};
    std::shared_ptr<mavsdk::Mavsdk> mavsdkPtr;
    std::shared_ptr<mavsdk::System> drone;
    std::shared_ptr<mavsdk::Action> action;
    std::shared_ptr<mavsdk::Telemetry> telemetry;
    std::unordered_map<std::string,HttpHandler> getRoutes;
    std::unordered_map<std::string,HttpHandler> postRoutes;
    std::mutex lock;
    
    public:
    Router(const SqlInfo&,const RedisInfo&,const std::string&root);
    void route(Message* conn);
};



#endif