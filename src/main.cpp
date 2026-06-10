// to start a quick htp server go yto the data folder and type
// python3 -m http.server

#include <Arduino.h>
#include <NonBlockingRtttl.h>
#include "driver/rtc_io.h"
#include <Preferences.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#define version "kijani_v3.02b"
#define versiondate "2025-06-02"

Preferences preferences;
AsyncWebServer server(80);

#define BUILTIN_LED 2
#define en8v 16     // removed from v2 board onward
#define en5v 17     // removed from v2 board onward
#define dvrsleep 12 // 34 -  modwire on v1 and v2 boards
#define MotorA1 32
#define MotorA2 33
#define MotorB1 25
#define MotorB2 26
#define servo1Pin 27
#define servo2Pin 14
#define Vmod 15
#define pgm 5 // factory reset pin
#define statled 2 //status led
Servo servo1;
Servo servo2;
Servo motorA;
Servo motorB;
int minUs = 500;
int maxUs = 2500;
// int pos = 0;

// ESP32PWM pwm;
int freq = 1000;

TaskHandle_t Task1; // motor and pins task
TaskHandle_t Task2; // radio tasks

float battvoltage = 0;
float temperature = 0;
// String unitcode = "000000";
String AP = "Mootbot"; // default ssid for our AP
String APpass = "";    // default password for our
float calibrationFactor = 5.02 / 213.0;

boolean paired = false;
String receivedData = "";
String pendingTune = "";
bool playPendingTune = false;

String startup = "Jingle:d=4,o=5,b=100:8b,16d6,16c6,8e6";
String test = "Scale:d=4,o=5,b=120:c,d,e,f,g,a,b,c6";
// // const char *startup4 = "Jingle:d=4,o=5,b=100:8b,16d6,16c6,8e6";
// const char *factoryreset = "we-rock:d=4,o=6,b=45:16d#.6,32d#.6,16a#.6,32a#.6,16c.7,32g#.6,16a#.6,32a#.6,16d#.6,32d#.6,16a#.6,32a#.6,32a#.6,32g#.6,32f#.6,16f.6,32f.6,16d#.6,32d#.6,16a#.6,32a#.6,16c.7,32g#.6,16a#.6,32a#.6,16d#.6,32d#.6,16a#.6,32a#.6,32f#.6,32f.6,32d.6,16d#.6,32d#.6,";
const char *testbutton = "SouthAfr:d=16,o=5,b=100:8g,8g,8g,8a,4b,4b,4a,4a,4g,4p,8b,8b,8b,8b,4c6,4c6,8b,8b,4b,4a,4p,8g,8g,8g,8a,4b,4b,4a,4c6,4b,4p,4a,4p,4g,4p,8f#,8g,4a,4g";

volatile unsigned long ledFlickerUntil = 0;
enum LedState
{
  LED_NO_WIFI,
  LED_CONNECTED,
  LED_CHARGING
};

LedState ledState = LED_NO_WIFI;

unsigned long lastLedChange = 0;
bool ledOutput = false;

void fatalerror(int errnum)
{
  while (true)
  {
    for (int i = 0; i < errnum; i++)
    {
      digitalWrite(BUILTIN_LED, HIGH);
      delay(200);
      digitalWrite(BUILTIN_LED, LOW);
      delay(200);
    }
    delay(1000);
  }
}


String Hex2Str(char din)
{
  String dta = "";
  if (din < 0x10)
  {
    dta += "0";
  }
  dta += String(din, HEX);
  dta.toUpperCase();
  return dta;
}

void loadsettings()
{
  preferences.begin("settings", true);
  // unitcode = preferences.getString("unitcode", unitcode);
  AP = preferences.getString("AP", AP);
  APpass = preferences.getString("APpass", APpass);
  startup = preferences.getString(
      "startupTune",
      "Jingle:d=4,o=5,b=100:8b,16d6,16c6,8e6");
  String temp = preferences.getString(
      "batteryCalibrationFactor",
      "0.02352");
  calibrationFactor = temp.toFloat();

  preferences.end();
}

