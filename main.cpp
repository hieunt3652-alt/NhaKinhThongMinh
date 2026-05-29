#include <Arduino.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 

// ================= CẤU HÌNH MẠNG WIFI (THAY ĐỔI TẠI ĐÂY) =================
const char* ssid = "-_-";         
const char* password = "0386797327";       
const char* mqtt_server = "broker.hivemq.com";   

const char* topic_data = "greenhouse/data";       
const char* topic_control = "greenhouse/control"; 
// ===========================================================================

// ================= KEEP OLD CODE: CHÂN CẢM BIẾN =================
#define DHTPIN 6     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);
const int CHANAS = 4; 
const int CHANMUA = 5;
const int analogPinDA = 7;  
// ===================================================================

// ---- ĐỊNH NGHĨA CHÂN THIẾT BỊ ĐIỀU KHIỂN ----
const int PIN_QUAT = 9;
const int PIN_SUOI = 10;
const int PIN_LED_TEMP = 11;
const int PIN_BOM = 12;
const int PIN_LED_HUMI = 13;
const int PIN_LED_MUA = 14;
const int PIN_DEN_AS = 20;

// ---- ĐỊNH NGHĨA CHÂN LCD VÀ NÚT BẤM ----
const int PIN_NUT_BAM = 40;
const int PIN_SDA = 41;
const int PIN_SCL = 42;

LiquidCrystal_I2C lcd(0x27, 16, 2); 

// ---- CÁC BIẾN LƯU TRẠNG THÁI ----
int manHinhHienTai = 1;     
bool trangThaiNutTruoc = HIGH; 
unsigned long thoiGianGuiWeb = 0; 

// BIẾN QUẢN LÝ CHẾ ĐỘ: 1 = TỰ ĐỘNG (AUTO), 0 = THỦ CÔNG (MANUAL)
int cheDoHeThong = 1; 

WiFiClient espClient;
PubSubClient client(espClient);

// ================= HÀM XỬ LÝ LỆNH ĐIỀU KHIỂN TỪ WEB GỬI XUỐNG =================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) { message += (char)payload[i]; }
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) return;

  // 1. Kiểm tra lệnh đổi chế độ
  if (doc.containsKey("mode")) {
    cheDoHeThong = doc["mode"];
    lcd.clear(); 
  }

  // 2. Nếu ở chế độ THỦ CÔNG, cho phép bật tắt thiết bị đơn lẻ
  if (cheDoHeThong == 0 && doc.containsKey("device")) {
    String device = doc["device"];
    int state = doc["state"];
    
    if (device == "QUAT") { digitalWrite(PIN_QUAT, state); }
    else if (device == "SUOI") { digitalWrite(PIN_SUOI, state); }
    else if (device == "BOM") { digitalWrite(PIN_BOM, state); }
    else if (device == "DEN") { digitalWrite(PIN_DEN_AS, state); }
  }
}

