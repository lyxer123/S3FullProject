// myWifi.h
#ifndef MY_WIFI_H
#define MY_WIFI_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// 默认配置值
#define DEFAULT_AP_SSID "ESP32-Gateway"        // 默认AP热点名称
#define DEFAULT_AP_PASSWORD "12345678"         // 默认AP密码(至少8位)
#define DEFAULT_STA_SSID ""                    // 默认STA连接的SSID(空表示无默认值)
#define DEFAULT_STA_PASSWORD ""                // 默认STA密码

// NVS存储的键名
#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_STA_SSID "sta_ssid"
#define NVS_KEY_STA_PASSWORD "sta_pwd"
#define NVS_KEY_AP_SSID "ap_ssid"
#define NVS_KEY_AP_PASSWORD "ap_pwd"

// WiFi连接参数
#define WIFI_CONNECT_TIMEOUT 10000             // WiFi连接超时时间(ms)
#define WIFI_RECONNECT_INTERVAL 5000           // 快速重连间隔(5秒)
#define WIFI_RECONNECT_INTERVAL_SLOW 30000     // 慢速重连间隔(30秒,失败3次后)
#define WIFI_MAX_RETRY_TIMES 999               // 最大重试次数(设置很大,持续重连)
#define WIFI_CHECK_INTERVAL 5000               // WiFi状态检查间隔(ms)
#define AP_CHECK_STA_INTERVAL 3000             // AP模式下检查STA连接的间隔(3秒,快速检测)

// DNS服务器配置
#define DNS_PORT 53
#define CAPTIVE_PORTAL_HOST "wifi.setup"

// WiFi状态枚举
enum WiFiStatus_t {
    WIFI_STATUS_IDLE = 0,           // 空闲状态
    WIFI_STATUS_CONNECTING,         // 正在连接
    WIFI_STATUS_CONNECTED,          // 已连接
    WIFI_STATUS_DISCONNECTED,       // 已断开
    WIFI_STATUS_AP_STARTED,         // AP已启动
    WIFI_STATUS_AP_STA_STARTED,     // AP+STA已启动
    WIFI_STATUS_RECONNECTING,       // 正在重连
    WIFI_STATUS_CONFIG_PORTAL       // 配网模式
};

class MyWiFi {
public:
    MyWiFi();
    ~MyWiFi();

    // 初始化WiFi
    bool begin();
    
    // 循环处理函数,需要在loop中调用
    void handle();
    
    // 获取当前WiFi状态
    WiFiStatus_t getStatus();
    
    // 获取当前WiFi模式
    wifi_mode_t getCurrentMode();
    
    // 获取连接的SSID
    String getConnectedSSID();
    
    // 获取本地IP地址(STA模式)
    String getLocalIP();
    
    // 获取AP IP地址
    String getAPIP();
    
    // 是否已连接到路由器
    bool isConnected();
    
    // 手动设置STA凭据并保存到NVS
    bool setSTACredentials(const String& ssid, const String& password);
    
    // 手动设置AP凭据并保存到NVS
    bool setAPCredentials(const String& ssid, const String& password);
    
    // 清除NVS中保存的WiFi配置
    void clearConfig();
    
    // 强制进入配网模式
    void startConfigPortal();
    
    // 断开连接并停止WiFi
    void disconnect();
    
    // 获取WiFi信号强度(RSSI)
    int getRSSI();
    
    // 获取连接的客户端数量(AP模式)
    int getConnectedClients();

private:
    // NVS偏好设置对象
    Preferences preferences;
    
    // Web服务器(用于配网)
    WebServer* webServer;
    
    // DNS服务器(用于强制门户)
    DNSServer* dnsServer;
    
    // WiFi配置信息
    String staSSID;
    String staPassword;
    String apSSID;
    String apPassword;
    
    // 状态变量
    WiFiStatus_t currentStatus;
    wifi_mode_t currentMode;
    unsigned long lastCheckTime;
    unsigned long lastReconnectTime;
    int reconnectAttempts;
    bool configPortalActive;
    unsigned long apModeStartTime;
    bool shouldStopPortal;
    unsigned long portalStopTime;
    
    // 内部功能函数
    bool loadConfig();                          // 从NVS加载配置
    bool saveConfig();                          // 保存配置到NVS
    void startSTA();                            // 启动STA模式
    void startAP();                             // 启动AP模式
    void startAPSTA();                          // 启动AP+STA模式
    bool connectToWiFi();                       // 连接到WiFi
    void handleReconnect();                     // 处理重连逻辑
    void handleConfigPortal();                  // 处理配网门户
    void setupWebServer();                      // 设置Web服务器
    void handleRoot();                          // 处理根页面请求
    void handleSave();                          // 处理保存配置请求
    void handleScan();                          // 处理WiFi扫描请求
    void handleStatus();                        // 处理状态查询请求
    void stopConfigPortal();                    // 停止配网门户
    String getConfigPortalHTML();               // 获取配网页面HTML
    String getStatusJSON();                     // 获取状态JSON
    void checkSTAConnection();                  // 检查STA连接状态
    void checkAPMode();                         // 在AP模式下检查是否可以连接STA
};

// 全局WiFi管理对象(外部声明,需在cpp中定义)
extern MyWiFi myWiFi;

#endif // MY_WIFI_H