void storesetting(String value, String key)
{
  // add setting toi nvm, reload all settings and put it in the event log
  Serial.print("Storing String ");
  Serial.print(key);
  Serial.print(", ");
  Serial.println(value);
  preferences.begin("settings", false);
  preferences.putString(key.c_str(), value);
  preferences.end();

  // reload all settings to update if changes were made to system settings
  loadsettings();
  // add to eventlog and auditlog
  // sendsettings();
  Serial.println("finished stopring");
}
int getbattery()
{
  int adcValue = analogRead(36);
  return adcValue * calibrationFactor * 1000;
}
void updateStatusLed()
{
  // Determine state
  if (getbattery() > 4500 )
  {
    ledState = LED_CHARGING;
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    ledState = LED_CONNECTED;
  }
  else
  {
    ledState = LED_NO_WIFI;
  }

  unsigned long now = millis();

  switch (ledState)
  {
    case LED_CHARGING:
      // Fast heartbeat (200ms)
      if (now - lastLedChange > 200)
      {
        lastLedChange = now;
        ledOutput = !ledOutput;
        digitalWrite(statled, ledOutput);
      }
      break;

    case LED_NO_WIFI:
      // Slow heartbeat (1000ms)
      if (now - lastLedChange > 1000)
      {
        lastLedChange = now;
        ledOutput = !ledOutput;
        digitalWrite(statled, ledOutput);
      }
      break;

    case LED_CONNECTED:

      if (millis() < ledFlickerUntil)
      {
        digitalWrite(statled, LOW);
      }
      else
      {
        digitalWrite(statled, HIGH);
      }
      break;
  }
}

void processItem(const String &item)
{
  // Split by colon to get name and data
  int delimiterPos = item.indexOf(':');
  if (delimiterPos == -1)
  {
    Serial.println("Malformed data item: " + item);
    return;
  }

  String name = item.substring(0, delimiterPos);
  String value = item.substring(delimiterPos + 1);

  // Handle each item based on its name
  if (name == "M1")
  {
    int speed = value.toInt();
    Serial.print("Set M1: ");
    Serial.println(speed);
    if (abs(speed) < 5)
    {
      speed = 0;
    }
    // get direction
    if (speed >= 0)
    {
      digitalWrite(MotorA2, LOW);
      ledcWrite(MotorA1, speed);
    }
    else
    {
      digitalWrite(MotorA2, HIGH);
      ledcWrite(MotorA1, 255 + speed);
    }
  }
  else if (name == "M2")
  {
    int speed = value.toInt();
    Serial.print("Set M2: ");
    Serial.println(speed);
    if (abs(speed) < 5)
    {
      speed = 0;
    }
    // get direction
    if (speed >= 0)
    {
      digitalWrite(MotorB2, LOW);
      ledcWrite(MotorB1, speed);
    }
    else
    {
      digitalWrite(MotorB2, HIGH);
      ledcWrite(MotorB1, 255 + speed);
    }
  }
  else if (name == "S1")
  {
    int speed = value.toInt();
    Serial.print("Set S1: ");
    Serial.println(speed);
    servo1.write(speed);
  }
  else if (name == "S2")
  {
    int speed = value.toInt();
    Serial.print("Set S2: ");
    Serial.println(speed);
    servo2.write(speed);
  }
  else if (name == "estop")
  {
    bool estopOn = (value == "on");
    Serial.print("estop has been set to ");
    Serial.println(estopOn ? "ON" : "OFF");
    ledcWrite(MotorA1, 0);
    ledcWrite(MotorB1, 0);
    digitalWrite(MotorA2, LOW);
    digitalWrite(MotorB2, LOW);
  }

  else
  {
    Serial.println("Unknown name: " + name);
    Serial.println("got: " + item);
  }
}

