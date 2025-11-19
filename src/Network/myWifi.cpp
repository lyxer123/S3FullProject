// myWifi.cpp
#include "myWifi.h"

// 全局WiFi管理对象实例
MyWiFi myWiFi;

MyWiFi::MyWiFi() {
    webServer = nullptr;
    dnsServer = nullptr;
    currentStatus = WIFI_STATUS_IDLE;
    currentMode = WIFI_MODE_NULL;
    lastCheckTime = 0;
    lastReconnectTime = 0;
    reconnectAttempts = 0;
    configPortalActive = false;
    apModeStartTime = 0;
    shouldStopPortal = false;
    portalStopTime = 0;
}

MyWiFi::~MyWiFi() {
    if (webServer) {
        delete webServer;
        webServer = nullptr;
    }
    if (dnsServer) {
        delete dnsServer;
        dnsServer = nullptr;
    }
    preferences.end();
}

bool MyWiFi::begin() {
    Serial.println("[WiFi] Initializing WiFi...");
    
    // WiFi模式设置为NULL,准备配置
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // 加载配置
    if (!loadConfig()) {
        Serial.println("[WiFi] Failed to load config, using defaults");
    }
    
    Serial.printf("[WiFi] STA SSID: %s", staSSID.c_str());
    Serial.printf("[WiFi] AP SSID: %s", apSSID.c_str());
    
    // 如果有STA配置,尝试连接
    if (staSSID.length() > 0) {
        startSTA();
        if (connectToWiFi()) {
            Serial.println("[WiFi] Successfully connected to WiFi");
            currentStatus = WIFI_STATUS_CONNECTED;
            currentMode = WIFI_MODE_STA;
            return true;
        } else {
            Serial.println("[WiFi] Failed to connect, starting AP mode");
            startConfigPortal();
            return false;
        }
    } else {
        // 没有STA配置,直接进入AP配网模式
        Serial.println("[WiFi] No STA config found, starting config portal");
        startConfigPortal();
        return false;
    }
}

void MyWiFi::handle() {
    unsigned long currentTime = millis();
    
    // 检查是否需要延迟停止配网门户
    if (shouldStopPortal && currentTime >= portalStopTime) {
        shouldStopPortal = false;
        
        // 尝试连接新的WiFi
        stopConfigPortal();
        WiFi.mode(WIFI_STA);
        if (connectToWiFi()) {
            currentStatus = WIFI_STATUS_CONNECTED;
            currentMode = WIFI_MODE_STA;
            Serial.println("[WiFi] Successfully connected with new credentials");
        } else {
            Serial.println("[WiFi] Failed to connect with new credentials");
            startConfigPortal();
        }
        return;
    }
    
    // 处理配网门户
    if (configPortalActive && !shouldStopPortal) {
        handleConfigPortal();
        
        // 在AP/AP+STA模式下,周期性尝试连接STA
        if (currentTime - lastReconnectTime >= AP_CHECK_STA_INTERVAL) {
            lastReconnectTime = currentTime;
            if (staSSID.length() > 0 && currentStatus != WIFI_STATUS_CONNECTED) {
                checkAPMode();
            }
        }
    }
    
    // 检查STA连接状态
    if ((currentMode == WIFI_MODE_STA || currentMode == WIFI_MODE_APSTA) && 
        currentStatus == WIFI_STATUS_CONNECTED) {
        if (currentTime - lastCheckTime >= WIFI_CHECK_INTERVAL) {
            lastCheckTime = currentTime;
            checkSTAConnection();
        }
    }
    
    // 在断线状态下尝试重连
    if (currentStatus == WIFI_STATUS_DISCONNECTED && !configPortalActive) {
        if (currentTime - lastReconnectTime >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectTime = currentTime;
            handleReconnect();
        }
    }
}

WiFiStatus_t MyWiFi::getStatus() {
    return currentStatus;
}

wifi_mode_t MyWiFi::getCurrentMode() {
    return currentMode;
}

String MyWiFi::getConnectedSSID() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.SSID();
    }
    return "";
}

