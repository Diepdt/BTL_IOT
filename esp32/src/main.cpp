/*
 * ESP32 ĐIỀU KHIỂN RƠ-LE 220V QUA MQTT (Phiên bản 8 - 2 Kênh)
 *
 * Chức năng:
 * 1. Thủ Công (MANUAL)
 * 2. Hẹn Giờ (SCHEDULE)
 * 3. Đếm Ngược (COUNTDOWN)
 * ...Tất cả đều hoạt động độc lập cho 2 kênh
 */

// BẮT BUỘC khi dùng .cpp
#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// 1. THÔNG TIN WIFI
char wifi_ssid[] = "1";
char wifi_pass[] = "12345689";

// 2. THÔNG SỐ MQTT
const char *MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const int NUM_RELAYS = 2; // Số lượng Rơ-le

// 3. ĐỊNH NGHĨA CÁC "CHỦ ĐỀ" (TOPIC) MQTT
// Code sẽ tự động thêm số 1 hoặc 2
#define MQTT_TOPIC_RELAY_SET "odien/relay/%d/set"
#define MQTT_TOPIC_RELAY_STATUS "odien/relay/%d/status"
#define MQTT_TOPIC_SCHEDULE_SET "odien/schedule/%d/set"
#define MQTT_TOPIC_MODE_STATUS "odien/mode/%d/status"
#define MQTT_TOPIC_COUNTDOWN_SET "odien/countdown/%d/set"

// 4. PIN VÀ CÀI ĐẶT GIỜ
const uint8_t RELAY_PINS[NUM_RELAYS] = {27, 26}; // {Ổ 1, Ổ 2}
const long GMT_OFFSET_SEC = 7 * 3600;

// --- Khởi tạo các đối tượng ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", GMT_OFFSET_SEC);

// --- Biến toàn cục (Nhân đôi cho 2 Rơ-le) ---
enum RelayMode
{
  MANUAL,
  SCHEDULE,
  COUNTDOWN
};

struct RelayData
{
  bool state;
  RelayMode mode;
  // Hẹn giờ
  long startTimeInSeconds;
  long stopTimeInSeconds;
  // Đếm ngược
  unsigned long countdownEndTime;
  bool countdownTargetState;
};

RelayData relays[NUM_RELAYS];

// --- HÀM 1: ĐIỀU KHIỂN RƠ-LE ---
void setRelay(int relayIndex, bool state)
{
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS)
    return;

  relays[relayIndex].state = state;
  // Kích mức thấp (LOW = BẬT, HIGH = TẮT)
  digitalWrite(RELAY_PINS[relayIndex], state ? LOW : HIGH);

  Serial.printf("Ro-le %d %s\n", relayIndex + 1, state ? "BAT" : "TAT");

  if (mqttClient.connected())
  {
    char topic[40];
    sprintf(topic, MQTT_TOPIC_RELAY_STATUS, relayIndex + 1);
    mqttClient.publish(topic, state ? "ON" : "OFF");
  }
}

// --- HÀM 1B: ĐẶT CHẾ ĐỘ ---
void setMode(int relayIndex, RelayMode mode)
{
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS)
    return;

  relays[relayIndex].mode = mode;

  const char *modeStr = "MANUAL";
  if (mode == SCHEDULE)
    modeStr = "SCHEDULE";
  if (mode == COUNTDOWN)
    modeStr = "COUNTDOWN";

  Serial.printf("Ro-le %d Che do: %s\n", relayIndex + 1, modeStr);
  if (mqttClient.connected())
  {
    char topic[40];
    sprintf(topic, MQTT_TOPIC_MODE_STATUS, relayIndex + 1);
    mqttClient.publish(topic, modeStr);
  }
}

// --- HÀM 2: HÀM ĐƯỢC GỌI KHI CÓ TIN NHẮN MQTT ĐẾN ---
void mqttCallback(char *topic, byte *payload, unsigned int length);

// --- HÀM 3: KẾT NỐI LẠI MQTT ---
void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Dang ket noi MQTT...");
    String clientId = "ESP32-2CH-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("da ket noi!");

      char subTopic[40];
      for (int i = 0; i < NUM_RELAYS; i++)
      {
        sprintf(subTopic, MQTT_TOPIC_RELAY_SET, i + 1);
        mqttClient.subscribe(subTopic);
        sprintf(subTopic, MQTT_TOPIC_SCHEDULE_SET, i + 1);
        mqttClient.subscribe(subTopic);
        sprintf(subTopic, MQTT_TOPIC_COUNTDOWN_SET, i + 1);
        mqttClient.subscribe(subTopic);

        // Gửi trạng thái hiện tại ngay khi kết nối
        setRelay(i, relays[i].state);
        setMode(i, relays[i].mode);
      }
    }
    else
    {
      Serial.print("that bai, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" thu lai sau 5 giay");
      delay(5000);
    }
  }
}

// --- HÀM 4: KIỂM TRA LỊCH ---
void checkSchedule(int relayIndex)
{
  if (relays[relayIndex].mode != SCHEDULE || relays[relayIndex].startTimeInSeconds < 0 || relays[relayIndex].stopTimeInSeconds < 0)
  {
    return;
  }

  long currentTimeInSeconds = ntpClient.getHours() * 3600 + ntpClient.getMinutes() * 60 + ntpClient.getSeconds();
  long startTime = relays[relayIndex].startTimeInSeconds;
  long stopTime = relays[relayIndex].stopTimeInSeconds;

  bool shouldBeOn = false;
  if (startTime < stopTime)
  { // (VD: 8h - 17h)
    if (currentTimeInSeconds >= startTime && currentTimeInSeconds < stopTime)
    {
      shouldBeOn = true;
    }
  }
  else if (startTime > stopTime)
  { // (VD: 22h - 6h)
    if (currentTimeInSeconds >= startTime || currentTimeInSeconds < stopTime)
    {
      shouldBeOn = true;
    }
  }
  if (shouldBeOn != relays[relayIndex].state)
  {
    setRelay(relayIndex, shouldBeOn);
  }
}

