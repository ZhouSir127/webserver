#include "router.h"
#include "../http_conn/http_conn.h"

Router::Router(const SqlInfo &sqlInfo)
    : user(sqlInfo), mavsdkPtr(nullptr), drone(nullptr), action(nullptr),
      telemetry(nullptr),
      getRoutes{
          {"/",
           [](HttpConn *conn) -> void {
             conn->realFilePath = conn->root + "/log.html";
           }},
          {"/register",
           [](HttpConn *conn) -> void {
             conn->realFilePath = conn->root + "/register.html";
           }},
          {"/connect",
           [&](HttpConn *) -> void {
            std::unique_lock<std::mutex>Lock(lock); 
            
            if (!mavsdkPtr) {
               mavsdk::Mavsdk::Configuration config(
                   mavsdk::ComponentType::CompanionComputer);
               mavsdkPtr = std::make_shared<mavsdk::Mavsdk>(config);
               mavsdkPtr->add_any_connection("udp://:14540");
               LOG_INFO("正在初始化 MAVSDK 并监听 udp://:14540...", nullptr);
             }
             // 2. 等待并获取无人机系统 (System)
             // 由于网络发现需要一点时间，我们加一个简易的轮询等待 (最多等 1.5
             // 秒)
             if (!drone) {
               bool found = false;
               for (int i = 0; i < 15; ++i) {
                 if (!mavsdkPtr->systems().empty()) {
                   drone =
                       mavsdkPtr->systems().at(0); // 获取发现的第一个无人机系统
                   if (drone->is_connected()) {
                     found = true;
                     break;
                   }
                 }
                 usleep(100000); // 睡 100 毫秒再查
               }

               // 3. 实例化动作和遥测插件！(最关键的一步)
               if (found && drone) {
                 action = std::make_shared<mavsdk::Action>(drone);
                 telemetry = std::make_shared<mavsdk::Telemetry>(drone);
                 LOG_INFO(
                     "成功连接到无人机系统！Action 和 Telemetry 插件已就绪。",
                     nullptr);
               } else {
                 drone = nullptr; // 没连上就清空，防止野指针
                 LOG_ERROR("连接超时：未在 udp://:14540 "
                           "发现无人机系统。请检查仿真器(SITL)是否运行。",
                           nullptr);
               }
             } else
               LOG_INFO("无人机系统早已连接，无需重复连接。", nullptr);
           }},
          {"/disconnect",
           [&](HttpConn *) -> void {
            std::unique_lock<std::mutex>Lock(lock); 
             mavsdkPtr = nullptr; 
            action = nullptr;
             telemetry = nullptr;
             drone = nullptr;
             LOG_INFO("无人机连接已断开", nullptr);
           }},
          {"/connstatus",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::System> tmpDrone;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpDrone = this->drone;
             }
             if (tmpDrone) {
               bool is_conn = tmpDrone->is_connected();
               LOG_INFO("连接状态: %d", is_conn);
             }
           }},
          {"/arm",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->arm();
           }},
          {"/disarm",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->disarm();
           }},
          {"/hold",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->hold();
           }},
          {"/takeoff",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->takeoff();
           }},
          {"/land",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->land();
           }},
          {"/rtl",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->return_to_launch();
           }},
          {"/position",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }

             if (tmpTelemetry)
               tmpTelemetry->position();
           }},
          {"/velocity",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->velocity_ned();
           }},
          {"/attitude",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->attitude_euler();
           }},
          {"/battery",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->battery();
           }},
          {"/gps",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->gps_info();
           }},

          {"/state",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->health();
           }}},
      postRoutes{
          {"/register",
           [&](HttpConn *conn) -> void {
             std::string name, password;
             std::istringstream iss(conn->requestBody);

             if (!(iss >> name))
               return;
             if (!(iss >> password))
               return;

             if (user.exists(name) == false) {
               if (user.add(name, password)) {
                 conn->realFilePath = conn->root + "/log.html";
                 LOG_INFO("User Register Success: %s", name.c_str());
               } else {
                 conn->realFilePath = conn->root + "/registerError.html";
                 LOG_ERROR("User Register Failed (DB Error): %s", name.c_str());
               }
             } else {
               conn->realFilePath = conn->root + "/registerError.html";
               LOG_ERROR("User Register Failed (User Exists): %s",
                         name.c_str());
             }
           }},
          {"/login", [&](HttpConn *conn) -> void {
             std::string name, password;
             std::istringstream iss(conn->requestBody);

             if (!(iss >> name))
               return;
             if (!(iss >> password))
               return;

             if (user.check(name, password)) {
               conn->realFilePath = conn->root + "/index.html";
               LOG_INFO("User Login Success: %s", name.c_str());
             } else {
               conn->realFilePath = conn->root + "/logError.html";
               LOG_ERROR("User Login Failed (Wrong password/No user): %s",
                         name.c_str());
             }
           }}} {}

void Router::route(HttpConn *conn) {
  if (conn->method == HttpMethod::GET) {
    auto it = getRoutes.find(conn->url);
    if (it != getRoutes.end())
      it->second(conn);
  } else if (conn->method == HttpMethod::POST) {
    auto it = postRoutes.find(conn->url);
    if (it != postRoutes.end())
      it->second(conn);
  }
}