#include <WiFi.h>
#include "time.h"
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_MCP3008.h>
 
const char* ntpServer = "pool.ntp.org"; //ntp server yang dapat digunakan hanya yang internasional (gmt), tidak bisa local seperti server indonesia (gmt+7)
const long gmtOffset = 25200; //gmt+7 in seconds
const int daylightOffset = 0; //indonesia tidak punya daylight offset
 
OneWire onewire(23);
DallasTemperature sensors(&onewire);
 
Adafruit_TCS34725 TCS = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);
uint16_t color[4];
const int phSample[7][4] =
{
  {382,  356,  210,  4},
  {345,  335,  202,  5},
  {314,  359,  258,  6},
  {241,  280,  193,  7},
  {208,  254,  189,  8},
  {187,  234,  190,  9},
  {184,  232,  199,  10}
}; //data sampel RGB dari setiap warna di strip pH, diambil dari color training
int phSize = sizeof(phSample)/sizeof(phSample[0]);
 
Adafruit_MCP3008 ADC;
 
const char* ssid = "ARDYAN";
const char* password = "L4wr3nc30610";
const char* user = "mqtt_1000000212_7021f455-9dfd-4f48-9746-2ffebd82fdb5";
const char* pass = "ki4cqivdbl1vwlgd";
const char* server = "mqtt.iotera.io";
const char* topic = "iotera/pub/1000000212/7021f455-9dfd-4f48-9746-2ffebd82fdb5/data";
String orgMsg;
 
const int microsecsMultiply = 60000000; //multiplier seconds to microseconds untuk deep sleep
const int deepsleepDuration = 10; //durasi deep sleep dalam menit
 
 
WiFiClient espClient;
PubSubClient client(espClient);
 
int ph = 0;
float temp = 0.0;
int turbid = 0;
float light = 0.0;
float volt = 0.0;
float battPercent = 0.0;
 
void setup() {
  TCS.begin();
  ADC.begin(19, 17, 18, 16); //MCP3008 menggunakan SPI Software karena pin SPI Hardware sudah digunakan untuk sensor lain
  Serial.begin(115200);
 
  WiFi.begin(ssid, password);
  int timeoutWiFi = millis(); //terdapat timeout ketika mau koneksi ke WiFi (60 dtk), saat waktu sudah habis maka ESP32 akan restart. Sehingga jika belum terhubung ke WiFi belum bisa lanjut
  Serial.print("Connecting to network");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
    if(millis() - timeoutWiFi >= 60000) {
      ESP.restart();
    }
  }
  Serial.print("\nNETWORK CONNECTED to "); //jika berhasil akan menampilkan nama network yang digunakan dan IP address dari ESP32
  Serial.print(ssid);
  Serial.print(", IP Addr: ");
  Serial.println(WiFi.localIP());
 
  configTime(gmtOffset, daylightOffset, ntpServer); //mengambil waktu saat ini melalui ntp server. Terdapat timeout, jika waktu habis maka ESP32 akan restart.
  struct tm timeinfo;
  Serial.print("Getting current datetime");
  int timeoutNTP = millis();
  while(!getLocalTime(&timeinfo)){
    Serial.print(".");
    if(millis() - timeoutNTP >= 60000) {
      ESP.restart();
    }
  }
  Serial.println(&timeinfo, "\nCurrent datetime is %d %B %Y %H:%M:%S"); //menampilkan waktu saat ini yang diperoleh dari ntp server
   
  client.setServer(server, 1883);
 
  Serial.print("Connecting to IOTERA Broker"); //mencoba terhubung ke mqtt broker. Terdapat timeout, jika waktu habis maka ESP32 akan restart.
  int timeoutServer = millis();
  while(!client.connected()) {
    client.connect("1000000212", user, pass);
    Serial.print('.');
    if(millis() - timeoutServer >= 60000) {
      ESP.restart();
    }
  }
  Serial.print("\nCONNECTED TO: "); //setelah berhasil terhubung, menampilkan address dari broker yang digunakan
  Serial.println(server);
 
  ph = identifyPH(); //mengambil nilai pH dari function identifyPH
  temp = getTemp(); //mengambil nilai suhu dari function getTemp
  turbid = getTurbidity(); //mengambil nilai turbiditas dari function getTurbidity
  light = getLux(); //mengambil nilai intensitas cahaya dari function getLux
  volt = getVolt(); //mengambil nilai voltase baterai dari function getVolt
  battPercent = getBatPercent(); //mengambil nilai persentase baterai dari function getBatPercent
 
  //pesan yang akan dikirimkan menggunakan mqtt harus menggunakan tipe data char, sehingga semua angka dikonversi dari float/int ke string, kemudian dari string ke array of char (menggunakan format JSON)
  orgMsg = "{\"payload\": [{\"sensor\":\"phamSensor\",\"param\":\"ph\",\"value\":" + String(ph) + "}, {\"sensor\":\"tempSensor\",\"param\":\"temp\",\"value\":" + String(temp) + "}, {\"sensor\":\"turbiditySensor\",\"param\":\"turbid\",\"value\":" + String(turbid) + "}, {\"sensor\":\"lightSensor\",\"param\":\"light\",\"value\":" + String(light) + "}, {\"sensor\":\"battVoltage\",\"param\":\"volt\",\"value\":" + String(volt) + "}, {\"sensor\":\"battPercent\",\"param\":\"percentage\",\"value\":" + String(battPercent) + "}]}";
  int msgSize = orgMsg.length() + 1;
  char msg[msgSize];
  orgMsg.toCharArray(msg, msgSize);
   
  client.publish("iotera/pub/1000000212/7021f455-9dfd-4f48-9746-2ffebd82fdb5/data", msg); //pesan dikirimkan ke broker menggunakan topik yang sudah disediakan
  client.disconnect(); //JANGAN LUPA UNTUK DISCONNECT, antisipasi next connection bisa gagal
 
  //tampilkan semua data yang sudah diperoleh ESP32
  Serial.print("pH: ");
  Serial.println(ph);
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" °C");
  Serial.print("Kejernihan: ");
  Serial.print(turbid);
  Serial.println("%");
  Serial.print("Light: ");
  Serial.print(light);
  Serial.println(" lx");
  Serial.print("Voltage: ");
  Serial.print(volt);
  Serial.println(" v");
  Serial.print("Battery percentage: ");
  Serial.print(battPercent);
  Serial.println("%");
 
     
  Serial.println("Countdown to Deepsleep..."); //start countdown untuk deepsleep (10 detik)
  for(int i=10; i>=1; i--)
  {
    Serial.println(i);
    delay(1000);
  }
  esp_sleep_enable_timer_wakeup(microsecsMultiply * deepsleepDuration); //set timer untuk deepsleep selama yang ditentukan di awal
  Serial.println("ENTERING DEEPSLEEP...");
  esp_deep_sleep_start(); //masuk ke state deepsleep
}
 
