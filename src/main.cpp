/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-lora-sensor-web-server/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <logo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <Preferences.h>
#include <SPIFFS.h>

#define SCK   5
#define MISO  19
#define MOSI  27
#define SS    18
#define RST   14
#define DIO0  26
#define BAND  868E6

#define OLED_SDA     4
#define OLED_SCL     15
#define OLED_RST     16
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// Default credentials — used only if nothing is saved in NVS
String wifiSSID = "Quarto";
String wifiPass  = "master1";

WiFiUDP     ntpUDP;
NTPClient   timeClient(ntpUDP);
String formattedDate, day, hour, timestamp;
int    rssi;
String loRaMessage, temperature, pressure, altitude, readingID;
bool   wifiConnected = false;

WebServer        server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
Preferences      preferences;

// ─── SPIFFS file helper ───────────────────────────────────────────────────────

void serveFile(const char* path, const char* mime) {
  File f = SPIFFS.open(path, "r");
  if (f) { server.streamFile(f, mime); f.close(); }
  else   { server.send(500, "text/plain", String(path) + " nao encontrado no SPIFFS"); }
}

// ─── Credential helpers ───────────────────────────────────────────────────────

void loadCredentials() {
  preferences.begin("wifi", true);
  if (preferences.isKey("ssid")) {
    wifiSSID = preferences.getString("ssid", wifiSSID);
    wifiPass  = preferences.getString("pass",  wifiPass);
    Serial.println("Credenciais NVS: " + wifiSSID);
  } else {
    Serial.println("Credenciais padrao: " + wifiSSID);
  }
  preferences.end();
}

void saveCredentials(const String& ssid, const String& pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

// JSON-safe escaping for SSID strings inside JSON string values
String escapeJson(const String& s) {
  String r;
  r.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if      (c == '"')  r += "\\\"";
    else if (c == '\\') r += "\\\\";
    else if (c == '\n') r += "\\n";
    else if (c == '\r') r += "\\r";
    else if (c == '\t') r += "\\t";
    else                r += c;
  }
  return r;
}

// ─── Config portal ────────────────────────────────────────────────────────────

void showConfigOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,  0); display.println("** WiFi Config **");
  display.setCursor(0, 14); display.println("Conecte ao WiFi:");
  display.setCursor(4, 26); display.println("LoRa-Config");
  display.setCursor(0, 40); display.println("Acesse no browser:");
  display.setCursor(4, 52); display.println("192.168.4.1");
  display.display();
}

void startConfigPortal() {
  Serial.println("Iniciando portal de configuracao WiFi...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("LoRa-Config");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  showConfigOLED();

  server.on("/", HTTP_GET, []() {
    serveFile("/config.html", "text/html");
  });

  server.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n && i < 15; i++) {
      if (i) json += ",";
      json += "{\"ssid\":\"" + escapeJson(WiFi.SSID(i)) +
              "\",\"rssi\":"  + String(WiFi.RSSI(i)) +
              ",\"enc\":"     + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    WiFi.scanDelete();
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/save", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("pass");

    if (newSSID.length() == 0) {
      server.send(400, "text/plain", "SSID invalido");
      return;
    }

    saveCredentials(newSSID, newPass);
    server.send(200, "text/plain", "OK");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,  0); display.println("Credenciais salvas!");
    display.setCursor(0, 16); display.println(newSSID);
    display.setCursor(0, 32); display.println("Reiniciando...");
    display.display();

    delay(1500);
    ESP.restart();
  });

  server.begin();

  while (true) {
    server.handleClient();
    delay(5);
  }
}


// ─── OLED ─────────────────────────────────────────────────────────────────────

void startOLED() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("LORA RECEIVER");
  display.display();
}

// ─── LoRa ─────────────────────────────────────────────────────────────────────

void startLoRA() {
  int counter = 0;
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  while (!LoRa.begin(BAND) && counter < 10) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 10) {
    Serial.println("Starting LoRa failed!");
  }
  Serial.println("LoRa Initialization OK!");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("LoRa Initializing OK!");
  display.display();
  delay(2000);
}