String MyWiFi::getLocalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String MyWiFi::getAPIP() {
    if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

bool MyWiFi::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int MyWiFi::getRSSI() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

int MyWiFi::getConnectedClients() {
    if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
        return WiFi.softAPgetStationNum();
    }
    return 0;
}

bool MyWiFi::setSTACredentials(const String& ssid, const String& password) {
    staSSID = ssid;
    staPassword = password;
    return saveConfig();
}

bool MyWiFi::setAPCredentials(const String& ssid, const String& password) {
    if (password.length() < 8) {
        Serial.println("[WiFi] AP password must be at least 8 characters");
        return false;
    }
    apSSID = ssid;
    apPassword = password;
    return saveConfig();
}

void MyWiFi::clearConfig() {
    Serial.println("[WiFi] Clearing WiFi configuration...");
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    
    staSSID = DEFAULT_STA_SSID;
    staPassword = DEFAULT_STA_PASSWORD;
    apSSID = DEFAULT_AP_SSID;
    apPassword = DEFAULT_AP_PASSWORD;
}

void MyWiFi::startConfigPortal() {
    Serial.println("[WiFi] Starting config portal...");
    
    stopConfigPortal(); // 先停止已有的配网门户
    
    // 启动AP模式
    startAP();
    
    // 设置DNS服务器用于强制门户
    if (!dnsServer) {
        dnsServer = new DNSServer();
    }
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
    
    // 设置Web服务器
    setupWebServer();
    
    configPortalActive = true;
    apModeStartTime = millis();
    currentStatus = WIFI_STATUS_CONFIG_PORTAL;
    
    Serial.printf("[WiFi] Config portal started at %s", WiFi.softAPIP().toString().c_str());
    Serial.printf("[WiFi] Connect to AP: %s", apSSID.c_str());
}

void MyWiFi::disconnect() {
    Serial.println("[WiFi] Disconnecting WiFi...");
    
    stopConfigPortal();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    currentStatus = WIFI_STATUS_IDLE;
    currentMode = WIFI_MODE_NULL;
}

// ============ 私有函数实现 ============

bool MyWiFi::loadConfig() {
    preferences.begin(NVS_NAMESPACE, true); // 只读模式
    
    staSSID = preferences.getString(NVS_KEY_STA_SSID, DEFAULT_STA_SSID);
    staPassword = preferences.getString(NVS_KEY_STA_PASSWORD, DEFAULT_STA_PASSWORD);
    apSSID = preferences.getString(NVS_KEY_AP_SSID, DEFAULT_AP_SSID);
    apPassword = preferences.getString(NVS_KEY_AP_PASSWORD, DEFAULT_AP_PASSWORD);
    
    preferences.end();
    
    // 验证AP密码长度
    if (apPassword.length() < 8) {
        apPassword = DEFAULT_AP_PASSWORD;
    }
    
    return true;
}

bool MyWiFi::saveConfig() {
    // 先关闭之前可能打开的实例
    preferences.end();
    
    // 添加延迟，确保NVS准备就绪
    delay(10);
    
    // 以读写模式打开
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("[WiFi] ERROR: Failed to open NVS namespace");
        
        // 尝试清除该命名空间并重新打开
        Serial.println("[WiFi] Attempting to clear namespace and retry...");
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();  // 只清除该命名空间
        preferences.end();
        delay(100);
        
        if (!preferences.begin(NVS_NAMESPACE, false)) {
            Serial.println("[WiFi] ERROR: Still failed after clear");
            return false;
        }
    }
    
    Serial.printf("[WiFi] Saving - STA SSID: %s (len:%d)", staSSID.c_str(), staSSID.length());
    Serial.printf("[WiFi] Saving - STA Pass: *** (len:%d)", staPassword.length());
    
    // 逐个保存并检查结果
    size_t result1 = preferences.putString(NVS_KEY_STA_SSID, staSSID);
    Serial.printf("[WiFi] putString(sta_ssid) returned: %d", result1);
    
    size_t result2 = preferences.putString(NVS_KEY_STA_PASSWORD, staPassword);
    Serial.printf("[WiFi] putString(sta_pwd) returned: %d", result2);
    
    size_t result3 = preferences.putString(NVS_KEY_AP_SSID, apSSID);
    Serial.printf("[WiFi] putString(ap_ssid) returned: %d", result3);
    
    size_t result4 = preferences.putString(NVS_KEY_AP_PASSWORD, apPassword);
    Serial.printf("[WiFi] putString(ap_pwd) returned: %d", result4);
    
    bool success = (result1 > 0) && (result2 > 0) && (result3 > 0) && (result4 > 0);
    
    preferences.end();
    
    if (success) {
        Serial.println("[WiFi] ✓ Configuration saved to NVS successfully");
    } else {
        Serial.println("[WiFi] ✗ Failed to save configuration to NVS");
        Serial.println("[WiFi]   Possible causes:");
        Serial.println("[WiFi]   1. NVS partition full");
        Serial.println("[WiFi]   2. String too long (max 1984 bytes)");
        Serial.println("[WiFi]   3. NVS corruption");
    }
    
    return success;
}