void loop() {
  // put your main code here, to run repeatedly:
   
}
 
int identifyPH() {
  TCS.getRawData(&color[0], &color[1], &color[2], &color[3]); //mengambil nilai RGB dalam bentuk unsigned integer 16 bit, jika tidak raw menggunakan int 8 bit (0-255), raw dipilih karena range 16 bit lebih luas sehingga akuarsi bisa lebih tinggi
  int falsecount = 0, minDis = 0, listProbability[7][2] = {{}}, listDistance[7] = {}, size = 0, colorDistance = 0;
  int maxDist = listDistance[0];
  for(int i=0; i<phSize; i++)
  {
    colorDistance = getDistance(color[0], color[1], color[2], phSample[i][0], phSample[i][1], phSample[i][2]);
    if(colorDistance < 30) {
      listProbability[size][0] = colorDistance;
      listProbability[size][1] = phSample[i][3];
      listDistance[size] = colorDistance;
      size++;
    }
  }
   
  if(size != 0) {
    for(int j=0; j<size; j++) {
      maxDist = max(listDistance[j], maxDist);
    }
    for(int k=0; k<size; k++)
    {
      if(listProbability[k][0] == maxDist) {
        return listProbability[k][1];
      }
    }
  }
  else {
    return -1;
  }
}
 
//function mencari jarak dari satu warna ke warna lain
int getDistance(int rSens, int gSens, int bSens, int rSample, int gSample, int bSample) {
  return sqrt(pow(rSens - rSample, 2) + pow(gSens - gSample, 2) + pow(bSens - bSample, 2));
}
 
float getTemp() {
  float temp = 0;
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);
  return temp;
}
 
int getTurbidity() {
  int analog = 0, mapped = 0, cons = 0;
  for(int i=0; i<1000; i++)
  {
    analog += ADC.readADC(1);
  }
  analog = analog / 1000;
  mapped = map(analog, 0, 966, 0, 100);
  cons = constrain(mapped, 0, 100);
  return cons;
}
 
float getLux() {
  float voltage = 0, current = 0, microCurr = 0, lux = 0;
  for(int i=0; i<1000; i++) {
    voltage += ADC.readADC(2) * (5.083 / 1023.0);
  }
  voltage = voltage / 1000;
  current = voltage / 10000;
  microCurr = current * 1000000;
  lux = microCurr * 2;
  return lux;
}
 
float getVolt() {
  float voltage = 0;
  for(int i=0; i<1000; i++) {
    voltage +=  ADC.readADC(3) * (5.083 / 1023.0);
  }
  voltage = voltage / 1000;
  return voltage;
}
 
float getBatPercent() {
  int percent = batMap(volt, 3.00, 4.19, 0, 100);
  percent = constrain(percent, 0, 100);
  return percent;
}
 
//custom map function dikarenakan built-in map menggunakan tipe data long, sedangkan data yang digunakan adalah float
long batMap(float x, float in_min, float in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}