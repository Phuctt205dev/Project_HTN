#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Chân điều khiển đèn giao thông
#define RED1_PIN    12
#define YELL1_PIN   14
#define GREEN1_PIN  13
#define RED2_PIN    25
#define YELL2_PIN   33
#define GREEN2_PIN  32

// WiFi & MQTT
const char* ssid = "Jamashinaide";
const char* password = "6783250605";
const char* mqtt_server = "172.20.10.2";

WiFiClient espClient;
PubSubClient client(espClient);

// Biến đếm xe từ MQTT
volatile int vehicles_road1 = 0;
volatile int vehicles_road2 = 0;

// Thời gian đèn
const int MIN_GREEN_TIME = 10;  // Thời gian xanh tối thiểu (giây)
const int MAX_GREEN_TIME = 30;  // Thời gian xanh tối đa (giây)
const int YELLOW_TIME = 3;     // Thời gian vàng cố định (giây)

int greenDuration1 = MIN_GREEN_TIME;
int greenDuration2 = MIN_GREEN_TIME;

// Trạng thái đèn
enum State { GREEN1, YELLOW1, GREEN2, YELLOW2 };
State currentState = GREEN1;
unsigned long stateStartTime = 0;

void setupPins() {
  pinMode(RED1_PIN, OUTPUT);
  pinMode(YELL1_PIN, OUTPUT);
  pinMode(GREEN1_PIN, OUTPUT);
  pinMode(RED2_PIN, OUTPUT);
  pinMode(YELL2_PIN, OUTPUT);
  pinMode(GREEN2_PIN, OUTPUT);
}

void setLights(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  digitalWrite(RED1_PIN, r1);
  digitalWrite(YELL1_PIN, y1);
  digitalWrite(GREEN1_PIN, g1);
  digitalWrite(RED2_PIN, r2);
  digitalWrite(YELL2_PIN, y2);
  digitalWrite(GREEN2_PIN, g2);
}

void calculateGreenTimes() {
  // Tránh chia cho 0 bằng cách thêm 1
  float ratio = (vehicles_road1 + 1.0) / (vehicles_road2 + 1.0);
  
  // Điều chỉnh thời gian xanh dựa trên tỉ lệ xe
  if (ratio > 1.5) {  // Nhiều xe ở đường 1 hơn đáng kể
    greenDuration1 = min(MAX_GREEN_TIME, (int)(MIN_GREEN_TIME * ratio));
    greenDuration2 = MIN_GREEN_TIME;
  } 
  else if (ratio < 0.67) {  // Nhiều xe ở đường 2 hơn đáng kể
    greenDuration2 = min(MAX_GREEN_TIME, (int)(MIN_GREEN_TIME / ratio));
    greenDuration1 = MIN_GREEN_TIME;
  } 
  else {  // Số xe tương đương
    greenDuration1 = MIN_GREEN_TIME + 5;  // Thêm 5s cho cả 2 bên
    greenDuration2 = MIN_GREEN_TIME + 5;
  }

  // Đảm bảo không vượt quá thời gian tối đa
  greenDuration1 = constrain(greenDuration1, MIN_GREEN_TIME, MAX_GREEN_TIME);
  greenDuration2 = constrain(greenDuration2, MIN_GREEN_TIME, MAX_GREEN_TIME);

  Serial.print("Adjusted Times - Road1: ");
  Serial.print(greenDuration1);
  Serial.print("s, Road2: ");
  Serial.println(greenDuration2);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  
  if (String(topic) == "vehicles/road1") {
    vehicles_road1 = msg.toInt();
    Serial.print("Road1 vehicles updated: ");
    Serial.println(vehicles_road1);
  } 
  else if (String(topic) == "vehicles/road2") {
    vehicles_road2 = msg.toInt();
    Serial.print("Road2 vehicles updated: ");
    Serial.println(vehicles_road2);
  }
  
  // Tính toán lại thời gian khi có cập nhật số lượng xe
  calculateGreenTimes();
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Central")) {
      client.subscribe("vehicles/road1");
      client.subscribe("vehicles/road2");
      client.subscribe("control/manual");
    } else {
      delay(1000);
    }
  }
}

