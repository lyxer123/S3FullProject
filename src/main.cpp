#include "main.h"
#include "Network/myWifi.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);  // 给一些时间让串口初始化

  Serial.println("=================================");
  Serial.println("ESP32 Gateway Starting...");
  Serial.println("=================================");

  // 初始化WiFi
  Serial.println("\n[SETUP] Initializing WiFi...");
  myWiFi.begin();

  Serial.println("\n[SETUP] Setup completed.");
}

void loop()
{
  // 处理WiFi状态和自动重连
  myWiFi.handle();

  // 每10秒打印一次WiFi状态信息
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    
    Serial.println("\n========== WiFi Status ==========");
    Serial.printf("Mode: %d\n", myWiFi.getCurrentMode());
    Serial.printf("Status: %d\n", myWiFi.getStatus());
    
    if (myWiFi.isConnected()) {
      Serial.printf("Connected to: %s\n", myWiFi.getConnectedSSID().c_str());
      Serial.printf("IP Address: %s\n", myWiFi.getLocalIP().c_str());
      Serial.printf("Signal: %d dBm\n", myWiFi.getRSSI());
    } else {
      Serial.println("Not connected to WiFi");
      wifi_mode_t mode = myWiFi.getCurrentMode();
      if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        Serial.printf("AP IP: %s\n", myWiFi.getAPIP().c_str());
        Serial.printf("AP Clients: %d\n", myWiFi.getConnectedClients());
      }
    }
    Serial.println("=================================\n");
  }

  // 添加一个短暂的延迟,让其他任务有机会运行
  delay(10);
}