void setMotorSpeed(int speed, int direction)
{
  speed = constrain(speed, 0, 255); // Limit speed to valid range
  digitalWrite(MotorA2, direction);
  ledcWrite(0, speed); // Set PWM duty cycle
}
void playTone(int frequency, int duration_ms)
{
  // Calculate the delay for half a wave (1/frequency)
  int halfPeriod_us = 1000000 / (2 * frequency);

  unsigned long endTime = millis() + duration_ms;
  while (millis() < endTime)
  {
    // Alternate motor direction to create vibration
    digitalWrite(MotorA1, HIGH);
    digitalWrite(MotorA2, LOW);
    delayMicroseconds(halfPeriod_us);

    digitalWrite(MotorA1, LOW);
    digitalWrite(MotorA2, HIGH);
    delayMicroseconds(halfPeriod_us);
    yield(); 
  }

  // Stop the motor after the tone
  digitalWrite(MotorA1, LOW);
  digitalWrite(MotorA2, LOW);
}
void playRTTTL(const char *p)
{
  int default_dur = 4;
  int default_oct = 6;
  int bpm = 63;
  // Serial.println("1");
  // Skip name
  while (*p && *p != ':')
    p++;
  if (*p == ':')
    p++;
  ledcDetach(MotorA1);
  // Serial.println("2");
  pinMode(MotorA1, OUTPUT);
  // Serial.println("3");
  // Parse defaults
  while (*p && *p != ':')
  {
    if (*p == 'd')
    {
      p += 2;
      default_dur = atoi(p);
    }
    else if (*p == 'o')
    {
      p += 2;
      default_oct = atoi(p);
    }
    else if (*p == 'b')
    {
      p += 2;
      bpm = atoi(p);
    }

    while (*p && *p != ',')
    {
      if (*p == ':')
        break;
      p++;
    }

    if (*p == ',')
      p++;
  }

  if (*p == ':')
    p++;

  int wholenote = (60 * 1000L / bpm) * 4;

  while (*p)
  {
    int duration = 0;

    if (isdigit(*p))
    {
      duration = atoi(p);

      while (isdigit(*p))
        p++;
    }

    if (duration == 0)
      duration = default_dur;

    duration = wholenote / duration;

    int note = 0;

    switch (*p)
    {
    case 'c':
      note = 1;
      break;
    case 'd':
      note = 3;
      break;
    case 'e':
      note = 5;
      break;
    case 'f':
      note = 6;
      break;
    case 'g':
      note = 8;
      break;
    case 'a':
      note = 10;
      break;
    case 'b':
      note = 12;
      break;
    case 'p':
      note = 0;
      break;
    }

    p++;

    if (*p == '#')
    {
      note++;
      p++;
    }

    if (*p == '.')
    {
      duration += duration / 2;
      p++;
    }

    int octave = default_oct;

    if (isdigit(*p))
    {
      octave = *p - '0';
      p++;
    }

    if (*p == ',')
      p++;

    if (note == 0)
    {
      delay(duration);
      continue;
    }

    static const float noteTable[] =
        {
            0,
            261.63, // C
            277.18,
            293.66, // D
            311.13,
            329.63, // E
            349.23, // F
            369.99,
            392.00, // G
            415.30,
            440.00, // A
            466.16,
            493.88 // B
        };

    float freq = noteTable[note];

    while (octave > 4)
    {
      freq *= 2;
      octave--;
    }

    while (octave < 4)
    {
      freq /= 2;
      octave++;
    }
    // Serial.print(freq);
    // Serial.print(" ");
    // Serial.println(duration);
    // Serial.println("5");
    playTone((int)freq, duration);
    // Serial.println("6");
    delay(duration / 10);
  }
  // Serial.println("7");
  ledcAttach(MotorA1, 5000, 8);
  // Serial.println("8");
}

