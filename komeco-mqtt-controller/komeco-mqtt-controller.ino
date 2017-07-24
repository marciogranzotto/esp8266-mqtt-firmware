#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "DeviceConfiguration.h"

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <string.h>
#include "komeco.h"
#include <ArduinoOTA.h>

#define EEPROM_MAX_ADDRS 512
#define CONFIRMATION_NUMBER 42
#define DEFAULT_TEMPERATURE 22

#define MQTT_BROKER_PORT 1883

#define OTA_PASS "update"
#define OTA_HOSTNAME "air-conditioner"
#define OTA_PORT 8266

enum {
  ACCESS_POINT_WEBSERVER
};

MDNSResponder mdns;
ESP8266WebServer server(80);
const char* ssid = "esp-config-";
const char* passphrase = "esp8266e";
String st;
String content;

KomecoController komeco;

DeviceConfiguration conf;
bool currentState = false;

// DNS Server 
const byte DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(10, 10, 10, 1);

WiFiClient wclient;
PubSubClient clientMQTT;
bool shouldRunLoop = false;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic is: ");
  Serial.println(topic);
  String stringPayload = String((char*)payload);
  if ((char) payload[0] == '0' || strncasecmp_P((char *)payload, "off", length) == 0) {
      komeco.turnOff();
      Serial.println("Turning off");
  } else if ((char)payload[0] == '1' || strncasecmp_P((char *)payload, "on", length) == 0) {
      komeco.setTemperatureTo(DEFAULT_TEMPERATURE);
      Serial.println("Turning on");
  } else if (stringPayload.toInt() >= 16) {
      komeco.setTemperatureTo(stringPayload.toInt());
      Serial.print("Setting temperature to");
      Serial.println(stringPayload);
  } else if (strncasecmp_P((char *)payload, "reset", length) == 0) {
    clearEEPROM();
    ESP.restart();
  } else if (strncasecmp_P((char *)payload, "reboot", length) == 0) {
    ESP.restart();
  }
}

//void callback(const MQTT::Publish& pub) {
//  String payload = pub.payload_string();
//  Serial.println(payload);
//  if (payload.equals("on")) {
//    komeco.setTemperatureTo(DEFAULT_TEMPERATURE);
//  } else if (payload.equals("off")) {
//    komeco.turnOff();
//  } else if (payload.toInt() >= 16) {
//    komeco.setTemperatureTo(payload.toInt());
//  } else if (payload.equals("auto")) {
//    komeco.setModeToAuto();
//  } else if (payload.equals("heat")) {
//    komeco.setModeToHeat();
//  } else if (payload.equals("cool")) {
//    komeco.setModeToCool();
//  };
//  // digitalWrite(actuatorPin, currentState ? HIGH : LOW);
//}

volatile unsigned long last_micros = 0;
volatile unsigned long settingsTime = 0;

void setup() {;
  Serial.begin(115200);
  EEPROM.begin(EEPROM_MAX_ADDRS);
  delay(5000);
  Serial.println("Yellow World");
  delay(5000);
  setupOTA();
  
  bool configurationsRead = readMQTTSettingFromEEPROM();
  if (configurationsRead) {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.begin(conf.wifiSsid, conf.wifiPass);
    if (testWifi()) {
      Serial.println("Could connect and read MQTT Settings. Starting on application mode");
      setupApplication(); // WiFi established, setup application
      shouldRunLoop = true;
    } else {
      Serial.println("Could NOT connect. Starting on access point mode");
      setupAccessPoint(); // No WiFi or MQTT Settings yet, enter configuration mode
    }
  } else {
    Serial.println("Could NOT read configurations. Starting on access point mode");
    setupAccessPoint(); // No WiFi or MQTT Settings yet, enter configuration mode
  }
}

void setupOTA() {
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  Serial.println("Starting OTA");
}

boolean readMQTTSettingFromEEPROM() {
  EEPROMReadAnything(0, conf);
  Serial.print("Confirmation number is: ");
  Serial.println(conf.confirmation);
  bool configurationsRead = (conf.confirmation == CONFIRMATION_NUMBER);
  Serial.println(configurationsRead);
  return configurationsRead;
}

String readStringFromEEPROM(int addr, int length) {
  char array[length];
  for (int i = 0; i < length; i++) {
    addr += i;
    char c = char(EEPROM.read(addr));
    array[i] = c;
  }
  String read = String(array);
  Serial.print("read string from EEPROM: ");
  Serial.println(read);
  return String(array);
}

bool testWifi(void) {
  int c = 0;
  while ( c < 50 ) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    c++;
  }
  Serial.println("\nConnect timed out, opening AP");
  return false;
}

void setupApplication() {
  if (mdns.begin(ssid, WiFi.localIP())) {
    Serial.println("\nMDNS responder started");
  }
  delay(10);
  komeco.setup();
  connectToBroker();
}

bool connectToBroker() {
  clientMQTT = PubSubClient(wclient);
  clientMQTT.setServer(conf.broker, MQTT_BROKER_PORT);
  clientMQTT.setCallback(callback);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String clientID = "esp_" + macToStr(mac);
  if (clientMQTT.connect(clientID.c_str(), conf.mqttUser, conf.mqttPassword)) {
    Serial.println("connected to MQTT broker!");
    clientMQTT.subscribe(conf.topic);
    return true;
  }
  return false;
}