void setup() {
  // ================= KEEP OLD CODE =================
  Serial.begin(115200);
  pinMode(CHANAS, INPUT);
  pinMode(CHANMUA, INPUT);
  Serial.println("--- Bat dau doc cam bien  ---");
  // =================================================

  pinMode(PIN_QUAT, OUTPUT);
  pinMode(PIN_SUOI, OUTPUT);
  pinMode(PIN_LED_TEMP, OUTPUT);
  pinMode(PIN_BOM, OUTPUT);
  pinMode(PIN_LED_HUMI, OUTPUT);
  pinMode(PIN_LED_MUA, OUTPUT);
  pinMode(PIN_DEN_AS, OUTPUT);
  pinMode(PIN_NUT_BAM, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); 
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect("ESP32_Greenhouse_AnHoa")) {
        client.subscribe(topic_control); 
      }
    }
    client.loop();
  }

  // ================= KEEP OLD CODE: ĐỌC GIÁ TRỊ CẢM BIẾN =================
  int giaTrias = analogRead(CHANAS);
  int GTMUA = analogRead(CHANMUA);
  int DOIGTMUA = map(GTMUA, 4095, 0, 0, 100);
  int DOIGTAS = map(giaTrias, 4095, 0, 0, 100);
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    delay(100);
    return;
  }

  int analogValue = analogRead(analogPinDA);
  int percentHumidity = map(analogValue, 4095, 1500, 0, 100);
  percentHumidity = constrain(percentHumidity, 0, 100);
  // ===================================================================

  // ============= PHẦN XỬ LÝ LOGIC ĐIỀU KHIỂN (CHỈ CHẠY KHI Ở CHẾ ĐỘ AUTO) =============
  if (cheDoHeThong == 1) {
    if (t > 31) { digitalWrite(PIN_QUAT, HIGH); } else { digitalWrite(PIN_QUAT, LOW); }
    if (t < 28) { digitalWrite(PIN_SUOI, HIGH); } else { digitalWrite(PIN_SUOI, LOW); }
    if (t >= 28 && t <= 31) { digitalWrite(PIN_LED_TEMP, HIGH); } else { digitalWrite(PIN_LED_TEMP, LOW); }

    if (percentHumidity < 20) {
      digitalWrite(PIN_BOM, HIGH);      
      digitalWrite(PIN_LED_HUMI, LOW);   
    } else {
      digitalWrite(PIN_BOM, LOW);       
      digitalWrite(PIN_LED_HUMI, HIGH);  
    }

    if (DOIGTMUA > 10) { digitalWrite(PIN_LED_MUA, HIGH); } else { digitalWrite(PIN_LED_MUA, LOW); }
    if (DOIGTAS < 30) { digitalWrite(PIN_DEN_AS, HIGH); } else { digitalWrite(PIN_DEN_AS, LOW); }
  }

  // ============= GỬI ĐỒNG BỘ LÊN WEB (MỖI 2 GIÂY) =============
  if (millis() - thoiGianGuiWeb > 2000 && client.connected()) {
    thoiGianGuiWeb = millis();
    
    int q = digitalRead(PIN_QUAT);
    int s = digitalRead(PIN_SUOI);
    int b = digitalRead(PIN_BOM);
    int d = digitalRead(PIN_DEN_AS);

    String payload = "{\"temp\":" + String(t) + 
                     ",\"humiSoil\":" + String(percentHumidity) + 
                     ",\"light\":" + String(DOIGTAS) + 
                     ",\"rain\":" + String(DOIGTMUA) + 
                     ",\"mode\":" + String(cheDoHeThong) +
                     ",\"st_quat\":" + String(q) + 
                     ",\"st_suoi\":" + String(s) + 
                     ",\"st_bom\":" + String(b) + 
                     ",\"st_den\":" + String(d) + "}";
    client.publish(topic_data, payload.c_str());
  }

  // ================= KEEP OLD CODE: NÚT BẤM CHUYỂN MÀN HÌNH =================
  bool trangThaiNutHienTai = digitalRead(PIN_NUT_BAM);
  if (trangThaiNutTruoc == HIGH && trangThaiNutHienTai == LOW) {
    manHinhHienTai++;
    if (manHinhHienTai > 3) { manHinhHienTai = 1; }
    lcd.clear(); 
    delay(50);   
  }
  trangThaiNutTruoc = trangThaiNutHienTai;

  // ================= KEEP OLD CODE: ĐIỀU KHIỂN HIỂN THỊ LCD =================
  if (manHinhHienTai == 1) {
    lcd.setCursor(0, 0); lcd.print("ND:"); lcd.print((int)t); lcd.print("C  ");
    lcd.setCursor(9, 0); lcd.print("MUA:"); lcd.print(DOIGTMUA); lcd.print("% ");
    lcd.setCursor(0, 1); lcd.print("DA:"); lcd.print(percentHumidity); lcd.print("%  ");
    lcd.setCursor(9, 1); lcd.print("AS:"); lcd.print(DOIGTAS); lcd.print("% ");
  } 
  else if (manHinhHienTai == 2) {
    int st_quat = (digitalRead(PIN_QUAT) == HIGH) ? 1 : 0;
    int st_suoi = (digitalRead(PIN_SUOI) == HIGH) ? 1 : 0;
    int st_bom  = (digitalRead(PIN_BOM) == HIGH) ? 1 : 0;
    int st_den  = (digitalRead(PIN_DEN_AS) == HIGH) ? 1 : 0;

    lcd.setCursor(0, 0);
    lcd.print("Q:"); lcd.print(st_quat);
    lcd.print(" S:"); lcd.print(st_suoi);
    lcd.print(" B:"); lcd.print(st_bom);
    lcd.print(" D:"); lcd.print(st_den);

    lcd.setCursor(0, 1);
    if (cheDoHeThong == 1) { lcd.print("MODE: AUTOMATIC "); } 
    else { lcd.print("MODE: MANUAL    "); }
  } 
  else if (manHinhHienTai == 3) {
    lcd.setCursor(0, 0); lcd.print("                ");
    lcd.setCursor(0, 1); lcd.print("                ");
  }

  delay(50); 
}