void MyWiFi::startSTA() {
    Serial.println("[WiFi] Starting STA mode...");
    WiFi.mode(WIFI_STA);
    currentMode = WIFI_MODE_STA;
    delay(100);
}

void MyWiFi::startAP() {
    Serial.printf("[WiFi] Starting AP mode with SSID: %s", apSSID.c_str());
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    
    currentMode = WIFI_MODE_AP;
    currentStatus = WIFI_STATUS_AP_STARTED;
    
    delay(100);
    
    Serial.printf("[WiFi] AP IP address: %s", WiFi.softAPIP().toString().c_str());
}

void MyWiFi::startAPSTA() {
    Serial.println("[WiFi] Starting AP+STA mode...");
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    
    currentMode = WIFI_MODE_APSTA;
    currentStatus = WIFI_STATUS_AP_STA_STARTED;
    
    delay(100);
}

bool MyWiFi::connectToWiFi() {
    if (staSSID.length() == 0) {
        Serial.println("[WiFi] No SSID configured");
        return false;
    }
    
    Serial.printf("[WiFi] Connecting to %s...", staSSID.c_str());
    
    currentStatus = WIFI_STATUS_CONNECTING;
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm", WiFi.RSSI());
        reconnectAttempts = 0;
        return true;
    } else {
        Serial.println("[WiFi] Connection failed");
        return false;
    }
}

void MyWiFi::handleReconnect() {
    reconnectAttempts++;
    Serial.printf("[WiFi] [AP+STA Mode] Reconnecting (attempt %d)...", reconnectAttempts);
    
    currentStatus = WIFI_STATUS_RECONNECTING;
    
    // 在AP+STA模式下尝试连接
    if (currentMode != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apSSID.c_str(), apPassword.c_str());
        currentMode = WIFI_MODE_APSTA;
    }
    
    // 尝试连接STA
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    
    unsigned long startTime = millis();
    bool connected = false;
    while (millis() - startTime < WIFI_CONNECT_TIMEOUT) {
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (connected) {
        Serial.printf("[WiFi] Reconnected successfully! IP: %s", WiFi.localIP().toString().c_str());
        currentStatus = WIFI_STATUS_CONNECTED;
        reconnectAttempts = 0;
        
        // 成功连接后,关闭AP模式,切换到纯STA
        Serial.println("[WiFi] Switching to STA mode...");
        stopConfigPortal();
        delay(100);
        WiFi.mode(WIFI_STA);
        currentMode = WIFI_MODE_STA;
    } else {
        Serial.println("[WiFi] Reconnection failed, staying in AP+STA mode");
        currentStatus = WIFI_STATUS_DISCONNECTED;
        
        // 保持AP+STA模式,继续提供配网服务
        if (currentMode != WIFI_MODE_APSTA) {
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(apSSID.c_str(), apPassword.c_str());
            currentMode = WIFI_MODE_APSTA;
        }
    }
}

