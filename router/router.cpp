#include "router.h"
#include "../http_conn/http_conn.h"

Router::Router(const SqlInfo &sqlInfo, const RedisInfo &redisInfo,const std::string&root)
    : user(sqlInfo, redisInfo),root(root),mavsdkPtr(nullptr), drone(nullptr),
      action(nullptr), telemetry(nullptr),
      getRoutes{
          {"/login",
           [this](Message *mess) -> void {
             mess->setFile(this->root + "/log.html");
           }},
          {"/register",
           [this](Message *conn) -> void {
             conn->setFile (this->root + "/register.html");
           }},
          {"/index",
           [this](Message *conn) -> void {
             conn->setFile (this->root + "/index.html");
           }},
          {"/connect",
           [this](Message *) -> void {
             // 1. 如果已经连接成功，直接返回
             {
               std::unique_lock<std::mutex> Lock(lock);
               if (drone && drone->is_connected()) {
                 // 这里你需要根据你的 HttpConn 逻辑返回成功 JSON
                 // 例如：conn->responseBody = "{\"status\":\"success\",
                 // \"msg\":\"Already connected\"}";
                 LOG_INFO("MAVSDK is already connected.");
                 return;
               }
             }

             // 2. 检查是否正在连接中（CAS 操作，保证线程安全）
             bool expected = false;
             if (!isConnecting.compare_exchange_strong(expected, true)) {
               LOG_WARN("Connection already in progress...");
               // 返回 JSON 告知前端正在连接
               return;
             }

             // 3. 启动后台分离线程，专职负责等待连接，不阻塞工作线程池！
             std::thread([this]() {
               {
                 std::unique_lock<std::mutex> Lock(lock);
                 if (!mavsdkPtr) {
                   mavsdk::Mavsdk::Configuration config(
                       mavsdk::ComponentType::CompanionComputer);
                   mavsdkPtr = std::make_shared<mavsdk::Mavsdk>(config);
                   mavsdkPtr->add_any_connection("udpin://0.0.0.0:14540");
                   LOG_INFO("MAVSDK initializing udpin://0.0.0.0:14540...");
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
                   LOG_INFO("Drone Action and Telemetry objects created. Found "
                            "= true");
                   found = true;
                   break;
                 }
                 usleep(
                     100000); // 只有这个孤立的后台线程在睡，线程池依然生龙活虎
               }

               if (!found) {
                 std::unique_lock<std::mutex> Lock(lock);
                 drone = nullptr;
                 LOG_ERROR("Timeout: Failed to discover drone on udp://:14540");
               }

               // 连接过程结束（无论成功失败），重置标志位
               isConnecting = false;
             }).detach(); // 分离线程，让它自己在后台跑

             // 4. 工作线程立刻向下执行，返回 202 Accepted 或 200 OK
             // 告知已触发连接流程 比如构造一段
             // JSON："{\"status\":\"connecting\", \"msg\":\"Background
             // connection started\"}"
           }},
          {"/disconnect",
           [this](Message *) -> void {
             std::unique_lock<std::mutex> Lock(lock);

             action = nullptr;
             telemetry = nullptr;

             drone = nullptr;

             mavsdkPtr = nullptr;

             LOG_INFO("无人机连接已断开且资源释放完毕");
           }},
          {"/connstatus",
           [this](Message *) -> void {
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
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction) {
               mavsdk::Action::Result res = tmpAction->arm();
               if (res != mavsdk::Action::Result::Success) {
                 LOG_WARN("无人机解锁(Arm)失败！原因代码: ",
                          static_cast<int>(res));
               } else {
                 LOG_INFO("无人机已成功解锁");
               }
             } else {
               LOG_WARN("尝试解锁失败：无人机未连接");
             }
           }},
          {"/disarm",
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->disarm();
           }},
          {"/hold",
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->hold();
           }},
          {"/takeoff",
           [this](Message *) -> void {
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
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->land();
           }},
          {"/rtl",
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Action> tmpAction;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpAction = this->action;
             }
             if (tmpAction)
               tmpAction->return_to_launch();
           }},
          {"/position",
           [this](Message *) -> void {
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
           [this](Message *) -> void {
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
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->velocity_ned();
           }},
          {"/attitude",
           [this](Message*) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->attitude_euler();
           }},
          {"/gps",
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->gps_info();
           }},

          {"/state",
           [this](Message *) -> void {
             std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
             {
               std::unique_lock<std::mutex> Lock(lock);
               tmpTelemetry = this->telemetry;
             }
             if (tmpTelemetry)
               tmpTelemetry->health();
           }}},
      postRoutes{{"/register",
                  [this](Message *conn) -> void {
                    std::string name, password;
                    std::istringstream iss(conn->getBody() );

                    if (!(iss >> name) || !(iss >> password)) {
                      conn->setFile(  this->root + "/registerError.html" );
                      LOG_ERROR("Payload 解析失败，内容: ", conn->getBody() );
                      return;
                    }
                    if (user.add(name, password)) {
                      conn->setFile(this->root + "/log.html");
                      LOG_INFO("User Register Success: ", name);
                    } else {
                      conn->setFile( this->root + "/registerError.html" );
                      LOG_ERROR("User Register Failed (User Exists): ", name);
                    }
                  }},
                 {"/login", [this](Message *conn) -> void {
                    std::string name, password;
                    std::istringstream iss(conn->getBody());

                    if (!(iss >> name) || !(iss >> password)) {
                      conn->setFile( this->root + "/logError.html" );
                      LOG_ERROR("Payload 解析失败，内容: ", conn->getBody());
                      return;
                    }

                    std::string token = user.login(name, password);

                    if (!token.empty()) {
                      conn->setToken(token);
                      conn->setFile(this->root + "/index.html");
                      LOG_INFO("User Login Success: ", name);
                    } else {
                      conn->setFile( this->root + "/logError.html" );
                      LOG_WARN("User Login Failed (Wrong password/No user):",
                               name);
                    }
                  }}} {}

