#include <ESP8266mDNS.h>

#include <MQTT.h>
#include <PubSubClient.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

enum {
  //  APPLICATION_WEBSERVER = 0,
  ACCESS_POINT_WEBSERVER
};

MDNSResponder mdns;
ESP8266WebServer server(80);
const char* ssid = "esp8266e";  // Use this as the ssid as well
// as the mDNS name
const char* passphrase = "esp8266e";
String st;
String content;
char topic[] = "home/bedroom/led"; //MQTT topic
char broker[] = "casa-granzotto.ddns.net"; //MQTT broker
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
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);  // Assume we've already been configured
  //  Serial.setDebugOutput(true);
  //  WiFi.printDiag(Serial);
  if (testWifi()) {
    setupApplication(); // WiFi established, setup application
  } else {
    setupAccessPoint(); // No WiFi yet, enter configuration mode
  }
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
  }
  pinMode(led, OUTPUT);
  delay(10);
  clientMQTT.set_callback(callback);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macStr = macToStr(mac);
  if (clientMQTT.connect(macStr)) {
    Serial.print("connected to MQTT broker!");
    connectedToBroker = true;
    clientMQTT.subscribe(topic);
  }

  //launchWeb(APPLICATION_WEBSERVER); // In this example just launch a
  // web server
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
  //  WiFi.printDiag(Serial);
}

void setupWebServerHandlers(int webservertype)
{
  if ( webservertype == ACCESS_POINT_WEBSERVER ) {
    server.on("/", handleDisplayAccessPoints);
    server.on("/setap", handleSetAccessPoint);
    server.onNotFound(handleNotFound);
  }
  //  else if (webservertype == APPLICATION_WEBSERVER) {
  //    server.on("/", handleRoot);
  //    server.on("/setap", handleAccessPointAlreadySet);
  //    server.onNotFound(handleNotFound);
  //  }
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
  content += ")";
  content += "<p>";
  content += st;
  content += "<p><form method='get' action='setap'><label>SSID: </label>";
  content += "<input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
  content += "<p>We will attempt to connect to the selected AP and reset if successful.";
  content += "<p>Wait a bit and try connecting to http://";
  content += ssid;
  content += ".local";
  content += "</html>";
  server.send(200, "text/html", content);
}

void handleSetAccessPoint() {
  int httpstatus = 200;
  String qsid = server.arg("ssid");
  String qpass = server.arg("pass");
  if (qsid.length() > 0 && qpass.length() > 0) {
    for (int i = 0; i < qsid.length(); i++)
    {
      // Deal with (potentially) plus-encoded ssid
      qsid[i] = (qsid[i] == '+' ? ' ' : qsid[i]);
    }
    for (int i = 0; i < qpass.length(); i++)
    {
      // Deal with (potentially) plus-encoded password
      qpass[i] = (qpass[i] == '+' ? ' ' : qpass[i]);
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(qsid.c_str(), qpass.c_str());
    if (testWifi()) {
      Serial.println("\nGreat Success!");
      delay(3000);
      abort();
    }
    content = "<!DOCTYPE HTML>\n<html>";
    content += "Failed to connect to AP ";
    content += qsid;
    content += ", try again.</html>";
  } else {
    content = "<!DOCTYPE HTML><html>";
    content += "Error, no ssid or password set?</html>";
    Serial.println("Sending 404");
    httpstatus = 404;
  }
  server.send(httpstatus, "text/html", content);
}

//void handleRoot() {
//  IPAddress ip = WiFi.localIP();
//  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
//  uint8_t mac[6];
//  WiFi.macAddress(mac);
//  String macStr = macToStr(mac);
//  content = "<!DOCTYPE HTML>\n<html>Hello from ";
//  content += ssid;
//  content += " at ";
//  content += ipStr;
//  content += " (";
//  content += macStr;
//  content += ")";
//  content += "</html>";
//  server.send(200, "text/html", content);
//}

//void handleAccessPointAlreadySet() {
//  content = "<!DOCTYPE HTML>\n<html>";
//  content += "You already set up the access point and it is working if you got this far.";
//  content += "</html>";
//  server.send(200, "text/html", content);
//}

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
    clientMQTT.loop();
  } else {
    server.handleClient();  // In this example we're not doing too much
  }
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}
