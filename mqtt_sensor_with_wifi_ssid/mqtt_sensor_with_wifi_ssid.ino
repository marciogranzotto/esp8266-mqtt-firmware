#include <ESP8266mDNS.h>

#include <DHT.h>
#include <MQTT.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "DeviceConfiguration.h"

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include "DNSServer.h"
#include <string.h>

#define EEPROM_MAX_ADDRS 512
#define CONFIRMATION_NUMBER 42
#define DHTTYPE DHT22 // Sensor type

enum {
  ACCESS_POINT_WEBSERVER
};

MDNSResponder mdns;
ESP8266WebServer server(80);
const char* ssid = "esp-config-mode";
const char* passphrase = "esp8266e";
String st;
String content;
String tempTopic;
String humidTopic;

DeviceConfiguration conf;
int sensorPin = 2;
DHT dht(sensorPin, DHTTYPE);
const int sleepTimeS = 60;

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(10, 10, 10, 1);

WiFiClient wclient;
PubSubClient clientMQTT(wclient, conf.broker);
bool shouldRunLoop = false;
unsigned long initialTime = 0UL;

void setup() {
  initialTime = micros();
  Serial.begin(115200);
  EEPROM.begin(EEPROM_MAX_ADDRS);
  delay(5000);
  Serial.println(micros() - initialTime); //1
  initialTime = micros();
  Serial.println("Yellow World");
  bool configurationsRead = readMQTTSettingFromEEPROM();
  if (configurationsRead) {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.begin(conf.wifiSsid, conf.wifiPass);
    if (testWifi()) {
      Serial.println(micros() - initialTime); //2
      initialTime = micros();
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
  Serial.println("\nTesting WiFi...");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Wifi connected!");
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
  dht.begin();
  delay(10);

  connectToBroker();
}

bool connectToBroker() {
  clientMQTT = PubSubClient(wclient, conf.broker);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String clientID = "esp_" + macToStr(mac);
  if (clientMQTT.connect(MQTT::Connect(clientID).set_auth(conf.mqttUser, conf.mqttPassword))) {
    Serial.println("connected to MQTT broker!");
    tempTopic = String(conf.topicTemperature);
    humidTopic = String(conf.topicHumidity);
    return true;
  }
  return false;
}

void setupAccessPoint(void) {
  Serial.println("setting wifi mode");
  WiFi.mode(WIFI_STA);
  Serial.println("disconnecting");
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  Serial.println("waiting");
  delay(100);
  Serial.println("scanning");
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
  st = "<ol>";
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
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);
  launchWeb(ACCESS_POINT_WEBSERVER);
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
  dnsServer.start(DNS_PORT, "*", apIP);
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
  content += "<p><label>MQTT Broker URL or IP: </label><input name='broker'><p><label>MQTT Temperature Topic: </label><input name='topicTemperature' placeholder='home/bedroom/temperature'><p><label>MQTT Humidity Topic: </label><input name='topicHumidity' placeholder='home/bedroom/humidity'><p><label>MQTT User: </label><input name='user'><p><label>MQTT Password: </label><input type='password' name='mqttpass'>";
  content += "<p><input type='submit'></form>";
  content += "<p>We will attempt to connect to the selected AP and broker and reset if successful.";
  content += "<p>Wait a bit and try to subscribe to the selected topic";
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
  server.arg("topicTemperature").toCharArray(conf.topicTemperature, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("topicHumidity").toCharArray(conf.topicHumidity, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("user").toCharArray(conf.mqttUser, DEVICE_CONF_ARRAY_LENGHT);
  server.arg("mqttpass").toCharArray(conf.mqttPassword, DEVICE_CONF_ARRAY_LENGHT);
  Serial.println(conf.confirmation);
  Serial.println(conf.wifiSsid);
  Serial.println(conf.wifiPass);
  Serial.println(conf.broker);
  Serial.println(conf.topicTemperature);
  Serial.println(conf.topicHumidity);
  Serial.println(conf.mqttUser);
  Serial.println(conf.mqttPassword);
  if (sizeof(conf.wifiSsid) > 0 && sizeof(conf.wifiPass) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(false);
    WiFi.begin(conf.wifiSsid, conf.wifiPass);
    if (testWifi()) {
      Serial.println("\nWifi Connection Success!");
      if (sizeof(conf.broker) > 0 && sizeof(conf.topicTemperature) > 0 && sizeof(conf.topicHumidity) > 0) {
        Serial.println("clearing EEPROM...");
        clearEEPROM();
        Serial.println("writting EEPROM...");
        EEPROMWriteAnything(0, conf);
        EEPROM.commit();
        Serial.println("Done! See you soon");
        delay(3000);
        abort();
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
  Serial.println(micros() - initialTime); //3
  initialTime = micros();
  Serial.println("loop()");
  dnsServer.processNextRequest();
  if (shouldRunLoop) {
    if (testWifi()) {
      Serial.println(micros() - initialTime); //4
      initialTime = micros();
      Serial.println("Wifi connected.");
      if (!clientMQTT.connected()) { //reconnects to the broker
        Serial.println("connection with broker lost!");
        connectToBroker();
      }

      if (clientMQTT.connected()) {
        Serial.println(micros() - initialTime); //5
        initialTime = micros();
        Serial.println("still connected to the broker!");
        clientMQTT.loop();
        readSensorAndPublishResults();
        Serial.println(micros() - initialTime); //6
        initialTime = micros();
        Serial.println("ESP8266 in sleep mode");
        ESP.deepSleep(sleepTimeS * 1000000);
      }
    } else {
      server.handleClient();  // In this example we're not doing too much
    }
  } else {
    server.handleClient();  // In this example we're not doing too much
  }
}

void readSensorAndPublishResults() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  String temperature = String(t, 1);
  String humidity = String(h, 1);
  Serial.println("temperature " + temperature);
  Serial.println("humidity " + humidity);
  if (t == t) { //t is not NaN
    temperature += "°C";
    
    MQTT::Publish pub(tempTopic, temperature);
    pub.set_retain(true);
    clientMQTT.publish(pub);
  }

  if (h == h) { //h is not NaN
    humidity += "\045";
    
    MQTT::Publish pub(humidTopic, humidity);
    pub.set_retain(true);
    clientMQTT.publish(pub);
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

void clearEEPROM() {
  for (int i = 0; i < EEPROM_MAX_ADDRS; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(100);
}
