#include <ESP8266mDNS.h>

#include <MQTT.h>
#include <PubSubClient.h>
#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>

#define EEPROM_MAX_ADDRS 512
#define CONFIRMATION_NUMBER 42

enum {
  //  APPLICATION_WEBSERVER = 0,
  ACCESS_POINT_WEBSERVER
};

MDNSResponder mdns;
ESP8266WebServer server(80);
const char* ssid = "";  // Use this as the ssid as well
// as the mDNS name
const char* passphrase = "esp8266e";
String st;
String content;
String broker; //= "casa-granzotto.ddns.net"; //MQTT broker
String topic; //= "home/bedroom/led"; //MQTT topic
String mqttUser; // = "osmc";
String mqttPassword; // = "84634959";
String wifiSsid;
String wifiPass;
boolean connectedToBroker = false;


int led = 2;

void callback(const MQTT::Publish& pub) {
  if (pub.payload_string().equals("on")) {
    digitalWrite(led, HIGH);
  };
  if (pub.payload_string().equals("off")) {
    digitalWrite(led, LOW);
  };
}

WiFiClient wclient;
PubSubClient clientMQTT(wclient, broker);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_MAX_ADDRS);
  delay(10);
  Serial.println("Yellow World");
  bool mqttSuccess = readMQTTSettingFromEEPROM();
  WiFi.mode(WIFI_STA);  // Assume we've already been configured
  WiFi.persistent(false);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  //  Serial.setDebugOutput(true);
  //  WiFi.printDiag(Serial);
  if (mqttSuccess && testWifi()) {
    Serial.println("Could connect and read MQTT Settings. Starting on application mode");
    setupApplication(); // WiFi established, setup application
  } else {
    Serial.println("Could NOT connect and read MQTT Settings. Starting on access point mode");
    setupAccessPoint(); // No WiFi or MQTT Settings yet, enter configuration mode
  }
}

boolean readMQTTSettingFromEEPROM(){
  //the order is: {qsid, qpass, broker, topic, mqttUser, mqttPass}
  int addr = 0; //start from 0
  int confirm = EEPROMReadInt(addr);
  Serial.print("Confirmation number was:");
  Serial.println(confirm);
  if(confirm == CONFIRMATION_NUMBER){ //just to confirm that the eeprom isn't full of gibberish
  addr += 2;
  int length = EEPROMReadInt(addr);
  addr += 2; //need to skip 2 positions that were used by the length int
  if (length > 0){
    wifiSsid = readStringFromEEPROM(addr, length);
    addr += length;
    length = EEPROMReadInt(addr);
    addr += 2;
    wifiPass = readStringFromEEPROM(addr, length);
    addr += length;
    length = EEPROMReadInt(addr);
    addr += 2;
    if (length > 0){
      broker = readStringFromEEPROM(addr, length);
      addr += length;
      //now we get the topic
      length = EEPROMReadInt(addr);
      addr += 2;
      if(length > 0){
        topic = readStringFromEEPROM(addr, length);
        addr += length;
        //let's get the user and passoword
        length = EEPROMReadInt(addr);
        addr += 2;
        mqttUser = readStringFromEEPROM(addr, length);
        addr += length;
        length = EEPROMReadInt(addr);
        addr += 2;
        mqttPassword = readStringFromEEPROM(addr, length);
        return true;
      }
    }
  }
}
return false;
}

