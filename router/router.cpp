#include "router.h"
#include "../http_conn/http_conn.h"

Router::Router(const SqlInfo &sqlInfo,const RedisInfo &redisInfo)
    : user(sqlInfo,redisInfo), mavsdkPtr(nullptr), drone(nullptr), action(nullptr),
      telemetry(nullptr),
      getRoutes{
          {"/login",
           [](HttpConn *conn) -> void {
             conn->realFilePath = conn->root + "/log.html";
           }},
          {"/register",
           [](HttpConn *conn) -> void {
             conn->realFilePath = conn->root + "/register.html";
           }},
           {"/index",
            [](HttpConn *conn) -> void {
             conn->realFilePath = conn->root + "/index.html";
           }},
          {"/connect",
           [&](HttpConn *) -> void {
             {
               std::unique_lock<std::mutex> Lock(lock);
               if (!mavsdkPtr) {
                 mavsdk::Mavsdk::Configuration config(mavsdk::ComponentType::CompanionComputer);
                 mavsdkPtr = std::make_shared<mavsdk::Mavsdk>(config);
                 mavsdkPtr->add_any_connection("udpin://0.0.0.0:14580");
                 LOG_INFO("正在初始化 MAVSDK 并监听 udpin://0.0.0.0:14580...");
               }
             }

             bool found = false;
             for (int i = 0; i < 15; ++i) {
               std::shared_ptr<mavsdk::System> tempDrone;
               {
                 std::unique_lock<std::mutex> Lock(lock);
                 if (mavsdkPtr && !mavsdkPtr->systems().empty()) {
                   tempDrone = mavsdkPtr->systems().at(0);
                 }
               }

               if (tempDrone && tempDrone->is_connected()) {
                 std::unique_lock<std::mutex> Lock(lock);
                 drone = tempDrone;
                 action = std::make_shared<mavsdk::Action>(drone);
                 telemetry = std::make_shared<mavsdk::Telemetry>(drone);
                 LOG_INFO("成功连接到无人机系统！Action 和 Telemetry 插件已就绪。");
                 found = true;
                 break;
               }
               usleep(100000);
             }

             if (!found) {
               std::unique_lock<std::mutex> Lock(lock);
               drone = nullptr;
               LOG_ERROR("连接超时：未在 udp://:14540 发现无人机系统。");
             }
           }},
          {"/disconnect",
           [&](HttpConn *) -> void {
             std::unique_lock<std::mutex>Lock(lock);

             action = nullptr;
             telemetry = nullptr;

             drone = nullptr;

             mavsdkPtr = nullptr;

             LOG_INFO("无人机连接已断开且资源释放完毕");
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
               LOG_INFO("连接状态: ", is_conn);
             }
           }},
          {"/arm",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction) {
                 mavsdk::Action::Result res = tmpAction->arm();
                 if (res != mavsdk::Action::Result::Success) {
                     LOG_WARN("无人机解锁(Arm)失败！原因代码: ", static_cast<int>(res));
                 } else {
                     LOG_INFO("无人机已成功解锁");
                 }
             } else {
                 LOG_WARN("尝试解锁失败：无人机未连接");
             }
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

             if (tmpAction) {
                 mavsdk::Action::Result res = tmpAction->takeoff();

                 if (res != mavsdk::Action::Result::Success) {
                     LOG_WARN("无人机起飞失败！原因代码: ", static_cast<int>(res));
                 } else {
                     LOG_INFO("无人机起飞命令发送成功");
                 }
             } else {
                 LOG_WARN("尝试起飞失败：无人机尚未连接或 Action 插件未就绪。");
             }
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

             if (tmpTelemetry) {
                 mavsdk::Telemetry::Position pos = tmpTelemetry->position();

                 LOG_INFO("获取位置成功 - 纬度: ", pos.latitude_deg,
                          ", 经度: ", pos.longitude_deg,
                          ", 绝对高度: ", pos.absolute_altitude_m, "m");

             } else {
                 LOG_WARN("获取位置失败：无人机未连接");
             }
           }},
          {"/battery",
           [&](HttpConn *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry) {
                 mavsdk::Telemetry::Battery bat = tmpTelemetry->battery();
                 LOG_INFO("电池状态 - 剩余电量: ", bat.remaining_percent * 100,
                          "%, 电压: ", bat.voltage_v, "V");
             }
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

             if (!(iss >> name) || !(iss >> password)){
                conn->realFilePath = conn->root + "/registerError.html";
                LOG_ERROR("Payload 解析失败，内容: ", conn->requestBody);
                return;
             }
             if (user.add(name, password) ){
                 conn->realFilePath = conn->root + "/log.html";
                 LOG_INFO("User Register Success: ", name);
             } else {
               conn->realFilePath = conn->root + "/registerError.html";
               LOG_ERROR("User Register Failed (User Exists): ",name);
             }
           }},
          {"/login", [&](HttpConn *conn) -> void {
             std::string name, password;
             std::istringstream iss(conn->requestBody);

             if (!(iss >> name) || !(iss >> password)){
                conn->realFilePath = conn->root + "/logError.html";
                LOG_ERROR("Payload 解析失败，内容: ",conn->requestBody);
                return;
             }

             conn->token = user.login(name, password);

             if (!conn->token.empty()) {
               conn->realFilePath = conn->root + "/index.html";
               LOG_INFO("User Login Success: ", name);
             } else {
               conn->realFilePath = conn->root + "/logError.html";
               LOG_WARN("User Login Failed (Wrong password/No user):",
                         name);
             }
           }}} {}

void Router::route(HttpConn *conn){

  if(conn->url == "/register" || conn->url == "/login"){
    if (conn->method == HttpMethod::GET) {
      std::unordered_map<std::string, HttpHandler>::const_iterator it =  getRoutes.find(conn->url);
      if (it != getRoutes.end())
        it->second(conn);
    } else if (conn->method == HttpMethod::POST) {
      std::unordered_map<std::string, HttpHandler>::const_iterator it = postRoutes.find(conn->url);
      if (it != postRoutes.end())
        it->second(conn);
    }
    return;
  }

  bool invalid = conn->cookie.empty();
  std::string token;
  if(invalid == false){
      size_t pos = conn->cookie.find("token=");
      if (pos != std::string::npos && (pos == 0 || conn->cookie[pos - 1] == ' ' || conn->cookie[pos - 1] == ';') ){
        pos += 6;
        size_t endPos = conn->cookie.find(';',pos);
        if (endPos != std::string::npos)
            token = conn->cookie.substr(pos, endPos - pos);
        else
            token = conn->cookie.substr(pos);
      }else
        invalid = true;
  }

  if(invalid || user.verify(token) == false ){
    auto it = getRoutes.find("/login");
    if (it != getRoutes.end())
      it->second(conn);
    return;
  }

  if(conn->url == "/"){
    auto it = getRoutes.find("/index");
    if (it != getRoutes.end() )
      it->second(conn);
    return;
  }else{
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
}