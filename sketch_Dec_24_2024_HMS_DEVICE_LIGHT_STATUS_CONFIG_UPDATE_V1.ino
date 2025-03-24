#include <WiFi.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
// Global objects
HTTPClient http;
WiFiManager wifiManager;
WebServer server(80);
DynamicJsonDocument jsonConfig(1024);

// Pin and configuration variables
const int switchPin = 3;
String serialNumber = "ESP32T001";  // Default Serial Number
String serverURL, wifiSSID, wifiPassword;
String temperatureThreshold = "30";
int previousSwitchState = HIGH;
bool shouldSaveConfig = false;
String socketConnectionStatus = "Disconencted";

// Replace with your server's IP address and port
String server_ip = "";    // Server IP address
String server_port = "";  // Server port number

String gmtTimeZone = "";

// IP configuration
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
String deviceIPaddress = "";
unsigned long previousHeartbeatMillis = 0;  // Stores last time heartbeat was sent
long intervalHeartbeat = 10;                // Interval at which to send heartbeat (10 seconds)
// Function declarations
bool saveConfig();
bool loadConfig();
void saveConfigCallback();
void handleRoot();
void handleSaveConfig();
void sendSwitchStatus(int status);
void wifiManagerSetup();
void listLittleFSFiles();
void handleConfigJson();
void connectToWiFi();
void setStaticIP();
void safeRestart();
bool socketConnectServer();
void socketDeviceHeartBeatToServer();





WiFiClient client;  // Create a client object

void setup() {
  Serial.begin(115200);  // Start the Serial communication at 115200 baud rate
  delay(1000);

  pinMode(switchPin, INPUT_PULLUP);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS. Restarting...");
    safeRestart();
  }

  if (!loadConfig()) {
    Serial.println("Error loading config. Resetting WiFi credentials.");
    wifiManager.resetSettings();
  }

  connectToWiFi();

  Serial.println("Web server started at: " + WiFi.localIP().toString());

  // Initialize Web Server
  server.on("/", handleRoot);
  server.on("/save_config", HTTP_POST, handleSaveConfig);
  server.on("/config.json", HTTP_GET, handleConfigJson);
  server.on("/logo", HTTP_GET, handleLogoImage);
  server.begin();

/*

// Store device name and manufacturer in EEPROM
  EEPROM.write(deviceNameStart, 'M');
  EEPROM.write(deviceNameStart + 1, 'y');
  EEPROM.write(deviceNameStart + 2, 'A');
  EEPROM.write(deviceNameStart + 3, 'r');
  EEPROM.write(deviceNameStart + 4, 'd');
  EEPROM.write(deviceNameStart + 5, 'u');
  EEPROM.write(deviceNameStart + 6, 'i');
  EEPROM.write(deviceNameStart + 7, 'n');
  EEPROM.write(deviceNameStart + 8, 'o');
  
  EEPROM.write(manufacturerStart, 'M');
  EEPROM.write(manufacturerStart + 1, 'y');
  EEPROM.write(manufacturerStart + 2, 'C');
  EEPROM.write(manufacturerStart + 3, 'o');
  EEPROM.write(manufacturerStart + 4, 'm');
  EEPROM.write(manufacturerStart + 5, 'p');
  EEPROM.write(manufacturerStart + 6, 'a');
  EEPROM.write(manufacturerStart + 7, 'n');
  EEPROM.write(manufacturerStart + 8, 'y');

  // Print stored values
  String deviceName = "";
  for (int i = deviceNameStart; i < deviceNameStart + 9; i++) {
    deviceName += char(EEPROM.read(i));
  }

  String manufacturer = "";
  for (int i = manufacturerStart; i < manufacturerStart + 9; i++) {
    manufacturer += char(EEPROM.read(i));
  }*/

  socketConnectServer();
}

void loop() {
  server.handleClient();
  handleSwitchState();
  handleHeartbeat();
  delay(1000);  // Non-blocking delay
}

void handleSwitchState() {
  int switchState = digitalRead(switchPin);
  if (switchState != previousSwitchState) {
    sendSwitchStatus(switchState);
    previousSwitchState = switchState;
  }
}

void handleHeartbeat() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousHeartbeatMillis >= intervalHeartbeat * 1000) {
    previousHeartbeatMillis = currentMillis;
    socketDeviceHeartBeatToServer();
  }
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");

  if (!wifiSSID.isEmpty() && !wifiPassword.isEmpty()) {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    int retryCount = 0;

    while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi!");
      if(!wifiSSID.isEmpty())
      {
      
      updateJsonConfig("config.json", "wifiSSID", wifiSSID.c_str());
      //updateJsonConfig("config.json", "wifiPassword", wifiPassword.c_str());
      }
      updateJsonConfig("config.json", "wifiConnection", "Connected");
      ///setStaticIP();
    } else {
      Serial.println("Failed to connect. Starting WiFiManager...");
      updateJsonConfig("config.json", "wifiConnection", "Disconnected");
      wifiManagerSetup();
    }
  } else {
    wifiManagerSetup();
  }
}

void setStaticIP() {
  IPAddress currentIP = WiFi.localIP();
  IPAddress newIP(currentIP[0], currentIP[1], currentIP[2], 100);
  IPAddress newGateway(currentIP[0], currentIP[1], 1, 1);

  if (!WiFi.config(newIP, newGateway, subnet)) {
    Serial.println("Failed to configure static IP");
  } else {
    Serial.println("New Static IP: " + WiFi.localIP().toString());
    deviceIPaddress = WiFi.localIP().toString();
  }
}

void wifiManagerSetup() {
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  if (!wifiManager.autoConnect(serialNumber.c_str())) {
    Serial.println("WiFiManager failed. Restarting...");
    delay(3000);
    safeRestart();
  }

  wifiSSID = WiFi.SSID();
  wifiPassword = "";  // WiFi password is typically not retrievable

  if (shouldSaveConfig) {
    saveConfig();
  }
}

void sendSwitchStatus(int status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send switch status.");
    return;
  }

  DynamicJsonDocument swtchDoc(1024);
  swtchDoc["serialNumber"] = serialNumber;
  swtchDoc["type"] = "switchStatus";
  //swtchDoc["config"] = jsonConfig;
  swtchDoc["switchStatus"] = String(status);

  String swichData;
  serializeJson(swtchDoc, swichData);

  client.println(swichData);
  Serial.println("Sent Swtch Data: " + swichData);
 Serial.println("serverURL: " + String(serverURL));
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData ="serial_number=" + serialNumber + "&room_number=" + serialNumber + "&status=" + String(status);
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
      Serial.println("HTTP Response code: " + String(httpResponseCode));
  } else {
      Serial.println("Error in HTTP POST: " + String(httpResponseCode));
  }

  http.end();
}
