# myWifi - ESP32 WiFi管理模块

一个功能完整的ESP32 WiFi管理模块,提供智能的AP/STA切换和Web配网功能。

## 核心特性

- **多模式支持**: AP、STA、AP+STA混合模式自动切换
- **NVS配置存储**: 自动保存和读取WiFi配置
- **智能重连机制**: 断线立即启动AP+STA模式,后台持续重连
- **Web配网门户**: 类似WiFiManager的现代化配网界面
- **零等待切换**: 断线后立即提供AP服务,无需等待
- **自动恢复**: 重连成功后自动切换回STA模式

## 快速开始

### 基本使用

```cpp
#include "Network/myWifi.h"

void setup() {
    Serial.begin(115200);
    myWiFi.begin();  // 初始化WiFi
}

void loop() {
    myWiFi.handle();  // 必须调用以处理WiFi状态
}
```

### 获取状态

```cpp
// 检查连接状态
if (myWiFi.isConnected()) {
    Serial.println("IP: " + myWiFi.getLocalIP());
    Serial.println("SSID: " + myWiFi.getConnectedSSID());
    Serial.println("信号: " + String(myWiFi.getRSSI()) + " dBm");
}

// 获取当前模式
wifi_mode_t mode = myWiFi.getCurrentMode();
// WIFI_MODE_STA = 1, WIFI_MODE_AP = 2, WIFI_MODE_APSTA = 3
```

## 工作原理

### 启动流程
```
1. 读取NVS配置
   ├─ 有配置? 尝试连接WiFi
   │  ├─ 成功 → STA模式运行
   │  └─ 失败 → AP配网模式
   └─ 无配置 → AP配网模式
```

### 智能重连机制 (v1.2+)

**断线后立即响应:**
```
检测到断线 (0秒)
  ↓
立即启动AP+STA混合模式
  ├─ [AP线程] 提供配网服务 (192.168.4.1)
  └─ [STA线程] 后台持续重连原热点
       ↓
     重连成功
       ↓
     自动关闭AP,切换回STA模式
```

**优势:**
- ⚡ **零等待**: 断线后立即可访问设备
- ✅ **高可用**: 配网和重连并行进行
- 🔄 **自动化**: 连接成功自动切换模式
- 🎯 **灵活**: 随时可重新配置WiFi

## Web配网界面

当设备进入AP配网模式时:

1. 连接热点: `ESP32-Gateway` (密码: `12345678`)
2. 打开浏览器访问: `192.168.4.1`
3. 点击"Scan Networks"扫描可用WiFi
4. 选择网络,输入密码
5. 点击"Save & Connect"

设备会自动尝试连接,成功后自动切换到STA模式。

## 配置参数

### 默认设置 (myWifi.h)

```cpp
// AP热点配置
#define DEFAULT_AP_SSID "ESP32-Gateway"
#define DEFAULT_AP_PASSWORD "12345678"

// 连接参数
#define WIFI_CONNECT_TIMEOUT 10000        // 连接超时: 10秒
#define WIFI_RECONNECT_INTERVAL 5000      // 快速重连: 5秒
#define WIFI_RECONNECT_INTERVAL_SLOW 30000 // 慢速重连: 30秒
#define WIFI_CHECK_INTERVAL 5000          // 状态检查: 5秒
#define AP_CHECK_STA_INTERVAL 3000        // AP模式检测: 3秒
```

### 智能重连策略

- 前3次: 5秒快速重连
- 3次后: 30秒慢速重连
- 断线后: 立即尝试重连(不等待)

## API参考

### 初始化

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `begin()` | 初始化WiFi模块 | bool |
| `handle()` | 处理WiFi状态(必须在loop中调用) | void |

### 状态查询

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `isConnected()` | 是否已连接 | bool |
| `getConnectedSSID()` | 获取已连接的SSID | String |
| `getLocalIP()` | 获取本地IP地址 | String |
| `getAPIP()` | 获取AP IP地址 | String |
| `getRSSI()` | 获取信号强度 | int (dBm) |
| `getCurrentMode()` | 获取当前模式 | wifi_mode_t |
| `getStatus()` | 获取当前状态 | WiFiStatus_t |

### 配置管理

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `setSTACredentials(ssid, pwd)` | 设置并保存STA凭据 | bool |
| `setAPCredentials(ssid, pwd)` | 设置并保存AP凭据 | bool |
| `clearConfig()` | 清除所有配置 | void |
| `startConfigPortal()` | 手动启动配网门户 | void |

## 高级用法

### 监听状态变化

