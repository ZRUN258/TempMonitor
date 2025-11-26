多通道温度巡检仪（ESP32）

本项目基于 ESP32，实现了一个可远程配置、自动上传温度数据的多通道温度巡检系统。
系统支持 MQTT 通讯、SD 卡配置本地化、NTP 本地时间同步、多通道温度采集（可扩展）。

✨ 功能特点
🛰️ 1. WiFi 自动联网

设备启动后自动连接预设 WiFi，并打印本机 IP。

📡 2. MQTT 远程配置

设备会订阅：

/config/<MAC地址>


如无配置文件，会向服务器发布首次上线请求：

/firstConnect


服务器返回配置 JSON，例如：

{
  "channels": 4,
  "samplingPeriod": 1000
}


设备自动保存至 SD 卡并立即生效。

💾 3. SD 卡配置持久化

配置被写入 /config.txt，掉电仍能保存。

设备优先从 SD 卡读取配置，无需每次上线都等待服务器。

🌡️ 4. 多通道温度采集

当前代码模拟两个 ADC 输入（ADC32 / ADC33），可扩展至 16 通道：

float temps[16];


真实使用时替换 readTemps() 内部逻辑即可。

☁️ 5. 温度上报

按 samplingPeriod 周期将所有通道的数据以 JSON 格式发布至：

/temp


示例：

{
  "time": "2025-01-01 12:00:00.123",
  "mac": "A1B2C3D4E5F6",
  "temp1": 23.5,
  "temp2": 24.1
}

🕒 6. 本地时间同步

使用 NTP 校时：

pool.ntp.org
time.windows.com


可替换为局域网 NTP 服务器。

🔧 硬件连接
ESP32 引脚
信号	引脚	说明
ADC1	GPIO32	温度通道 1 模拟输入
ADC2	GPIO33	温度通道 2 模拟输入
SD_CS	GPIO5	SD 卡片选引脚
MOSI/MISO/SCK	默认 SPI	SD 卡 SPI 接口

注意：温度采集部分为模拟示例，需根据实际传感器方案调整。

📂 配置文件格式（SD卡 /config.txt）
{
  "channels": 4,
  "samplingPeriod": 2000
}


字段说明：

字段	含义	类型
channels	温度通道数量 (1~16)	int
samplingPeriod	上报周期，毫秒	int
🧠 程序逻辑总览
flowchart TD

A[启动 ESP32] --> B[读取 SD 配置]
B -->|存在有效配置| E[进入工作模式]
B -->|无配置| C[连接 WiFi]

C --> D[连接 MQTT 服务器]
D --> F[尝试获取 retain 配置]

F -->|收到配置| G[写入SD]
F -->|未收到| H[向 /firstConnect 请求配置]

H --> I[等待服务器下发]
I --> G --> E

E --> J[周期采样温度]
J --> K[MQTT 发布 /temp]
K --> E

📦 MQTT 主题说明
主题	方向	说明
/config/<MAC>	Server → Device	下发配置 JSON
/firstConnect	Device → Server	首次上线请求服务器下发配置
/temp	Device → Server	温度 JSON 上报
🔌 如何部署服务器（示例）
Mosquitto MQTT 配置
sudo apt install mosquitto mosquitto-clients
mosquitto -v

订阅温度数据
mosquitto_sub -t /temp -v

🧪 测试示例
查看设备日志

通过串口打印可以看到：

WiFi IP

SD 卡类型和容量

MQTT 是否连接成功

配置是否加载

温度上传内容

📁 项目结构建议（发布 GitHub 时）
/src
    main.cpp  ← 当前代码
/doc
    wiring.png  ← 电路图
    mqtt-flow.md
README.md  ← 本文件
platformio.ini / arduino.json

📌 后续可扩展方向

✔️ 实际温度传感器（MAX6675 / MAX31855 / PT100 / DS18B20 等）

✔️ 离线缓存：无网络情况下写入 SD 卡

✔️ OTA 升级

✔️ Web 配置界面

✔️ 支持 Modbus RTU / TCP
