#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>

#define SERVER_IP "192.168.31.90" // MQTT 服务器 IP 地址
#define SERVER_PORT 1884        // MQTT 服务器端口
#define SSID "Mi3000T_2.4G"       // WiFi 名称
#define PASSWORD "123mi123" // WiFi 密码
#define ADC_PIN 32 // ADC 引脚 ❗️模拟测温数据
#define ADC_PIN2 33 // 第二个 ADC 引脚 ❗️模拟测温数据
#define SD_CS 5  // SD 卡片选引脚
// ========== 配置区域 ==========
const char* ssid = SSID;
const char* password = PASSWORD ;
const char* mqtt_server = SERVER_IP;
const int   mqtt_port = SERVER_PORT;
const char* topic = "/temp";
unsigned short channels = 0;
int period = 0; // 上传周期，单位：毫秒
bool isConfiged = false; //是否从服务器收到配置信息
bool isSdCardMounted = false; // SD卡是否已挂载
float temps[16]; // 温度通道数据缓存
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastUploadTime = 0;
String macAddress;
// ========== 函数声明 ==========
String getMacAddress();                            // 获取MAC地址
void setup_wifi();                                 // 连接 WiFi
void initTime();                                   // 初始化时间
String getTimestamp();                             // 获取时间戳 格式YYYY-MM-DD HH:MM:SS.sss
void reconnect();                                  // MQTT 重连
void getConfig();                                  // 向MQTT的/config/<macAddress>获取本机设置，如果没有，则向/firstConnect发布请求
void mqttCallback(char* topic, 
                  byte* payload, 
                  unsigned int length);            // MQTT 回调函数，处理收到的消息
void readTemps();                                  // 读取所有通道的温度数据
void publishTemp();                                // 发布温度数据
void readFile(String path, String* content);                        // 读取SD卡文件内容
void writeFile(String path,String* content);      // 写入内容到SD卡文件
void SdCardSetup();                                // 初始化SD卡
// ========== 初始化 ==========

void setup() {
  macAddress = getMacAddress();                 // 获取MAC地址
  Serial.begin(115200);
  Serial.print("MAC Address: ");
  Serial.println(macAddress); 

  setup_wifi();                                 //连接WiFi
  initTime();                                   //初始化时间
  client.setServer(mqtt_server, mqtt_port);     //设置MQTT服务器
  client.setCallback(mqttCallback);             //设置MQTT回调函数
  if (!client.connected()) {                    //连接MQTT服务器
    reconnect();
  }
  getConfig();                                  //获取配置信息
}

// ========== 主循环 ==========
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  // 到达上传周期
  if (now - lastUploadTime >= period) {
    lastUploadTime = now;
    readTemps();
    publishTemp();
  }
}

// ========== 函数实现 ==========

String getMacAddress() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  String macAddress = String(mac[0], HEX) +
                      String(mac[1], HEX) +
                      String(mac[2], HEX) +
                      String(mac[3], HEX) +
                      String(mac[4], HEX) +
                      String(mac[5], HEX);
  macAddress.toUpperCase();
  return macAddress;
}

void setup_wifi() {
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initTime() {
  configTime(8 * 3600, 0, "pool.ntp.org", "time.windows.com");  // 东八区 ,❗️时间服务器后续需改为本地的时间服务器
  Serial.println("Waiting for time sync...");
  // 等待同步
  struct tm timeInfo;
  while (!getLocalTime(&timeInfo)) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized.");
}

String getTimestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  struct tm timeinfo;
  localtime_r(&tv.tv_sec, &timeinfo);

  char buffer[64];
  snprintf(buffer, sizeof(buffer),
           "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec,
           tv.tv_usec / 1000   // 微秒 → 毫秒
  );

  return String(buffer);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");
    if (client.connect(macAddress.c_str())) {
      Serial.println("connected.");
      String topic = "/config/" + macAddress;
      client.subscribe(topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("  retry in 2 seconds...");
      delay(2000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.printf("收到配置 topic=%s, msg=%s\n", topic, msg.c_str());

  if (msg.indexOf("channels") >= 0 && msg.indexOf("samplingPeriod") >= 0) {
    int c1 = msg.indexOf("channels");
    int c2 = msg.indexOf("samplingPeriod");

    channels = msg.substring(msg.indexOf(":", c1)+1, msg.indexOf(",", c1)).toInt();
    period = msg.substring(msg.indexOf(":", c2)+1, msg.indexOf("}", c2)).toInt();

    isConfiged = true;
  }
}

void readTemps() {
    // ❗️这里模拟读取温度传感器数据
    for (int i = 0; i < channels; i++) {
        if(i == 1) {
            int adcValue2 = analogRead(ADC_PIN2);
            float faketemp2 = (4095 - adcValue2) * (2000 / 4095.0) - 200; // 用ADC值模拟温度，范围-200 ~ 1800
            temps[i] = faketemp2; 
            continue;
        }
        int adcValue = analogRead(ADC_PIN);
        float faketemp = (4095 - adcValue) * (2000 / 4095.0) - 200; // 用ADC值模拟温度，范围-200 ~ 1800
        temps[i] = faketemp; 
        //temps[i] = 20.0 + random(0, 100) / 10.0; // 用随机数模拟 20.0 ~ 29.9 的温度值
    }
}

void publishTemp() {
    StaticJsonDocument<256> doc;
    doc["time"] = getTimestamp();
    doc["mac"] = macAddress;

    for (int i = 0; i < channels; i++) {
        String key = "temp" + String(i + 1);
        doc[key] = temps[i];
    }

    char buffer[256];
    size_t len = serializeJson(doc, buffer);
    client.publish("/temp", buffer, len);
}

void getConfig(){
  String topic = "/config/" + macAddress;
  client.subscribe(topic.c_str());
  delay(100);

  Serial.println("等待配置信息 retain 或 服务端发送...");
  unsigned long start = millis();
  while (millis() - start < 3000) {   // 给3秒处理retain,❗️此处待优化
    client.loop();
    delay(10);
    if (isConfiged) break;
  }
  // 如果没有 retain，则向 /firstConnect 发广播请求配置
  if(isConfiged==false){
    client.publish("/firstConnect", macAddress.c_str());
  }
  // 阻塞等待配置到达
  while (!isConfiged) {
    client.loop();
    delay(50);
  }

  Serial.println("配置信息已接收！");
  Serial.printf("channels=%d, samplingPeriod=%d\n", channels, period);
}

void readFile(String path,String* content) {
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) {
    Serial.println("Failed to open file.");
    return;
  }

  Serial.println("----- File Content -----");
  while (file.available()) {
    Serial.write(file.read());
    char ch = file.read();
    content->concat(ch);
  }
  file.close();
}

void writeFile(String path,String* content) {
  File file = SD.open(path.c_str(), FILE_APPEND);
  if (!file) {
    Serial.println("Open file failed!");
    return;
  }
  file.println(*content);
  file.close();
  Serial.println("Write finished.");
}

void SdCardSetup() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    isSdCardMounted = false;
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    isSdCardMounted = false;
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  isSdCardMounted = true;
}