// ─── WiFi ─────────────────────────────────────────────────────────────────────

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

  display.clearDisplay();
  display.setCursor(0,  0); display.print("Conectando WiFi...");
  display.setCursor(0, 14); display.print(wifiSSID);
  display.setCursor(0, 28); display.print("Aguarde...");
  display.display();

  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("\nWiFi nao conectado. Iniciando portal de configuracao.");
    display.clearDisplay();
    display.setCursor(0,  0); display.print("WiFi falhou:");
    display.setCursor(0, 14); display.print(wifiSSID);
    display.setCursor(0, 28); display.print("Iniciando config...");
    display.display();
    delay(2000);
    return;
  }

  wifiConnected = true;
  Serial.println("\nWiFi conectado: " + WiFi.localIP().toString());
  display.clearDisplay();
  display.setCursor(0,  0); display.print("WiFi conectado!");
  display.setCursor(0, 14); display.print(wifiSSID);
  display.setCursor(0, 28); display.print("IP: ");
  display.print(WiFi.localIP());
  display.setCursor(0, 42); display.print("http://");
  display.print(WiFi.localIP());
  display.display();
  delay(2000);
}

// ─── LoRa data & NTP ─────────────────────────────────────────────────────────

String getJsonValue(String data, String key) {
  String searchKey = "\"" + key + "\":";
  int startIndex = data.indexOf(searchKey);
  if (startIndex == -1) return "";
  startIndex += searchKey.length();
  while (startIndex < (int)data.length() && data.charAt(startIndex) == ' ') startIndex++;
  bool quoted = data.charAt(startIndex) == '"';
  if (quoted) startIndex++;
  int endIndex = startIndex;
  while (endIndex < (int)data.length()) {
    char c = data.charAt(endIndex);
    if (quoted && c == '"') break;
    if (!quoted && (c == ',' || c == '}')) break;
    endIndex++;
  }
  return data.substring(startIndex, endIndex);
}

void getLoRaData() {
  Serial.print("Lora packet received: ");
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    Serial.print(LoRaData);
    readingID   = getJsonValue(LoRaData, "id");
    temperature = getJsonValue(LoRaData, "temperatura");
    pressure    = getJsonValue(LoRaData, "pressao");
    altitude    = getJsonValue(LoRaData, "altitude");
  }
  rssi = LoRa.packetRssi();
  Serial.print(" with RSSI ");
  Serial.println(rssi);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,  0); display.print("Pacote recebido");
  display.setCursor(0, 12); display.print("ID: ");    display.print(readingID);
  display.setCursor(0, 24); display.print("Temp: ");  display.print(temperature); display.print(" C");
  display.setCursor(0, 36); display.print("Press: "); display.print(pressure);    display.print(" Pa");
  display.setCursor(0, 48); display.print("Alt: ");   display.print(altitude);    display.print(" m");
  display.display();
}

void getTimeStamp() {
  unsigned long ntpStart = millis();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    if (millis() - ntpStart > 5000) {
      Serial.println("NTP timeout.");
      return;
    }
  }
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime(&epochTime);
  char buf[25];
  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
          ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  formattedDate = String(buf);
  int splitT = formattedDate.indexOf("T");
  day       = formattedDate.substring(0, splitT);
  hour      = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  timestamp = day + " " + hour;
}

// ─── Web server routes ────────────────────────────────────────────────────────

void startServer() {
  server.on("/",            HTTP_GET, []() { serveFile("/index.html", "text/html"); });
  server.on("/temperature", HTTP_GET, []() { server.send(200, "text/plain", temperature); });
  server.on("/pressure",    HTTP_GET, []() { server.send(200, "text/plain", pressure); });
  server.on("/altitude",    HTTP_GET, []() { server.send(200, "text/plain", altitude); });
  server.on("/timestamp",   HTTP_GET, []() { server.send(200, "text/plain", timestamp); });
  server.on("/rssi",        HTTP_GET, []() { server.send(200, "text/plain", String(rssi)); });
  server.on("/readingid",   HTTP_GET, []() { server.send(200, "text/plain", readingID); });
  server.begin();
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  startOLED();
  logo();
  delay(2000);

  if (!SPIFFS.begin(true)) {
    Serial.println("ERRO: falha ao montar SPIFFS");
  }

  loadCredentials();
  startLoRA();
  connectWiFi();

  if (wifiConnected) {
    startServer();
    timeClient.begin();
    timeClient.setTimeOffset(0);
    display.clearDisplay();
    display.setCursor(0,  0); display.print("LoRa Receiver OK");
    display.setCursor(0, 12); display.print("Aguardando pacote");
    display.display();
    Serial.println("Setup OK. Aguardando pacotes LoRa...");
  } else {
    startConfigPortal(); // blocks until save + restart
  }
}

void loop() {
  if (wifiConnected) {
    server.handleClient();
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Packet size: ");
    Serial.println(packetSize);
    getLoRaData();
    if (wifiConnected) {
      getTimeStamp();
    }
  }
}
