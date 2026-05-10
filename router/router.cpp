#include "router.h"
#include "../http_conn/http_conn.h"

Router::Router(const SqlInfo &sqlInfo, const RedisInfo &redisInfo, const std::string& Root)
    : user(sqlInfo, redisInfo), root(std::move(Root) ), mavsdkPtr(nullptr), drone(nullptr),
      action(nullptr), telemetry(nullptr),
      routes{
          // ==================== GET 请求路由组 ====================
          { HttpMethod::GET, {
              {"/login", [this](Message *mess) -> void {
                  mess->setFile(root + "/login.html");
              }},
              {"/register", [this](Message *mess) -> void {
                  mess->setFile(root + "/register.html");
              }},
              {"/index", [this](Message *mess) -> void {
                  mess->setFile(root + "/index.html");
              }},
              {"/connect", [this](Message *) -> void {
                  // 1. 如果已经连接成功，直接返回
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      if (drone && drone->is_connected()) {
                          LOG_INFO("MAVSDK is already connected.");
                          return;
                      }
                  }
                  
                  // 2. 检查是否正在连接中（CAS 操作，保证线程安全）
                  bool expected = false;
                  if (!isConnecting.compare_exchange_strong(expected, true)) {
                      LOG_WARN("Connection already in progress...");
                      return;
                  }

                  // 3. 启动后台分离线程，专职负责等待连接，不阻塞工作线程池！
                  std::thread([this]() {
                      {
                          std::unique_lock<std::mutex> Lock(lock);
                          if (!mavsdkPtr) {
                              mavsdk::Mavsdk::Configuration config(mavsdk::ComponentType::CompanionComputer);
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
                              LOG_INFO("Drone Action and Telemetry objects created. Found = true");
                              found = true;
                              break;
                          }
                          usleep(100000); // 只有这个孤立的后台线程在睡，线程池依然生龙活虎
                      }

                      if (!found) {
                          std::unique_lock<std::mutex> Lock(lock);
                          drone = nullptr;
                          LOG_ERROR("Timeout: Failed to discover drone on udp://:14540");
                      }

                      // 连接过程结束（无论成功失败），重置标志位
                      isConnecting = false;
                  }).detach(); 
              }},
              {"/disconnect", [this](Message *) -> void {
                  std::unique_lock<std::mutex> Lock(lock);
                  action = nullptr;
                  telemetry = nullptr;
                  drone = nullptr;
                  mavsdkPtr = nullptr;
                  LOG_INFO("无人机连接已断开且资源释放完毕");
              }},
              {"/connstatus", [this](Message *) -> void {
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
              {"/arm", [this](Message *) -> void {
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
              {"/disarm", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Action> tmpAction;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpAction = this->action;
                  }
                  if (tmpAction) tmpAction->disarm();
              }},
              {"/hold", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Action> tmpAction;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpAction = this->action;
                  }
                  if (tmpAction) tmpAction->hold();
              }},
              {"/takeoff", [this](Message *) -> void {
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
              {"/land", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Action> tmpAction;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpAction = this->action;
                  }
                  if (tmpAction) tmpAction->land();
              }},
              {"/rtl", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Action> tmpAction;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpAction = this->action;
                  }
                  if (tmpAction) tmpAction->return_to_launch();
              }},
              {"/position", [this](Message *) -> void {
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
              {"/battery", [this](Message *) -> void {
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
              {"/velocity", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpTelemetry = this->telemetry;
                  }
                  if (tmpTelemetry) tmpTelemetry->velocity_ned();
              }},
              {"/attitude", [this](Message*) -> void {
                  std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpTelemetry = this->telemetry;
                  }
                  if (tmpTelemetry) tmpTelemetry->attitude_euler();
              }},
              {"/gps", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpTelemetry = this->telemetry;
                  }
                  if (tmpTelemetry) tmpTelemetry->gps_info();
              }},
              {"/state", [this](Message *) -> void {
                  std::shared_ptr<mavsdk::Telemetry> tmpTelemetry;
                  {
                      std::unique_lock<std::mutex> Lock(lock);
                      tmpTelemetry = this->telemetry;
                  }
                  if (tmpTelemetry) tmpTelemetry->health();
              }}
          }}, // 结束 HttpMethod::GET 的映射
          { HttpMethod::POST, { 
              {"/register", [this](Message *mess) -> void {
                  std::string name, password;
                  std::istringstream iss(mess->getBody());

                  if (!(iss >> name) || !(iss >> password)) {
                      mess->setFile(root + "/registerError.html");
                      LOG_ERROR("Payload 解析失败，内容: ", mess->getBody());
                      return;
                  }
                  if (user.add(name, password)) {
                      mess->setFile(root + "/login.html");
                      LOG_INFO("User Register Success: ", name);
                  } else {
                      mess->setFile(root + "/registerError.html");
                      LOG_ERROR("User Register Failed (User Exists): ", name);
                  }
              }},
              {"/login", [this](Message *mess) -> void {
                  std::string name, password;
                  std::istringstream iss(mess->getBody());

                  if (!(iss >> name) || !(iss >> password)) {
                      mess->setFile(root + "/logError.html");
                      LOG_ERROR("Payload 解析失败，内容: ", mess->getBody());
                      return;
                  }

                  std::string token = user.login(name, password);

                  if (!token.empty()) {
                      mess->setToken(token);
                      mess->setFile(root + "/index.html");
                      LOG_INFO("User Login Success: ", name);
                  } else {
                      mess->setFile(root + "/logError.html");
                      LOG_WARN("User Login Failed (Wrong password/No user):", name);
                  }
              }}
          }} // 结束 HttpMethod::POST 的映射
      } {}

void Router::invoke(Message*mess,HttpMethod method,const URL&url) {
    std::unordered_map<HttpMethod,std::unordered_map<URL, HttpHandler> >::const_iterator it = routes.find(method);
    if (it != routes.end() ){
        std::unordered_map<URL, HttpHandler>::const_iterator uh = it->second.find(url );
        if(uh != it -> second.end() )
        uh->second(mess);
    }
};

void Router::route(Message *mess) {

  HttpMethod method = mess->getMethod();
  const URL&url =mess->getURL();

  if (url == "/register" || url == "/login") {//不能用cookie
    invoke(mess,method,url);
    return;
  }

  const std::string&cookie = mess -> getCookie();

  bool invalid = cookie.empty();
  std::string token;
  if (invalid == false) {
    size_t pos = cookie.find("token=");
    if (pos != std::string::npos && (pos == 0 || cookie[pos - 1] == ' ' || cookie[pos - 1] == ';')) 
    {
      pos += 6;
      size_t endPos = cookie.find(';', pos);
      if (endPos != std::string::npos)
        token = cookie.substr(pos, endPos - pos);
      else
        token = cookie.substr(pos);
    } else
      invalid = true;
  }

  if (invalid || user.verify(token) == false) 
    routes[HttpMethod::GET]["/login"](mess);
  else if (url == "/")
    routes[HttpMethod::GET]["/index"](mess);
  else
      invoke(mess,method,url);
}