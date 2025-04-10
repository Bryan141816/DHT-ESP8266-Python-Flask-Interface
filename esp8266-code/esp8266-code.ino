#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"

#define DHTPIN D4    // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 22  (AM2302), AM2321

const char* ssid = "wifissd"; //change it to your wifi network
const char* password = "password";
ESP8266WebServer server(80);

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  
  dht.begin();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");
  Serial.print("ESP8266 IP Address: ");
  Serial.println(WiFi.localIP());
  
  server.on("/data", HTTP_GET, []() {
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
    String data = "{\"temperature\": " + String(t)  +"}";
    server.send(200, "application/json", data);
  });

  server.begin();
}

void loop() {
  server.handleClient();
}