void setup()
{
  // setup the debug out comms
  Serial.begin(115200);
  delay(200);
  // Serial.println("1");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Serial.print("\n\nBooting system, ");
  Serial.print(version);
  Serial.print(", ");
  Serial.println(versiondate);

  // for (;;){
  //   Serial.println(getbattery());
  //   delay(1000);
  // }

  // init io's
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(en8v, OUTPUT);
  pinMode(en5v, OUTPUT);
  pinMode(dvrsleep, OUTPUT);
  // digitalWrite(Vmod, LOW);
  pinMode(Vmod, OUTPUT);
  digitalWrite(Vmod, LOW);
  pinMode(Vmod, INPUT);
  pinMode(MotorA1, OUTPUT);
  pinMode(MotorA2, OUTPUT);
  pinMode(MotorB1, OUTPUT);
  pinMode(MotorB2, OUTPUT);
  pinMode(pgm, INPUT);
  pinMode(statled, OUTPUT);
  digitalWrite(statled, LOW);
  // // // ---- Read Internal Temperature ----
  float temperature = temperatureRead(); // Read internal temperature (in °C)

  // ---- Print Results ----
  Serial.print("Input Voltage: ");
  Serial.print(float(getbattery()) / 1000);
  Serial.println("V");

  Serial.print("Internal Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");

  digitalWrite(en8v, HIGH);
  digitalWrite(en5v, HIGH);
  digitalWrite(dvrsleep, HIGH);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servo1.setPeriodHertz(50); // Standard 50hz servo
  servo2.setPeriodHertz(50); // Standard 50hz servo

  servo1.attach(servo1Pin, minUs, maxUs);
  servo2.attach(servo2Pin, minUs, maxUs);
  ledcAttach(MotorA1, 5000, 8);
  ledcAttach(MotorB1, 5000, 8);

  // playTone(987, 300);  // 8b: B (987 Hz) for 300 ms
  // playTone(1175, 150); // 16d6: D6 (1175 Hz) for 150 ms
  // playTone(1047, 150); // 16c6: C6 (1047 Hz) for 150 ms
  // playTone(1319, 300); // 8e6: E6 (1319 Hz) for 300 ms

  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    fatalerror(1);
  }

  // // rtttl::begin(en8v, startup4);

  // // while (!rtttl::done())
  // // {
  // //   rtttl::play();
  // // }
  // // rtttl::stop();
  // // digitalWrite(MotorA1, LOW);

  // // Serial.print("tune done");

  // // Get the MAC address of the ESP32
  // String macAddress = WiFi.macAddress(); // Format: XX:XX:XX:XX:XX:XX

  uint64_t chipid = ESP.getEfuseMac();

  char suffix[7];
  sprintf(suffix, "%06X", (uint32_t)(chipid & 0xFFFFFF));

  AP = "MootBot_" + String(suffix);

  // Serial.println(apName);
  // macAddress.replace(":", ""); // Remove colons for a cleaner name (optional)
  // String pin = macAddress.substring(6); // Use the last 4 characters of the MAC
  // AP = "MootBot_" + pin;
  Serial.println(AP);
  // Generate a unique PIN from the MAC address
  // int numericPin = 0;
  // for (int i = 0; i < pin.length(); i++) {
  //   numericPin = numericPin * 16 + (isdigit(pin[i]) ? pin[i] - '0' : toupper(pin[i]) - 'A' + 10);
  // }
  // numericPin %= 10000; // Ensure it's a 4-digit PIN

  Serial.println("ending setup");

  // not sure what this does but its to do with the headers for the html UI
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/processcontrol", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              //Parameters
              // motorA -255 to 255
              // mototB -255 to 255
              // Servo1 0 to 180
              // Servo2 0 to 180
              // boost true/false
              ledFlickerUntil = millis() + 100;
              if (request->hasParam("M1")) {
                String value = request->arg("M1");
                int speed = value.toInt();
                Serial.print("Set M1: ");
                Serial.println(speed);
                if (abs(speed) < 5)
                {
                  speed = 0;
                }
                // get direction
                if (speed >= 0)
                {
                  digitalWrite(MotorA2, LOW);
                  ledcWrite(MotorA1, speed);
                }
                else
                {
                  digitalWrite(MotorA2, HIGH);
                  ledcWrite(MotorA1, 255 + speed);
                }
              }
              if (request->hasParam("M2")) {
                String value = request->arg("M2");
                int speed = value.toInt();
                Serial.print("Set M2: ");
                Serial.println(speed);
                if (abs(speed) < 5)
                {
                  speed = 0;
                }
                // get direction
                if (speed >= 0)
                {
                  digitalWrite(MotorB2, LOW);
                  ledcWrite(MotorB1, speed);
                }
                else
                {
                  digitalWrite(MotorB2, HIGH);
                  ledcWrite(MotorB1, 255 + speed);
                }
              }
    
              if (request->hasParam("S1")) {
                String value = request->arg("S1");
                int speed = value.toInt();
                Serial.print("Set S1: ");
                Serial.println(speed);
                servo1.write(speed);
              }
              if (request->hasParam("S2")) {
                String value = request->arg("S2");
                int speed = value.toInt();
                Serial.print("Set S2: ");
                Serial.println(speed);
                servo2.write(speed);
              }
              request->send(200, "text/html", "done"); });
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("file"))
    {
        request->send(400, "text/plain", "Missing file");
        return;
    }

    String path = request->getParam("file")->value();
    // Serial.println(path);
    if (LittleFS.remove("/"+path))
    {
        request->send(200, "text/plain", "Deleted");
    }
    else
    {
        request->send(500, "text/plain", "Delete failed");
    } });
  server.on(
      "/upload",
      HTTP_POST,
      [](AsyncWebServerRequest *request)
      {
        request->send(200);
      },
      [](AsyncWebServerRequest *request,
         String filename,
         size_t index,
         uint8_t *data,
         size_t len,
         bool final)
      {
        static File uploadFile;

        if (index == 0)
        {
          String path = "/" + filename;
          uploadFile = LittleFS.open(path, "w");
        }

        if (uploadFile)
        {
          uploadFile.write(data, len);
        }

        if (final)
        {
          uploadFile.close();
        }
      });
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");
    ledFlickerUntil = millis() + 100; 
    response->print("{");

    response->printf("\"used\":%u,", LittleFS.usedBytes());
    response->printf("\"total\":%u,", LittleFS.totalBytes());

    response->print("\"files\":[");

    File root = LittleFS.open("/");

    bool first = true;
    int i = 0;

    File file = root.openNextFile();

    while (file)
    {
        if (!first)
            response->print(",");

        first = false;

        StaticJsonDocument<256> data;

        data["id"] = i++;
        data["name"] = file.name();
        // data["dir"] = file.isDirectory();
        data["size"] = file.isDirectory() ? 0 : file.size();

        serializeJson(data, *response);

        file.close();
        file = root.openNextFile();
    }

    response->print("]}");

    request->send(response); });

  server.on("/getver", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String resp = "ver: " version;

              request->send(200, "text/html", resp.c_str()); });

  server.on("/playrtttl", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              //TODO: I dont know why the system restarts with this, will look at it later
              ledFlickerUntil = millis() + 100;
    if (!request->hasParam("tune"))
    {
        request->send(400, "text/plain", "Missing tune parameter");
        return;
    }

    String tune = request->getParam("tune")->value();

    Serial.println("Playing RTTTL:");
    Serial.println(tune);
    delay(1000);
    // playRTTTL(tune.c_str());
    pendingTune = request->arg("tune");
    playPendingTune = true;
    request->send(200, "text/plain", "Playing"); });

  server.on("/systeminfo", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              // Get free heap memory
              uint32_t freeHeap = ESP.getFreeHeap();
              ledFlickerUntil = millis() + 100;
              // Get SPIFFS total and used bytes
              size_t totalBytes = LittleFS.totalBytes();
              size_t usedBytes = LittleFS.usedBytes();

              // Get WiFi info
              // String macAddress = WiFi.macAddress();
                uint64_t chipid = ESP.getEfuseMac();

              char suffix[7];
              sprintf(suffix, "%06X", (uint32_t)(chipid & 0xFFFFFF));
              String macAddress = String(suffix);
              // Get uptime
              uint32_t uptime = millis() / 1000;

              // Get flash info
              uint32_t flashChipSize = ESP.getFlashChipSize();
              uint32_t flashChipSpeed = ESP.getFlashChipSpeed();
              String flashChipMode;
              switch (ESP.getFlashChipMode()) {
                case FM_QIO: flashChipMode = "QIO"; break;
                case FM_QOUT: flashChipMode = "QOUT"; break;
                case FM_DIO: flashChipMode = "DIO"; break;
                case FM_DOUT: flashChipMode = "DOUT"; break;
                case FM_FAST_READ: flashChipMode = "FAST_READ"; break;
                case FM_SLOW_READ: flashChipMode = "SLOW_READ"; break;
                default: flashChipMode = "UNKNOWN"; break;
              }
              //<tr> <td> UnitCode/Serial </td><td>" + unitcode + "</td> </tr>
              String resp = "<div style='margin-left:auto; margin-right:auto;'>\
                <table id='wifi-settings-table'>\
                  <thead>\
                    <tr>\
                      <th>Item</th>\
                      <th>Value</th>\
                  </tr>\
                </thead>\
                <tbody>\
                  <tr> <td> RAM </td><td>" + String(freeHeap) + " bytes</td> </tr>\
                  <tr> <td> File system </td><td>" + String(usedBytes) + " / " + String(totalBytes) + " bytes used</td> </tr>\
                  <tr> <td> MAC Address </td><td>" + macAddress + "</td> </tr>\
                  <tr> <td> Uptime </td><td>" + String(uptime) + " seconds</td> </tr>\
                  <tr> <td> Flash Chip Size </td><td>" + String(flashChipSize / (1024 * 1024)) + " MB</td> </tr>\
                  <tr> <td> Firmware virsion </td><td>" versiondate "</td> </tr>\
                  <tr> <td> battvoltage </td><td>" + float(getbattery())/1000 + "</td> </tr>\
                  <tr> <td> temperature </td><td>" + (int)temperatureRead() + "</td> </tr>\
                </tbody>\
    </table>\
                  </div>";
              //TODO: are there any other system settings we should add?
              request->send(200, "text/html", resp.c_str()); });
  server.on("/quickstatus", HTTP_ANY, [](AsyncWebServerRequest *request)
            {
              ledFlickerUntil = millis() + 100;
              // int vIn = adcValue * 22.5;
              float vIn = float(getbattery())/1000;
              // float vIn = adcValue * 3.3 / 4095.0 * 11.0;
              // // ---- Read Internal Temperature ----
              float temperature = temperatureRead(); // Read internal temperature (in °C)
              String json = "{\"result\":\"pass\","
              "\"vIn\":" + String(vIn) + ","
              "\"temperature\":" + String(temperature) +
              "}";
                
              // Serial.println(json);
              request->send(200, "application/json", json); });
  server.on("/getsettings", HTTP_ANY, [](AsyncWebServerRequest *request)
            {
              ledFlickerUntil = millis() + 100;
              String json = "";
              if (request->hasParam("key"))
                {

                 String key = request->getParam("key")->value();
                  Serial.println(key);                  
                  preferences.begin("settings", false);
                  String defaultans = "None";
                  //if we dont have it whats the default?
                  if (key == "AP") {
                    defaultans = AP;
                  }
                  if (key == "startupTune") {
                    defaultans = startup;
                  }
                  if (key == "batteryCalibrationFactor") {
                    defaultans = String(calibrationFactor,5);
                  }
                  String value = preferences.getString(key.c_str(), defaultans);
                  Serial.println(value);
                  preferences.end();
                  json = "{\"result\":\"pass\","
                      "\"key\":\"" + key + "\","
                      "\"value\":\"" + value + "\"}";
                }
              else
              {
                json = "{\"result\":\"fail\"}";
              }
      
              Serial.println(json);
              request->send(200, "application/json", json); });

  server.on("/updatesettings", HTTP_ANY, [](AsyncWebServerRequest *request)
            {
              ledFlickerUntil = millis() + 100;
              if (request->hasParam("key") && request->hasParam("value"))
              {

                String key = request->arg("key");
                String value = request->arg("value");

                storesetting(value, key);

                request->send(200, "text/plain", "Updated successfully");
                // vTaskDelay(1000);
                // ESP.restart();
              }
              else
              {
                request->send(200, "text/plain", "Updated failed");
                Serial.println("updatefailed: ");
                for (uint8_t i = 0; i < request->args(); i++)
                {
                  // AsyncWebParameter *p = request->getParam(i);
                  const AsyncWebParameter *p = request->getParam(i);
                  Serial.print(p->name());
                  Serial.print(": ");
                  Serial.println(p->value());
                }
              } });
  server.on("/updatefirmware", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    // this is called after upload finishes
    ledcWrite(MotorA1, 0);
    ledcWrite(MotorB1, 0);
    request->send(200, "text/plain", "Update complete. Rebooting...");
    delay(500);
    ESP.restart(); },

            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {

    if (index == 0) {
      Serial.println("OTA Update Start");

      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        return;
      }
    }

    if (len) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }

    if (final) {
      if (Update.end(true)) {
        Serial.println("OTA Success");
      } else {
        Update.printError(Serial);
      }
    } });
  // endpoint to factory reset all settings
  server.on("/reset", HTTP_ANY, [](AsyncWebServerRequest *request)
            {
              String resp;
              if (request->hasArg("factory_reset"))
              {
                // initdb(false);
                // defaultsettings();
                resp = " \
        <html>\
            <body>\
            <p>Database has been reset to factory defaults</p>\
            <p>You will be redirected in 10 seconds</p>\
            <script>\
                var timer = setTimeout(function() {\
                    window.location='index.html'\
                }, 10000);\
            </script>\
        </body>\
        </html>";
                // request->send ( 200, "application/json", resp );
                request->send(200, "text/html", resp.c_str());
                Serial.println("{factoryreset:1}");
                ESP.restart();
              }
              else
              {
                resp = " \
        <html>\
            <body>\
            <p>Database has NOT been reset to factory defaults, Please follow instructions to do so. </p>\
            <p>You will be redirected in 3 seconds</p>\
            <script>\
                var timer = setTimeout(function() {\
                    window.location='index.html'\
                }, 3000);\
            </script>\
        </body>\
        </html>";

                request->send(200, "text/html", resp.c_str());
                ESP.restart();
              } });

  // allow browser to view files on SPIFFS, making index.html the default
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Handle "not found" requests
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    String requestedUrl = request->url();
    
    // Log the requested URL for debugging purposes
    Serial.print("server.notfound triggered: ");
    Serial.println(requestedUrl);

    // Otherwise, redirect to the homepage
    request->redirect("/"); });

  if (digitalRead(pgm))
  {
    loadsettings();
    playRTTTL(startup.c_str());
  }
  else
  {
    playRTTTL(testbutton);
  }


  Serial.println("AP mode started");
  Serial.println(AP);
  WiFi.mode(WIFI_AP);

  delay(100);
  Serial.println("AP mode set");



  // Configure the IP address
  IPAddress local_ip(10, 10, 10, 10);
  if (!WiFi.softAPConfig(local_ip, local_ip, IPAddress(255, 255, 255, 0)))
  {
    Serial.println("AP Config Failed!");
  }
  else
  {
    Serial.println("AP Config Success!");
  }
  if (!WiFi.softAP(AP, APpass)) // access point and password
  {
    fatalerror(5);
  }
  // dnsServer.start(53, "*", WiFi.softAPIP());

  // TODO: impliment factory reset

  server.begin();
  // delay(2000);
  // playRTTTL("Jingle:d=4,o=5,b=100:8b,8e6,16d6,16c6");
  // pendingTune = "Jingle:d=4,o=5,b=100:8b,8e6,16d6,16c6";
  // playPendingTune = true;
}
// int speed = 0;
// bool direction;
void loop()
{
  //make led heartbeat fast when charging and slow when no connection. On when connecvted and not charging, flicker with commands
  updateStatusLed();

  // if we are charging we should not move the motors
  if (getbattery() > 1500)
  {
    ledcWrite(MotorA1, 0);
    ledcWrite(MotorB1, 0);
    digitalWrite(MotorA2, LOW);
    digitalWrite(MotorB2, LOW);
  }

  if (playPendingTune)
  {
    playPendingTune = false;
    Serial.println("Tune contents:");
    Serial.println(pendingTune);

    Serial.println("Calling playRTTTL");

    playRTTTL(pendingTune.c_str());
    // delay(2000);
    // Serial.println("B");
    // playRTTTL("Jingle:d=4,o=5,b=100:8b,8e6,16d6,16c6");
    // Serial.println("C");
  }

  // testing the hardware
  //  servo1.write(abs(speed/2));
  //  Serial.println(speed);
  //  if (direction) {
  //    speed ++;
  //    if (speed>254) {
  //      direction = false;
  //    }
  //  } else {
  //    speed --;
  //    if (speed<-254) {
  //      direction = true;
  //    }
  //  }

  // if (speed>=0){
  //   digitalWrite(MotorA2, LOW);
  //   ledcWrite(MotorA1, speed);
  // } else {
  //   digitalWrite(MotorA2, HIGH);
  //   ledcWrite(MotorA1, 255+speed);
  // }

  // if (speed>=0){
  //   digitalWrite(MotorB2, LOW);
  //   ledcWrite(MotorB1, speed);
  // } else {
  //   digitalWrite(MotorB2, HIGH);
  //   ledcWrite(MotorB1, 255+speed);
  // }
  // delay(10);
}