// --- HÀM 5: KIỂM TRA ĐẾM NGƯỢC ---
void checkCountdown(int relayIndex)
{
  if (relays[relayIndex].mode != COUNTDOWN)
  {
    return;
  }

  // Nếu đã đến lúc
  if (millis() >= relays[relayIndex].countdownEndTime)
  {
    setRelay(relayIndex, relays[relayIndex].countdownTargetState); // Thực hiện hành động
    setMode(relayIndex, MANUAL);                                   // Hoàn thành, quay về chế độ Thủ Công
  }
}

// --- SETUP ---
void setup()
{
  Serial.begin(115200);

  for (int i = 0; i < NUM_RELAYS; i++)
  {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH); // LOW = BẬT, nên mặc định là HIGH (TẮT)

    relays[i].state = false;
    relays[i].mode = MANUAL;
    relays[i].startTimeInSeconds = -1;
    relays[i].stopTimeInSeconds = -1;
    relays[i].countdownEndTime = 0;
  }

  Serial.print("Dang ket noi WiFi ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi da ket noi!");

  ntpClient.begin();
  ntpClient.update();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

// --- LOOP ---
unsigned long lastNTPUpdate = 0;
unsigned long lastScheduleCheck = 0;

void loop()
{
  if (!mqttClient.connected())
  {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Cập nhật NTP mỗi 15 phút
  if (millis() - lastNTPUpdate > 900000)
  {
    lastNTPUpdate = millis();
    ntpClient.update();
  }

  // Kiểm tra Hẹn Giờ (mỗi giây)
  if (millis() - lastScheduleCheck > 1000)
  {
    lastScheduleCheck = millis();
    for (int i = 0; i < NUM_RELAYS; i++)
    {
      checkSchedule(i);
    }
  }

  // Kiểm tra Đếm Ngược (liên tục)
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    checkCountdown(i);
  }
}

// --- HÀM 2 (Phần định nghĩa) ---
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Nhan tin nhan tu topic [");
  Serial.print(topic);
  Serial.print("]: ");

  char message[length + 1];
  strncpy(message, (char *)payload, length);
  message[length] = '\0';
  Serial.println(message);

  // Xác định xem topic này cho Rơ-le nào (1 hay 2)
  int relayIndex = -1;
  if (strstr(topic, "/1/"))
    relayIndex = 0; // Ổ 1 (index 0)
  if (strstr(topic, "/2/"))
    relayIndex = 1; // Ổ 2 (index 1)

  if (relayIndex == -1)
    return; // Topic không hợp lệ

  char baseTopic[40];

  // A. Xử lý lệnh Bật/Tắt thủ công
  sprintf(baseTopic, MQTT_TOPIC_RELAY_SET, relayIndex + 1);
  if (strcmp(topic, baseTopic) == 0)
  {
    setMode(relayIndex, MANUAL); // Bấm tay là chuyển sang chế độ Thủ Công
    setRelay(relayIndex, (strcmp(message, "ON") == 0));
  }

  // B. Xử lý lịch hẹn giờ
  sprintf(baseTopic, MQTT_TOPIC_SCHEDULE_SET, relayIndex + 1);
  if (strcmp(topic, baseTopic) == 0)
  {
    JsonDocument doc;
    deserializeJson(doc, message);

    const char *start = doc["start"];
    const char *stop = doc["stop"];
    bool active = doc["active"];

    if (start)
    {
      long H = strtol(strtok((char *)start, ":"), NULL, 10);
      long M = strtol(strtok(NULL, ":"), NULL, 10);
      relays[relayIndex].startTimeInSeconds = H * 3600 + M * 60;
    }
    if (stop)
    {
      long H = strtol(strtok((char *)stop, ":"), NULL, 10);
      long M = strtol(strtok(NULL, ":"), NULL, 10);
      relays[relayIndex].stopTimeInSeconds = H * 3600 + M * 60;
    }

    setMode(relayIndex, active ? SCHEDULE : MANUAL);

    if (!active)
    {
      setRelay(relayIndex, false); // Nếu tắt hẹn giờ thì tắt luôn Rơ-le
    }
    else
    {
      checkSchedule(relayIndex); // Kiểm tra lịch ngay
    }
  }

  // C. Xử lý lệnh Đếm Ngược
  sprintf(baseTopic, MQTT_TOPIC_COUNTDOWN_SET, relayIndex + 1);
  if (strcmp(topic, baseTopic) == 0)
  {
    JsonDocument doc;
    deserializeJson(doc, message);

    const char *target = doc["targetState"];
    unsigned long duration_s = doc["duration"];

    if (duration_s > 0)
    {
      setMode(relayIndex, COUNTDOWN); // Kích hoạt chế độ Đếm Ngược
      relays[relayIndex].countdownTargetState = (strcmp(target, "ON") == 0);
      relays[relayIndex].countdownEndTime = millis() + (duration_s * 1000);

      // Đặt trạng thái ban đầu (ngược lại)
      setRelay(relayIndex, !relays[relayIndex].countdownTargetState);
    }
  }
}