String readStringFromEEPROM(int addr, int length){
  char array[length];
  for(int i = 0; i < length; i++) {
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
  Serial.println("\nWaiting for Wifi to connect...");
  while ( c < 20 ) {
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
    ;
  }
  pinMode(led, OUTPUT);
  delay(10);

  connectToBroker();
}

void connectToBroker() {
  clientMQTT.set_callback(callback);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String clientID = "esp_" + macToStr(mac);
  if (clientMQTT.connect(MQTT::Connect(clientID).set_auth(mqttUser, mqttPassword))) {
    Serial.println("connected to MQTT broker!");
    connectedToBroker = true;
    clientMQTT.subscribe(topic);
  }
}

void setupAccessPoint(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
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
  WiFi.softAP(ssid, passphrase, 6);
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
  content += "<p><label>MQTT Broker URL or IP: </label><input name='broker'><p><label>MQTT Topic: </label><input name='topic'><p><label>MQTT User: </label><input name='user'><p><label>MQTT Password: </label><input type='password' name='mqttpass'>";
  content += "<p><input type='submit'></form>";
  content += "<p>We will attempt to connect to the selected AP and broker and reset if successful.";
  content += "<p>Wait a bit and try to publish/subscribe to the selected topic";
  content += "</html>";
  server.send(200, "text/html", content);
}

void handleSetAccessPoint() {
  int httpstatus = 200;
  String qsid = decodeUrlText(server.arg("ssid"));
  String qpass = decodeUrlText(server.arg("pass"));
  String broker = decodeUrlText(server.arg("broker"));
  String mqttTopic = decodeUrlText(server.arg("topic"));
  String mqttUser = decodeUrlText(server.arg("user"));
  String mqttPass = decodeUrlText(server.arg("mqttpass"));
  if (qsid.length() > 0 && qpass.length() > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(false);
    WiFi.begin(qsid.c_str(), qpass.c_str());
    if (testWifi()) {
      Serial.println("\nWifi Connection Success!");
      if (broker.length() > 0 && mqttTopic.length() > 0) {
        String mqttConfigs[] = {qsid, qpass, broker, mqttTopic, mqttUser, mqttPass};
        saveStringsToEEPROM(mqttConfigs);
        delay(3000);
        abort();
      } else {
        content = "<!DOCTYPE HTML>\n<html>No broker or topic setted, please try again.</html>";
        Serial.println("Sending 404");
        httpstatus = 404;
      }
    } else {
      content = "<!DOCTYPE HTML>\n<html>";
      content += "Failed to connect to AP ";
      content += qsid;
      content += ", try again.</html>";
    }
  } else {
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
  if (connectedToBroker) {
    connectedToBroker = clientMQTT.loop();
  } else {
    if (testWifi()){
      Serial.print("connection with broker lost!");
      connectToBroker(); //reconnects to the broker
    } else {
      server.handleClient();  // In this example we're not doing too much
    }
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

//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
//returns the current address
int EEPROMWriteInt(int p_address, int p_value)
{
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);

  EEPROM.write(p_address, lowByte);
  p_address++;
  EEPROM.write(p_address, highByte);
  return p_address;
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address)
{
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);

  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

//Saves the string length to the first 2 bytes and then the string on the said size
void saveStringsToEEPROM(String array[]) {
  clearEEPROM();
  int addr = 0;
  addr = EEPROMWriteInt(addr, CONFIRMATION_NUMBER);
  for (int i = 0; i < sizeof(array); i++){
    String obj = array[i];
    int size = obj.length();
    addr = EEPROMWriteInt(addr, size);
    for (int j = 0; j < obj.length(); j++) {
      char c = obj.charAt(j);
      EEPROM.write(++addr, c);
    }
  }
  EEPROM.commit();
  delay(100);
}

void clearEEPROM(){
  for (int i = 0; i < EEPROM_MAX_ADDRS; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(100);
}

//Decodes url paramters to a normal string
String decodeUrlText(String param) {
  param.replace("+"," ");
  param.replace("%21","!");
  param.replace("%23","#");
  param.replace("%24","$");
  param.replace("%26","&");
  param.replace("%27","'");
  param.replace("%28","(");
  param.replace("%29",")");
  param.replace("%2A","*");
  param.replace("%2B","+");
  param.replace("%2C",",");
  param.replace("%2F","/");
  param.replace("%3A",":");
  param.replace("%3B",";");
  param.replace("%3D","=");
  param.replace("%3F","?");
  param.replace("%40","@");
  param.replace("%5B","[");
  param.replace("%5D","]");

  return param;
}
