#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>
#include "base64.h"  // Needed for encoding username/password

// Device Name
const char* DEVICE_NAME = "55G Large";

// ---- WiFi Setup ----
const char* ssid = "Girls Gone Wireless";
const char* password = "Huber312!";

// ---- Email (SMTP) Setup ----
// Yahoo SMTP server info
const char* smtp_server = "smtp.mail.yahoo.com";
const int smtp_port = 465;
const char* email_user = "nathanhuber2014@yahoo.com";
const char* email_pass = "fkxbdgzsjsobieal";  // must generate App Password
const char* email_to = "nathanhuber2014@gmail.com";

// ---- Sensor Setup ----
#define ONE_WIRE_BUS 3  // GPIO3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ---- Buzzer Pin ----
#define BUZZER_PIN 5  // GPIO5 (D1)
unsigned long lastBuzzTime = 0;

// ---- LED Pin ----
#define LED_PIN 1  // GPIO1 (TX)
unsigned long lastLedToggle = 0;
bool ledState = false;

// ---- Web Server ----
ESP8266WebServer server(80);
int emailCount = 0;  // tracks how many emails sent

// ---- User Settings ----
float targetTempF = 78.0;  // default 78F
float toleranceF = 2.0;    // default ±3F

// ---- Email Function ----
void sendEmail(float tempF) {
  WiFiClientSecure client;
  client.setInsecure();  // skip certificate verification

  if (!client.connect(smtp_server, smtp_port)) {
    Serial.println("SMTP connection failed");
    return;
  }

  // SMTP handshake
  client.println("EHLO esp8266");
  delay(100);
  client.println("AUTH LOGIN");
  delay(100);
  client.println(base64::encode(email_user));
  delay(100);
  client.println(base64::encode(email_pass));
  delay(100);

  // Mail headers + body
  client.println("MAIL FROM:<" + String(email_user) + ">");
  client.println("RCPT TO:<" + String(email_to) + ">");
  client.println("DATA");
  client.println("From: ESP8266 <" + String(email_user) + ">");
  client.println("To: " + String(email_to));
  client.println("Subject: [" + String(DEVICE_NAME) + "] Temperature Alert!");
  client.println("MIME-Version: 1.0");
  client.println("Content-Type: text/plain; charset=UTF-8");
  client.println();
  client.println(String(DEVICE_NAME) + " reports: Temperature out of range: " + String(tempF, 1) + " °F");
  client.println(".");
  client.println("QUIT");

  Serial.println("Email attempted (check Yahoo inbox).");
}

// ---- Webpage Handler ----
void handleRoot() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  float tempF = (tempC * 9.0 / 5.0) + 32.0;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='10'>";  // auto refresh
  html += "<title>" + String(DEVICE_NAME) + " Temperature</title>";
  html += "<body style='font-family: Arial; text-align: center; margin-top: 50px;'>";

  html += "<h1>" + String(DEVICE_NAME) + " - Temperature Monitor</h1>";

  if (tempC == DEVICE_DISCONNECTED_C) {
    html += "<p><b>Error: Could not read temperature sensor.</b></p>";
  } else {
    html += "<p>Current Temperature: <b>" + String(tempF, 1) + " F</b></p>";
    html += "<p>Target: " + String(targetTempF, 1) + " F+- " + String(toleranceF, 1) + " F</p>";
  }

  // Input form
  html += "<form action='/set' method='GET'>";
  html += "Target Temp (F): <input type='text' name='target' value='" + String(targetTempF, 1) + "'><br><br>";
  html += "Tolerance (F): <input type='text' name='tol' value='" + String(toleranceF, 1) + "'><br><br>";
  html += "<input type='submit' value='Update'>";
  html += "</form>";

  html += "</body></html>";
  server.send(200, "text/html", html);

  // Check alarm condition
  if (tempC != DEVICE_DISCONNECTED_C) {
    if (tempF < targetTempF - toleranceF || tempF > targetTempF + toleranceF) {
      unsigned long now = millis();
      if (now - lastBuzzTime > 10000) {  // every 10s
        lastBuzzTime = now;
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);  // chirp
        digitalWrite(BUZZER_PIN, LOW);

        if (emailCount < 2) {  // only allow 2 emails max
          sendEmail(tempF);
          emailCount++;
        }
      }
    } else {
      // Temp is back in range → reset email counter
      emailCount = 0;
    }
  }
}

// ---- Handle User Input ----
void handleSet() {
  if (server.hasArg("target")) {
    targetTempF = server.arg("target").toFloat();
  }
  if (server.hasArg("tol")) {
    toleranceF = server.arg("tol").toFloat();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// LED Status
void updateLedStatus(bool wifiConnected, bool sensorOK) {
  unsigned long now = millis();

  if (!wifiConnected) {
    // Blink 1Hz (toggle every 500ms)
    if (now - lastLedToggle >= 500) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else if (!sensorOK) {
    // Blink 4Hz (toggle every 125ms)
    if (now - lastLedToggle >= 125) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    // Solid ON when all good
    digitalWrite(LED_PIN, HIGH);
  }
}

void handleJSON() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  float tempF = (tempC * 9.0 / 5.0) + 32.0;

  String json = "{";
  if (tempC == DEVICE_DISCONNECTED_C) {
    json += "\"error\":\"Sensor disconnected\"";
  } else {
    json += "\"device\":\"" + String(DEVICE_NAME) + "\",";
    json += "\"temperature\":" + String(tempF, 1) + ",";
    json += "\"target\":" + String(targetTempF, 1) + ",";
    json += "\"tolerance\":" + String(toleranceF, 1) + ",";
    bool inRange = !(tempF < targetTempF - toleranceF || tempF > targetTempF + toleranceF);
    json += "\"status\":\"" + String(inRange ? "in_range" : "out_of_range") + "\"";
  }
  json += "}";

  server.send(200, "application/json", json);
}
void sendIPEmail(IPAddress ip) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(smtp_server, smtp_port)) {
    Serial.println("SMTP connection failed");
    return;
  }

  client.println("EHLO esp8266");
  delay(100);
  client.println("AUTH LOGIN");
  delay(100);
  client.println(base64::encode(email_user));
  delay(100);
  client.println(base64::encode(email_pass));
  delay(100);

  client.println("MAIL FROM:<" + String(email_user) + ">");
  client.println("RCPT TO:<" + String(email_to) + ">");
  client.println("DATA");
  client.println("From: ESP8266 <" + String(email_user) + ">");
  client.println("To: " + String(email_to));
  client.println("Subject: ESP8266 Booted - IP Address");
  client.println("MIME-Version: 1.0");
  client.println("Content-Type: text/plain; charset=UTF-8");
  client.println();
  client.println("Subject: [" + String(DEVICE_NAME) + "] Booted - IP Address");
  client.println(String(DEVICE_NAME) + " has booted and connected to WiFi.");
  client.println("Current IP: " + ip.toString());
  client.println(".");
  client.println("QUIT");

  Serial.println("IP email attempted.");
}
void setup(void) {
  Serial.begin(115200);
  Serial.swap();  // Because using GPIO3 (RX)
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  sendIPEmail(WiFi.localIP());

  sensors.begin();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/json", handleJSON);
  server.begin();
  Serial.println("Web server started!");
}


void loop(void) {
  server.handleClient();

  // Update LED status
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  bool sensorOK = (tempC DEVICE_DISCONNECTED_C);
  updateLedStatus(wifiOK, sensorOK);
}