void MyWiFi::checkSTAConnection() {
    if (WiFi.status() != WL_CONNECTED && currentStatus == WIFI_STATUS_CONNECTED) {
        Serial.println("[WiFi] Connection lost!");
        currentStatus = WIFI_STATUS_DISCONNECTED;
        lastReconnectTime = millis();
        reconnectAttempts = 0;  // 重置重连计数，启用快速重连
        
        // 立即启动AP+STA模式,提供配网服务的同时尝试重连
        Serial.println("[WiFi] Starting AP+STA mode for reconnection...");
        startAPSTA();
        
        // 启动配网门户
        if (!configPortalActive) {
            // 设置DNS服务器用于强制门户
            if (!dnsServer) {
                dnsServer = new DNSServer();
            }
            dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
            
            // 设置Web服务器
            setupWebServer();
            
            configPortalActive = true;
            Serial.println("[WiFi] Config portal started for reconfiguration");
        }
        
        // 立即尝试第一次重连，不等待间隔
        Serial.println("[WiFi] Attempting immediate reconnection...");
        handleReconnect();
    }
    
    // 如果仍在断线状态，继续尝试重连
    if (currentStatus == WIFI_STATUS_DISCONNECTED) {
        unsigned long currentTime = millis();
        
        // 智能重连间隔：前3次每5秒，之后每30秒
        unsigned long reconnectInterval = (reconnectAttempts < 3) ? 
                                          WIFI_RECONNECT_INTERVAL : 
                                          WIFI_RECONNECT_INTERVAL_SLOW;
        
        if (currentTime - lastReconnectTime >= reconnectInterval) {
            lastReconnectTime = currentTime;
            handleReconnect();
        }
    }
}

void MyWiFi::checkAPMode() {
    // 在AP或AP+STA模式下尝试连接STA
    if (staSSID.length() > 0) {
        wl_status_t status = WiFi.status();
        
        // 先检查是否已连接成功
        if (status == WL_CONNECTED) {
            // 已连接成功
            Serial.printf("[WiFi] STA connected! IP: %s", WiFi.localIP().toString().c_str());
            Serial.println("[WiFi] Stopping AP and switching to STA mode");
            
            stopConfigPortal();
            currentStatus = WIFI_STATUS_CONNECTED;
            reconnectAttempts = 0;
            
            delay(100);
            WiFi.mode(WIFI_STA);
            currentMode = WIFI_MODE_STA;
            return;
        }
        
        // 只有在空闲或断开状态时才尝试连接，避免频繁调用WiFi.begin()
        if (status == WL_IDLE_STATUS || status == WL_DISCONNECTED || status == WL_NO_SSID_AVAIL) {
            // 使用静态变量避免过于频繁的连接尝试
            static unsigned long lastAttempt = 0;
            if (millis() - lastAttempt < 3000) {  // 间隔3秒，更快响应
                return;
            }
            lastAttempt = millis();
            
            Serial.println("[WiFi] [AP/AP+STA Mode] Attempting STA connection...");
            
            // 确保在AP+STA模式
            if (currentMode == WIFI_MODE_AP) {
                WiFi.mode(WIFI_AP_STA);
                currentMode = WIFI_MODE_APSTA;
                delay(100);
            }
            
            // 发起连接（非阻塞）
            WiFi.begin(staSSID.c_str(), staPassword.c_str());
        }
        // 如果是 WL_CONNECT_FAILED 等其他状态，等待下一个周期
    }
}

void MyWiFi::handleConfigPortal() {
    if (!dnsServer || !webServer) {
        return;
    }
    
    dnsServer->processNextRequest();
    webServer->handleClient();
}

void MyWiFi::setupWebServer() {
    if (!webServer) {
        webServer = new WebServer(80);
    }
    
    // 绑定处理函数
    webServer->on("/", [this]() { this->handleRoot(); });
    webServer->on("/save", HTTP_POST, [this]() { this->handleSave(); });
    webServer->on("/scan", [this]() { this->handleScan(); });
    webServer->on("/status", [this]() { this->handleStatus(); });
    webServer->onNotFound([this]() { this->handleRoot(); }); // 强制门户
    
    webServer->begin();
    Serial.println("[WiFi] Web server started");
}

void MyWiFi::handleRoot() {
    if (!webServer) return;
    webServer->send(200, "text/html", getConfigPortalHTML());
}