void publishTrafficData() {
  StaticJsonDocument<200> doc1, doc2;
  
  // Dữ liệu đường 1
  switch(currentState) {
    case GREEN1:
      doc1["light"] = "green";
      doc1["time"] = greenDuration1 - (millis() - stateStartTime) / 1000;
      doc1["vehicles"] = vehicles_road1;
      break;
    case YELLOW1:
      doc1["light"] = "yellow";
      doc1["time"] = YELLOW_TIME - (millis() - stateStartTime) / 1000;
      doc1["vehicles"] = vehicles_road1;
      break;
    default:
      doc1["light"] = "red";
      doc1["time"] = (currentState == GREEN2 ? greenDuration2 : YELLOW_TIME) + 
                     (currentState == YELLOW2 ? 0 : YELLOW_TIME) - 
                     (millis() - stateStartTime) / 1000;
      doc1["vehicles"] = vehicles_road1;
  }
  
  // Dữ liệu đường 2
  switch(currentState) {
    case GREEN2:
      doc2["light"] = "green";
      doc2["time"] = greenDuration2 - (millis() - stateStartTime) / 1000;
      doc2["vehicles"] = vehicles_road2;
      break;
    case YELLOW2:
      doc2["light"] = "yellow";
      doc2["time"] = YELLOW_TIME - (millis() - stateStartTime) / 1000;
      doc2["vehicles"] = vehicles_road2;
      break;
    default:
      doc2["light"] = "red";
      doc2["time"] = (currentState == GREEN1 ? greenDuration1 : YELLOW_TIME) + 
                     (currentState == YELLOW1 ? 0 : YELLOW_TIME) - 
                     (millis() - stateStartTime) / 1000;
      doc2["vehicles"] = vehicles_road2;
  }
  
  String output1, output2;
  serializeJson(doc1, output1);
  serializeJson(doc2, output2);
  
  client.publish("traffic/road1", output1.c_str());
  client.publish("traffic/road2", output2.c_str());
}

void setup() {
  Serial.begin(115200);
  setupPins();
  setLights(LOW, LOW, HIGH, HIGH, LOW, LOW); // Khởi tạo đèn

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  calculateGreenTimes(); // Tính toán thời gian ban đầu
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long currentMillis = millis();
  static unsigned long lastPublishTime = 0;

  switch (currentState) {
    case GREEN1:
      if (stateStartTime == 0) {
        setLights(LOW, LOW, HIGH, HIGH, LOW, LOW);
        stateStartTime = currentMillis;
      }
      if (currentMillis - stateStartTime >= greenDuration1 * 1000) {
        currentState = YELLOW1;
        stateStartTime = 0;
      }
      break;

    case YELLOW1:
      if (stateStartTime == 0) {
        setLights(LOW, HIGH, LOW, HIGH, LOW, LOW);
        stateStartTime = currentMillis;
      }
      if (currentMillis - stateStartTime >= YELLOW_TIME * 1000) {
        currentState = GREEN2;
        stateStartTime = 0;
        calculateGreenTimes(); // Tính toán lại khi chuyển pha
      }
      break;

    case GREEN2:
      if (stateStartTime == 0) {
        setLights(HIGH, LOW, LOW, LOW, LOW, HIGH);
        stateStartTime = currentMillis;
      }
      if (currentMillis - stateStartTime >= greenDuration2 * 1000) {
        currentState = YELLOW2;
        stateStartTime = 0;
      }
      break;

    case YELLOW2:
      if (stateStartTime == 0) {
        setLights(HIGH, LOW, LOW, LOW, HIGH, LOW);
        stateStartTime = currentMillis;
      }
      if (currentMillis - stateStartTime >= YELLOW_TIME * 1000) {
        currentState = GREEN1;
        stateStartTime = 0;
        calculateGreenTimes(); // Tính toán lại khi chuyển pha
      }
      break;
  }

  // Gửi dữ liệu mỗi giây
  if (currentMillis - lastPublishTime >= 1000) {
    publishTrafficData();
    lastPublishTime = currentMillis;
  }
}