```cpp
WiFiStatus_t lastStatus = WIFI_STATUS_IDLE;

void loop() {
    myWiFi.handle();
    
    WiFiStatus_t currentStatus = myWiFi.getStatus();
    if (currentStatus != lastStatus) {
        if (currentStatus == WIFI_STATUS_CONNECTED) {
            Serial.println("WiFi已连接!");
            // 启动需要网络的服务
        } else if (currentStatus == WIFI_STATUS_DISCONNECTED) {
            Serial.println("WiFi断开连接!");
            // 停止网络服务
        }
        lastStatus = currentStatus;
    }
}
```

### 信号强度监控

```cpp
if (myWiFi.isConnected()) {
    int rssi = myWiFi.getRSSI();
    if (rssi > -50) {
        Serial.println("信号优秀");
    } else if (rssi > -70) {
        Serial.println("信号良好");
    } else {
        Serial.println("信号较弱");
    }
}
```

## 测试指南

### 基础功能测试

1. ✅ 首次启动进入AP模式
2. ✅ Web配网功能正常
3. ✅ WiFi扫描成功
4. ✅ 配网后无崩溃
5. ✅ 配置保存到NVS
6. ✅ 重启后自动连接

### 断线重连测试

1. 设备正常连接WiFi
2. 关闭路由器
3. **立即**: ESP32进入AP+STA模式(无等待)
4. 可以连接AP进行配网
5. 重新打开路由器
6. ESP32自动重连成功
7. 自动关闭AP,切回STA模式

### 预期串口日志

**正常运行:**
```
[WiFi] Connected! IP: 192.168.1.100
[WiFi] RSSI: -45 dBm
```

**检测到断线:**
```
[WiFi] Connection lost!
[WiFi] Starting AP+STA mode for reconnection...
[WiFi] AP IP address: 192.168.4.1
```

**自动重连成功:**
```
[WiFi] Reconnected successfully! IP: 192.168.1.100
[WiFi] Switching to STA mode...
```

## 故障排除

### 问题1: 无法连接WiFi
- 检查SSID和密码是否正确
- 检查路由器是否正常工作
- 查看串口输出的错误信息

### 问题2: 配网页面无法打开
- 确认已连接到ESP32的AP热点
- 尝试访问 `192.168.4.1`
- 检查防火墙设置

### 问题3: 频繁断线重连
- 检查WiFi信号强度 (RSSI)
- 可能是路由器距离太远
- 建议信号强度 > -70 dBm

### 问题4: NVS保存失败
- 检查分区表配置
- 确保NVS分区足够大(至少20KB)
- 可以尝试 `myWiFi.clearConfig()` 清除配置

## 技术亮点

### 1. 延迟停止机制 (防止崩溃)

**问题**: 在Web回调中删除WebServer对象会导致Use-After-Free崩溃

**解决方案**: 
```cpp
// 在回调中设置标志
void handleSave() {
    shouldStopPortal = true;
    portalStopTime = millis() + 2000;
}

// 在主循环中安全删除
void handle() {
    if (shouldStopPortal && currentTime >= portalStopTime) {
        stopConfigPortal();  // 安全删除
    }
}
```

### 2. AP+STA并行工作

- AP模式提供配网服务
- STA模式后台持续重连
- 两者互不干扰,自动协调

### 3. 智能重连策略

- 前3次: 快速重连(5秒)
- 之后: 慢速重连(30秒)
- 断线立即尝试(无等待)
- 成功后重置计数器

## 性能指标

| 指标 | 数值 |
|------|------|
| 配网页面加载时间 | < 2秒 |
| WiFi扫描时间 | 2-5秒 |
| 连接WiFi时间 | 3-10秒 |
| 断线响应时间 | 立即 |
| AP服务可用时间 | 立即 |
| 内存占用(运行) | ~50KB |
| 内存占用(配网) | ~100KB |

## 注意事项

1. **必须调用handle()**: 在 `loop()` 中必须定期调用
2. **AP密码长度**: 至少8个字符
3. **内存占用**: AP+STA模式增加约50KB内存占用
4. **非阻塞设计**: 所有操作都是非阻塞的
5. **Flash要求**: 确保分区表中NVS分区至少20KB

## 版本历史

- **v1.0** (2025-01-19) - 初始版本
- **v1.1** (2025-01-19) - 修复StoreProhibited崩溃问题
- **v1.2** (2025-01-19) - AP/STA智能切换机制
  - 断线立即启动AP+STA
  - 持续重连不中断服务
  - 智能快速/慢速重连策略

## 分区表要求

使用 `esp32s3_4mb_partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000,  # 20KB (必需)
otadata,  data, ota,     0xE000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x160000,
```

## 技术支持

遇到问题请检查串口输出,所有重要操作都有详细日志。

---

**开发环境**: PlatformIO + ESP32-S3  
**测试设备**: ESP32-S3-DevKitC-1 (4MB Flash)  
**依赖库**: ESP32 WiFi, WebServer, DNSServer, Preferences