void MyWiFi::handleSave() {
    if (!webServer) return;
    
    String newSSID = webServer->arg("ssid");
    String newPassword = webServer->arg("password");
    
    Serial.printf("[WiFi] Received new credentials - SSID: %s\n", newSSID.c_str());
    
    if (newSSID.length() > 0) {
        staSSID = newSSID;
        staPassword = newPassword;
        
        if (!saveConfig()) {
            Serial.println("[WiFi] Warning: Failed to save config to NVS, but will try to connect");
        }
        
        webServer->send(200, "text/html", 
            "<html><body><h2>Configuration Saved!</h2>"
            "<p>Connecting to WiFi...</p>"
            "<p>Please wait...</p>"
            "</body></html>");
        
        // 设置延迟停止标志,避免在handleClient中删除webServer
        shouldStopPortal = true;
        portalStopTime = millis() + 2000;  // 2秒后停止
        
        Serial.println("[WiFi] Will stop config portal in 2 seconds...");
    } else {
        webServer->send(400, "text/html", "<html><body><h2>Error: SSID cannot be empty</h2></body></html>");
    }
}

void MyWiFi::handleScan() {
    if (!webServer) return;
    
    Serial.println("[WiFi] Scanning networks...");
    
    // 确保WiFi在支持扫描的模式下
    wifi_mode_t currentWiFiMode = WiFi.getMode();
    Serial.printf("[WiFi] Current mode before scan: %d", currentWiFiMode);
    
    // 如果是纯AP模式，临时切换到AP+STA以支持扫描
    bool needRestoreMode = false;
    if (currentWiFiMode == WIFI_MODE_AP) {
        Serial.println("[WiFi] Switching to AP+STA mode for scanning...");
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        needRestoreMode = true;
    }
    
    // 在AP+STA模式下，先停止STA连接以避免扫描冲突
    bool wasDisconnected = false;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Stopping STA connection for scan...");
        WiFi.disconnect();
        delay(200);
        wasDisconnected = true;
    }
    
    // 执行扫描（阻塞式，但添加超时保护）
    int n = WiFi.scanNetworks(false, true);  // false=同步扫描, true=显示隐藏网络
    Serial.printf("[WiFi] Found %d networks", n);
    
    String json = "[";
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"encryption\":" + String(WiFi.encryptionType(i));
            json += "}";
        }
    } else if (n == 0) {
        Serial.println("[WiFi] No networks found");
    } else {
        Serial.printf("[WiFi] Scan failed with error code: %d", n);
    }
    json += "]";
    
    WiFi.scanDelete();
    
    // 恢复原来的模式
    if (needRestoreMode) {
        Serial.println("[WiFi] Restoring AP mode...");
        WiFi.mode(WIFI_MODE_AP);
        delay(100);
    }
    
    webServer->send(200, "application/json", json);
    Serial.println("[WiFi] Scan response sent");
}

void MyWiFi::handleStatus() {
    if (!webServer) return;
    webServer->send(200, "application/json", getStatusJSON());
}

void MyWiFi::stopConfigPortal() {
    if (configPortalActive) {
        Serial.println("[WiFi] Stopping config portal...");
        
        // 先停止接受新请求
        if (dnsServer) {
            dnsServer->stop();
        }
        
        if (webServer) {
            webServer->stop();
        }
        
        // 小延迟确保所有处理完成
        delay(100);
        
        // 释放内存
        if (webServer) {
            delete webServer;
            webServer = nullptr;
        }
        
        if (dnsServer) {
            delete dnsServer;
            dnsServer = nullptr;
        }
        
        configPortalActive = false;
        shouldStopPortal = false;
    }
}