void Router::route(Message *conn) {

  const std::string&url =conn->getURL();
  HttpMethod method = conn->getMethod();

  if (url == "/register" || url == "/login") {
    if (method == HttpMethod::GET) {
      std::unordered_map<std::string, HttpHandler>::const_iterator it =
          getRoutes.find(url);
      if (it != getRoutes.end())
        it->second(conn);
    } else if (method == HttpMethod::POST) {
      std::unordered_map<std::string, HttpHandler>::const_iterator it =
          postRoutes.find(url );
      if (it != postRoutes.end())
        it->second(conn);
    }
    return;
  }

  const std::string&cookie = conn -> getCookie();

  bool invalid = cookie.empty();
  std::string token;
  if (invalid == false) {
    size_t pos = cookie.find("token=");
    if (pos != std::string::npos && (pos == 0 || cookie[pos - 1] == ' ' ||
                                    cookie[pos - 1] == ';')) {
      pos += 6;
      size_t endPos = cookie.find(';', pos);
      if (endPos != std::string::npos)
        token = cookie.substr(pos, endPos - pos);
      else
        token = cookie.substr(pos);
    } else
      invalid = true;
  }

  if (invalid || user.verify(token) == false) {
    auto it = getRoutes.find("/login");
    if (it != getRoutes.end())
      it->second(conn);
    return;
  }

  if (url == "/") {
    auto it = getRoutes.find("/index");
    if (it != getRoutes.end())
      it->second(conn);
    return;
  } else {
    if (method == HttpMethod::GET) {
      auto it = getRoutes.find(url);
      if (it != getRoutes.end())
        it->second(conn);
    } else if ( method == HttpMethod::POST) {
      auto it = postRoutes.find(url);
      if (it != postRoutes.end())
        it->second(conn);
    }
  }
}