void setupAccessPoint(void) {
  Serial.println("setting wifi mode");
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("disconnecting");
    WiFi.disconnect();
  }
  Serial.println("wating...");
  delay(100);
  Serial.println("scanning...");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  uint8_t mac[6];
  WiFi.macAddress(mac);
  WiFi.softAP((ssid + lastDigitsMacAddress(mac)).c_str());
  launchWeb(ACCESS_POINT_WEBSERVER);
}

void fetchNetworks() {
  st = "<ol>";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
}

void launchWeb(int webservertype) {
  Serial.println("\nWiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  setupWebServerHandlers(webservertype);
  // Start the server
  server.begin();
  Serial.print("Server type ");
  Serial.print(webservertype);
  Serial.println(" started");
  //Captive Portal
  dnsServer.start(DNS_PORT, "config.me", apIP);
}

void setupWebServerHandlers(int webservertype) {
  if ( webservertype == ACCESS_POINT_WEBSERVER ) {
    server.on("/", handleDisplayAccessPoints);
    server.on("/setap", handleSetAccessPoint);
    server.onNotFound(handleNotFound);
  }
}

void handleDisplayAccessPoints() {
  IPAddress ip = WiFi.softAPIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  fetchNetworks();
  String macStr = macToStr(mac);
  content = "<!DOCTYPE HTML>\n<html>Hello from ";
  content += ssid;
  content += " at ";
  content += ipStr;
  content += " (";
  content += macStr;
  content += ")<p>";
  content += st;
  content += "<p><form method='get' action='setap'>";
  content += "<label>SSID: </label><input name='ssid' length=32><label>Password: </label><input type='password' name='pass' length=64>";
  content += "<p><label>MQTT Broker URL or IP: </label><input name='broker'><p><label>MQTT Topic: </label><input name='topic' placeholder='home/bedroom/outlet/0'><p><label>MQTT User: </label><input name='user'><p><label>MQTT Password: </label><input type='password' name='mqttpass'>";
  content += "<p><input type='submit'></form>";
  content += "<p>We will attempt to connect to the selected AP and broker and reset if successful.";
  content += "<p>Wait a bit and try to publish to the selected topic";
  content += "</html>";
  server.send(200, "text/html", content);
}

void handleSetAccessPoint() {
  Serial.println("entered handleSetAccessPoint");
  int httpstatus = 200;
  conf.confirmation = CONFIRMATION_NUMBER;
  server.arg("ssid").toCharArray(conf.wifiSsid, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("pass").toCharArray(conf.wifiPass, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("broker").toCharArray(conf.broker, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("topic").toCharArray(conf.topic, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("user").toCharArray(conf.mqttUser, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("mqttpass").toCharArray(conf.mqttPassword, DEVICE_CONF_ARRAY_LENGHT);
  Serial.println(conf.confirmation);
  Serial.println(conf.wifiSsid);
  Serial.println(conf.wifiPass);
  Serial.println(conf.broker);
  Serial.println(conf.topic);
  Serial.println(conf.mqttUser);
  Serial.println(conf.mqttPassword);
  if (sizeof(conf.wifiSsid) > 0 && sizeof(conf.wifiPass) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(false);
    WiFi.begin(conf.wifiSsid, conf.wifiPass);
    if (testWifi()) {
      Serial.println("\nWifi Connection Success!");
      if (sizeof(conf.broker) > 0 && sizeof(conf.topic) > 0) {
        Serial.println("clearing EEPROM...");
        clearEEPROM();
        Serial.println("writting EEPROM...");
        EEPROMWriteAnything(0, conf);
        EEPROM.commit();
        Serial.println("Done! See you soon");
        delay(3000);
        ESP.restart();
      } else {
        content = "<!DOCTYPE HTML>\n<html>No broker or topic setted, please try again.</html>";
        Serial.println("Sending 404");
        httpstatus = 404;
      }
    } else {
      Serial.println("Could not connect to this wifi");
      content = "<!DOCTYPE HTML>\n<html>";
      content += "Failed to connect to AP ";
      content += conf.wifiSsid;
      content += ", try again.</html>";
    }
  } else {
    Serial.println("SSID or password not set");
    content = "<!DOCTYPE HTML><html>";
    content += "Error, no ssid or password set?</html>";
    //.println("Sending 404");
    httpstatus = 404;
  }
  server.send(httpstatus, "text/html", content);
}

void handleNotFound() {
  content = "File Not Found\n\n";
  content += "URI: ";
  content += server.uri();
  content += "\nMethod: ";
  content += (server.method() == HTTP_GET) ? "GET" : "POST";
  content += "\nArguments: ";
  content += server.args();
  content += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    content += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", content);
}

void loop() {
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  if (shouldRunLoop && testWifi()) {
    if (!clientMQTT.connected()) { //needs to reconnect to the broker
      Serial.println("connection with broker lost!");
      connectToBroker();
    }
    if (clientMQTT.connected()) {
      clientMQTT.loop();
      delay(100);
    }
  } else {
    if (settingsTime == 0) {
      settingsTime = micros();
    }

    if (micros() - settingsTime > 600000000) { //wait for 10m
      Serial.println("Watchdog: 10 minutes stuck on config mode. Restarting!");
      delay(1000);
      ESP.restart();
    }
    server.handleClient();
  }
}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

String lastDigitsMacAddress(const uint8_t* mac) {
  String result = macToStr(mac);
  return result.substring( result.length() - 5 );
}

void clearEEPROM() {
  for (int i = 0; i < EEPROM_MAX_ADDRS; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(100);
}