String MyWiFi::getConfigPortalHTML() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>ESP32 WiFi Setup</title><style>";
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);";
    html += "min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}";
    html += ".container{background:white;border-radius:10px;padding:30px;max-width:500px;width:100%;";
    html += "box-shadow:0 10px 40px rgba(0,0,0,0.2)}";
    html += "h1{color:#333;margin-bottom:10px;text-align:center}";
    html += ".subtitle{color:#666;text-align:center;margin-bottom:30px;font-size:14px}";
    html += ".form-group{margin-bottom:20px}";
    html += "label{display:block;margin-bottom:5px;color:#555;font-weight:bold}";
    html += "input,select{width:100%;padding:12px;border:2px solid #ddd;border-radius:5px;";
    html += "font-size:16px;transition:border-color 0.3s}";
    html += "input:focus,select:focus{outline:none;border-color:#667eea}";
    html += "button{width:100%;padding:12px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);";
    html += "color:white;border:none;border-radius:5px;font-size:16px;font-weight:bold;";
    html += "cursor:pointer;transition:transform 0.2s}";
    html += "button:hover{transform:translateY(-2px)}";
    html += "button:active{transform:translateY(0)}";
    html += ".scan-btn{background:linear-gradient(135deg,#f093fb 0%,#f5576c 100%);margin-bottom:15px}";
    html += ".network-list{max-height:200px;overflow-y:auto;border:2px solid #ddd;border-radius:5px;margin-top:10px}";
    html += ".network-item{padding:10px;border-bottom:1px solid #eee;cursor:pointer;transition:background 0.2s}";
    html += ".network-item:hover{background:#f5f5f5}";
    html += ".network-item:last-child{border-bottom:none}";
    html += ".signal{float:right;color:#888}";
    html += ".loading{text-align:center;color:#666;padding:20px}";
    html += ".info{background:#e3f2fd;padding:15px;border-radius:5px;margin-bottom:20px;font-size:14px;color:#1976d2}";
    html += "</style></head><body><div class=\"container\">";
    html += "<h1>WiFi Configuration</h1>";
    html += "<p class=\"subtitle\">ESP32 Gateway Setup</p>";
    html += "<div class=\"info\"><strong>Current AP:</strong> " + apSSID + "<br>";
    html += "<strong>IP Address:</strong> " + WiFi.softAPIP().toString() + "</div>";
    html += "<button class=\"scan-btn\" onclick=\"scanNetworks()\">Scan Networks</button>";
    html += "<div id=\"networkList\"></div>";
    html += "<form action=\"/save\" method=\"POST\">";
    html += "<div class=\"form-group\"><label for=\"ssid\">WiFi Network (SSID):</label>";
    html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"Enter WiFi SSID\" required></div>";
    html += "<div class=\"form-group\"><label for=\"password\">Password:</label>";
    html += "<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter WiFi Password\"></div>";
    html += "<button type=\"submit\">Save &amp; Connect</button></form></div>";
    html += "<script>";
    html += "function scanNetworks(){";
    html += "document.getElementById('networkList').innerHTML='<div class=\"loading\">Scanning...</div>';";
    html += "fetch('/scan').then(response=>response.json()).then(networks=>{";
    html += "let html='<div class=\"network-list\">';";
    html += "networks.forEach(network=>{";
    html += "let signal=network.rssi;";
    html += "let bars=signal>-50?'[****]':signal>-70?'[***]':'[**]';";
    html += "let lock=network.encryption>0?'[Lock] ':'';";
    html += "html+='<div class=\"network-item\" onclick=\"selectNetwork(\\''+network.ssid+'\\')\">';";
    html += "html+=lock+network.ssid+'<span class=\"signal\">'+bars+' '+signal+'dBm</span></div>';";
    html += "});";
    html += "html+='</div>';";
    html += "document.getElementById('networkList').innerHTML=html;";
    html += "}).catch(error=>{";
    html += "document.getElementById('networkList').innerHTML='<div class=\"loading\">Scan failed</div>';";
    html += "});}";
    html += "function selectNetwork(ssid){";
    html += "document.getElementById('ssid').value=ssid;";
    html += "document.getElementById('password').focus();}";
    html += "window.onload=function(){scanNetworks();};";
    html += "</script></body></html>";
    return html;
}

String MyWiFi::getStatusJSON() {
    String json = "{";
    json += "\"mode\":\"" + String(currentMode) + "\",";
    json += "\"status\":\"" + String(currentStatus) + "\",";
    json += "\"sta_ssid\":\"" + getConnectedSSID() + "\",";
    json += "\"sta_ip\":\"" + getLocalIP() + "\",";
    json += "\"sta_rssi\":" + String(getRSSI()) + ",";
    json += "\"ap_ssid\":\"" + apSSID + "\",";
    json += "\"ap_ip\":\"" + getAPIP() + "\",";
    json += "\"ap_clients\":" + String(getConnectedClients());
    json += "}";
    return json;
}
