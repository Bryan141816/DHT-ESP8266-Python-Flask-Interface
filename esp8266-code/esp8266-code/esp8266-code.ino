#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>   // mDNS support
#include "DHT.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ==== CONFIG =====
#define LEDPIN D0
#define DHTPIN D4          // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11  
    // DHT 11 sensor type
const char* ssid = "{wifi_ssid}";
const char* password = "{wifi_password}";

const char* host = "graph.facebook.com";
const int httpsPort = 443;

const char* fbAccessToken = "{pagetoken}";  // Your full Page Access Token
const char* recipientId = "{user psid}";   // Replace with the recipient's PSID


ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

bool messageSent = false;  // Flag to track if a message has been sent
unsigned long lastMessageTime = 0;  // Last time a message was sent
unsigned long messageCooldown = 60000;  // 10 minutes cooldown in milliseconds


void sendMessageToFacebook(float t) {
  WiFiClientSecure client;
  client.setInsecure(); // ⚠️ Not secure, for development only

  Serial.print("Connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed!");
    return;
  }

  // Construct the JSON payload
  String jsonPayload;
  StaticJsonDocument<256> doc;
  JsonObject message = doc.createNestedObject("message");
  message["text"] =  "Temperature is: " + String(t) + "°C";

  JsonObject recipient = doc.createNestedObject("recipient");
  recipient["id"] = recipientId;

  serializeJson(doc, jsonPayload);

  // Construct the HTTP POST request
  String url = "/v21.0/me/messages?access_token=" + String(fbAccessToken);

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("User-Agent: ESP8266");
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(jsonPayload.length()));
  client.println(); // End of headers
  client.println(jsonPayload); // Body

  // Wait for response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Print response
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();
  Serial.println("Request sent.");
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  dht.begin();
  pinMode(LEDPIN, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("ESP8266 IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up mDNS responder
  if (MDNS.begin("temperature01")) {
    Serial.println("mDNS responder started: http://temperature01.local");
  } else {
    Serial.println("Error setting up mDNS responder!");
  }

  // Set up server route
  server.on("/data", HTTP_GET, []() {
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      server.send(500, "application/json", "{\"error\":\"Failed to read temperature\"}");
      return;
    }
    String data = "{\"temperature\": " + String(t) + "}";
    server.send(200, "application/json", data);
  });

  server.on("/send_temp", HTTP_GET, []() {
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      server.send(500, "application/json", "{\"error\":\"Failed to read temperature\"}");
      return;
    }

    sendMessageToFacebook(t);
    server.send(200, "application/json", "{\"message\":\"Temperature sent to Facebook\"}");
  });

  server.on("/toggle_led", HTTP_GET, []() {
    if (server.hasArg("state")) {
      String state = server.arg("state");
      Serial.println("Received state: " + state);

      if (state == "true") {
        digitalWrite(LEDPIN, LOW);
      } 
      else if (state == "false") {
        digitalWrite(LEDPIN, HIGH);
      }

      server.send(200, "text/plain", "LED set to: " + state);
    } else {
      server.send(400, "text/plain", "Missing 'state' parameter");
    }
  });

  server.begin();
  Serial.println("HTTP server started");

}

void loop() {
  server.handleClient();
  MDNS.update(); // Keep mDNS running

  // If message has been sent and cooldown period has passed, reset flag
  if (messageSent && (millis() - lastMessageTime >= messageCooldown)) {
    messageSent = false;
    Serial.println("Cooldown over, resuming temperature readings.");
  }

  // Read temperature every 1 second if message hasn't been sent recently
  if (!messageSent) {
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    } else {
      Serial.print("Temperature: ");
      Serial.println(t);

      // If temperature is 35°C or higher, send the message
      if (t >= 35) {
        sendMessageToFacebook(t);  // Send temperature to Facebook
        messageSent = true;        // Set flag to stop reading temperature
        lastMessageTime = millis(); // Record the time the message was sent
      }
    }
  }

  delay(1000); // Wait 1 second before printing again
}
