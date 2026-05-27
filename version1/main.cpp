// first version that worked with bluetooth and mit inventor app
// tried to get it working with newer versions of things and gave up in favour of http/javascript in 2026
//
#include <Arduino.h>
// #include <AsyncJson.h>
// #include <NonBlockingRtttl.h>
// #include "HardwareSerial.h"
// #include "esp_adc_cal.h"
#include "driver/rtc_io.h"
// #include <Preferences.h>
// #include <Update.h>
// #include <nvs_flash.h>
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
#include <WiFi.h> // For getting the MAC address

#define version "kijaniv1"
#define versiondate "2025-05-23"

// Preferences preferences;

#define BUILTIN_LED 2
#define en8v 16
#define en5v 17
#define dvrsleep 12 //34
#define MotorA1 32
#define MotorA2 33
#define MotorB1 25
#define MotorB2 26
#define servo1Pin 27
#define servo2Pin 14
Servo servo1;
Servo servo2;
Servo motorA;
Servo motorB;
int minUs = 500;
int maxUs = 2500;
int pos = 0;

BluetoothSerial SerialBT;

ESP32PWM pwm;
int freq = 1000;

TaskHandle_t Task1;
TaskHandle_t Task2;

float battvoltage = 0;
float temperature = 0;

boolean paired = false;
String receivedData = "";

// const char *startup4 = "Jingle:d=4,o=5,b=100:8b,16d6,16c6,8e6";
// // const char *startup4 = "Jingle:d=4,o=5,b=100:8b,16d6,16c6,8e6";
// const char *factoryreset = "we-rock:d=4,o=6,b=45:16d#.6,32d#.6,16a#.6,32a#.6,16c.7,32g#.6,16a#.6,32a#.6,16d#.6,32d#.6,16a#.6,32a#.6,32a#.6,32g#.6,32f#.6,16f.6,32f.6,16d#.6,32d#.6,16a#.6,32a#.6,16c.7,32g#.6,16a#.6,32a#.6,16d#.6,32d#.6,16a#.6,32a#.6,32f#.6,32f.6,32d.6,16d#.6,32d#.6,";
// const char *testbutton = "SouthAfr:d=16,o=5,b=100:8g,8g,8g,8a,4b,4b,4a,4a,4g,4p,8b,8b,8b,8b,4c6,4c6,8b,8b,4b,4a,4p,8g,8g,8g,8a,4b,4b,4a,4c6,4b,4p,4a,4p,4g,4p,8f#,8g,4a,4g";

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

// void Task1code(void *pvParameters)
// {
//   // Serial.print("Task1 running on core ");
//   // Serial.println(xPortGetCoreID());

//   for (;;)
//   {
//       // Check if data is available to read
//     if (SerialBT.available()) {
//       String receivedData = SerialBT.readString();
//       Serial.print("Received: ");
//       Serial.println(receivedData);

//       // Send a response back
//       SerialBT.println("ESP32 received: " + receivedData);
//     }
//   }
// }

// // Task2code: blinks an LED every 700 ms
// void Task2code(void *pvParameters)
// {
//   // Serial.print("Task2 running on core ");
//   // Serial.println(xPortGetCoreID());

//   for (;;)
//   {
//         digitalWrite(BUILTIN_LED, LOW);
//         vTaskDelay(1000);
//         digitalWrite(BUILTIN_LED, HIGH);
//         vTaskDelay(1000);
//   }
// }
void processItem(const String& item) {
  // Split by colon to get name and data
  int delimiterPos = item.indexOf(':');
  if (delimiterPos == -1) {
    Serial.println("Malformed data item: " + item);
    return;
  }

  String name = item.substring(0, delimiterPos);
  String value = item.substring(delimiterPos + 1);

  // Handle each item based on its name
  if (name == "M1") {
    int speed = value.toInt();
    Serial.print("Set M1: ");
    Serial.println(speed);
    if (abs(speed)<5) {
      speed = 0;
    }
    //get direction
    if (speed>=0){
      digitalWrite(MotorA2, LOW);
      ledcWrite(MotorA1, speed);
    } else {
      digitalWrite(MotorA2, HIGH); 
      ledcWrite(MotorA1, 255+speed);
    }
    
     

  } else if (name == "M2") {
    int speed = value.toInt();
    Serial.print("Set M2: ");
    Serial.println(speed);
    if (abs(speed)<5) {
      speed = 0;
    }
        //get direction
    if (speed>=0){
      digitalWrite(MotorB2, LOW); 
      ledcWrite(MotorB1, speed);
    } else {
      digitalWrite(MotorB2, HIGH); 
      ledcWrite(MotorB1, 255+speed);
    }
    
     

  } else if (name == "S1") {
    int speed = value.toInt();
    Serial.print("Set S1: ");
    Serial.println(speed);
    servo1.write(speed);

  } else if (name == "S2") {
    int speed = value.toInt();
    Serial.print("Set S2: ");
    Serial.println(speed);
    servo2.write(speed);

  } else if (name == "estop") {
    bool estopOn = (value == "on");
    Serial.print("estop has been set to ");
    Serial.println(estopOn ? "ON" : "OFF");
    ledcWrite(MotorA1, 0);
    ledcWrite(MotorB1, 0);
    digitalWrite(MotorA2, LOW);
    digitalWrite(MotorB2, LOW);
    
  } else if (name == "connected") {
    Serial.print("connected to: ");
    Serial.println(value);
    int adcValue = analogRead(36);
    int vIn = adcValue*22.5;
    SerialBT.println("[batt:" + String(vIn)+"]");
  } else {
    Serial.println("Unknown name: " + name);
    Serial.println("got: " + item);
  }
}
void setMotorSpeed(int speed, int direction) {
  speed = constrain(speed, 0, 255); // Limit speed to valid range
  digitalWrite(MotorA2, direction);
  ledcWrite(0, speed); // Set PWM duty cycle
}
void playTone(int frequency, int duration_ms) {
    // Calculate the delay for half a wave (1/frequency)
    int halfPeriod_us = 1000000 / (2 * frequency); 

    unsigned long endTime = millis() + duration_ms;
    while (millis() < endTime) {
        // Alternate motor direction to create vibration
        digitalWrite(MotorA1, HIGH);
        digitalWrite(MotorA2, LOW);
        delayMicroseconds(halfPeriod_us);

        digitalWrite(MotorA1, LOW);
        digitalWrite(MotorA2, HIGH);
        delayMicroseconds(halfPeriod_us);
    }

    // Stop the motor after the tone
    digitalWrite(MotorA1, LOW);
    digitalWrite(MotorA2, LOW);
}
void setup()
{
  // setup the debug out comms
  Serial.begin(115200);

  // init io's
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(en8v, OUTPUT);
  pinMode(en5v, OUTPUT);
  pinMode(dvrsleep, OUTPUT);
  pinMode(MotorA1, OUTPUT);
  pinMode(MotorA2, OUTPUT);
  pinMode(MotorB1, OUTPUT);
  pinMode(MotorB2, OUTPUT);
  // pinMode(Servo1, OUTPUT);
  // pinMode(Servo2, OUTPUT);
  // digitalWrite(en8v, HIGH);
  // digitalWrite(dvrsleep, HIGH);
  // ledcSetup(0, 5000, 8); // Channel 0, 5kHz frequency, 8-bit resolution
  // ledcAttachPin(MotorA1, 0); // Attach PWM to MOTOR_PWM_PIN
  //   // Test motor control
  // Serial.println("Motor forward at 50% speed");
  // // setMotorSpeed(128, 0); // Forward at 50% speed
  // digitalWrite(MotorA2, LOW);
  // ledcWrite(0, 128); // Set PWM duty cycle
  // delay(2000);

  // Serial.println("Motor forward at full speed");
  // // setMotorSpeed(255, 0); // Forward at full speed
  // digitalWrite(MotorA2, LOW);
  // ledcWrite(0, 255); // Set PWM duty cycle
  // delay(2000);

  // Serial.println("Motor reverse at 50% speed");
  // // setMotorSpeed(128, 1); // Reverse at 50% speed
  // digitalWrite(MotorA2, HIGH);
  // ledcWrite(0, 128);
  // delay(2000);

  // Serial.println("Motor reverse at full speed");
  // // setMotorSpeed(0, 1); // Reverse at full speed
  // digitalWrite(MotorA2, HIGH);
  // ledcWrite(0, 0); // Set PWM duty cycle
  // delay(2000);

  // Serial.println("Motor stopped");
  // setMotorSpeed(0, 0); // Stop motor
  // delay(2000);

  // for(;;){}

  // esp_adc_cal_characteristics_t *adc_chars;
  // adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  // esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, 1100, adc_chars);

  // int adcValue = analogRead(36);  // Read the raw ADC value (0-4095)

  // // // Calculate the input voltage (Vin) using the voltage divider formula
  // //float vIn = vOut * (6800 + 470) / 6800;
  // int vIn = adcValue*22.5;

  // // // ---- Read Internal Temperature ----
  // float temperature = temperatureRead();  // Read internal temperature (in °C)

  // // ---- Print Results ----
  // Serial.print("Input Voltage: ");
  // Serial.print(vIn);
  // Serial.println("V");

  // Serial.print("Internal Temperature: ");
  // Serial.print(temperature);
  // Serial.println("°C");

  Serial.println();
  Serial.println();
  Serial.print("Booting system, ");
  Serial.print(version);
  Serial.print(", ");
  Serial.println(versiondate);
  digitalWrite(en8v, HIGH);
  digitalWrite(dvrsleep, HIGH);



  playTone(987, 300);  // 8b: B (987 Hz) for 300 ms
  // delay(50);           // Short delay between notes

  playTone(1175, 150); // 16d6: D6 (1175 Hz) for 150 ms
  // delay(50);           // Short delay between notes

  playTone(1047, 150); // 16c6: C6 (1047 Hz) for 150 ms
  // delay(50);           // Short delay between notes

  playTone(1319, 300); // 8e6: E6 (1319 Hz) for 300 ms


  digitalWrite(en5v, HIGH);
  servo1.setPeriodHertz(50);      // Standard 50hz servo
	servo2.setPeriodHertz(50);      // Standard 50hz servo
  
  
  // ledcSetup(2, 5000, 8);
  // ledcSetup(3, 5000, 8);
  // // ledcAttachPin(servo1Pin, 2);
  // // ledcAttachPin(servo2Pin, 3);
  // ledcAttachPin(MotorA1, 2);
  // ledcAttachPin(MotorB1, 3);
  ledcAttach(MotorA1, 5000, 8);
  ledcAttach(MotorB1, 5000, 8);
	servo1.attach(servo1Pin, minUs, maxUs);
	servo2.attach(servo2Pin, minUs, maxUs);
  

  // rtttl::begin(en8v, startup4);
  
  // while (!rtttl::done())
  // {
  //   rtttl::play();
  // }
  // rtttl::stop();
  // digitalWrite(MotorA1, LOW);



  // Serial.print("tune done");

  // while (1) {
  // int adcValue = analogRead(36);
  // int vIn = adcValue*22.5;
  // Serial.print(adcValue);
  // Serial.print(" : ");
  // Serial.println(vIn);
  // delay(500);
  // }
  // motorA.attach(MotorA1);
  // motorA.write(0);
  // digitalWrite(MotorA2, LOW);
  // motorB.attach(MotorB1);
  // motorB.write(0);
  // digitalWrite(MotorB2, LOW);

  
  // for(;;) {
 
  //   Serial.println("forward 50%");
  //   digitalWrite(MotorA2, LOW); 
  //   // motorB.write(32);
  //   ledcWrite(2, 128);  
  //   servo1.write(0);               
  //   delay(2000);  
  //   // Serial.println("forward 100%");
  //   // digitalWrite(MotorA2, LOW); 
  //   // // motorB.write(32);
  //   // ledcWrite(0, 255);  
  //   // // servo1.write(0);               
  //   // delay(2000); 
    
  //   Serial.println("off");
  //   digitalWrite(MotorA2, LOW); 
  //   ledcWrite(2, 0);
  //   // motorB.write(0);                 
  //   delay(2000);  
    
  //   // Serial.println("backward 50%");
  //   // digitalWrite(MotorA2, HIGH); 
  //   // // motorB.write(255);
  //   // ledcWrite(0, 128);
  //   // // servo1.write(180);                 
  //   // delay(2000);
  //   Serial.println("backward 100%");
  //   digitalWrite(MotorA2, HIGH); 
  //   // motorB.write(255);
  //   ledcWrite(2, 0);
  //   servo1.write(180);                 
  //   delay(2000);


  //   // Serial.println("off");
  //   // digitalWrite(MotorA2, LOW); 
  //   // // motorA.write(0);  
  //   // ledcWrite(0, 0);               
  //   // delay(2000); 

  // }     


  // ledcSetup(0, 1000, 8) //channel, freq, bits
  // ledcAttachPin(M1, 0); //attached motopin to channel 0
  // for (int speed = 0; speed <= 255; speed++) {
  //   ledcWrite(0, speed); // Set motor speed (PWM duty cycle)
  //   delay(20); // Adjust delay for smooth speed increase
  // }

  // Get the MAC address of the ESP32
  String macAddress = WiFi.macAddress(); // Format: XX:XX:XX:XX:XX:XX
  macAddress.replace(":", ""); // Remove colons for a cleaner name (optional)
  String pin = macAddress.substring(6); // Use the last 4 characters of the MAC
  // Create a unique Bluetooth name using the MAC address
  String bluetoothName = "MootBot_" + pin;

  // Generate a unique PIN from the MAC address
  // int numericPin = 0;
  // for (int i = 0; i < pin.length(); i++) {
  //   numericPin = numericPin * 16 + (isdigit(pin[i]) ? pin[i] - '0' : toupper(pin[i]) - 'A' + 10);
  // }
  // numericPin %= 10000; // Ensure it's a 4-digit PIN

  // Initialize Bluetooth with the unique name
  if (!SerialBT.begin(bluetoothName.c_str())) {
    Serial.println("An error occurred initializing Bluetooth");
    fatalerror(3);
  }
  // SerialBT.setPin(String(numericPin).c_str());

  Serial.println("Bluetooth initialized with name: " + bluetoothName);
  // Serial.println("Bluetooth PIN: " + String(numericPin));
  Serial.println("Waiting for connection...");
    // Register callbacks for connection events
  SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) {
      paired = true;
    } else if (event == ESP_SPP_CLOSE_EVT) {
      paired = false;
    }
  });

  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  // xTaskCreatePinnedToCore(
  //     Task1code, /* Task function. */
  //     "Task1",   /* name of task. */
  //     10000,     /* Stack size of task */
  //     NULL,      /* parameter of the task */
  //     1,         /* priority of the task */
  //     &Task1,    /* Task handle to keep track of created task */
  //     0);        /* pin task to core 0 */
  // delay(500);

  // // create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  // xTaskCreatePinnedToCore(
  //     Task2code, /* Task function. */
  //     "Task2",   /* name of task. */
  //     10000,     /* Stack size of task */
  //     NULL,      /* parameter of the task */
  //     1,         /* priority of the task */
  //     &Task2,    /* Task handle to keep track of created task */
  //     1);        /* pin task to core 1 */
  // delay(500);
}

void loop()
{
  if (SerialBT.available()) {
    digitalWrite(BUILTIN_LED, LOW);
    char incomingByte = SerialBT.read(); // Read one byte at a time
    receivedData += incomingByte;       // Append to the buffer

    if (incomingByte == ']') { // Check for the termination character
      Serial.print("Received: ");
      Serial.println(receivedData);
      String content = receivedData.substring(1, receivedData.length() - 1); 

      // Split by commas
      int start = 0, end = 0;
      while ((end = content.indexOf(',', start)) != -1) {
        processItem(content.substring(start, end));
        start = end + 1;
      }
      // Process the last item
      processItem(content.substring(start));
      // Send a response back
      int adcValue = analogRead(36);
      int vIn = adcValue*22.5;
      Serial.println(vIn);
      // SerialBT.println("[batt:" + String(vIn)+"]");

      // Clear the buffer for the next message
      receivedData = "";
    }




  }
  digitalWrite(BUILTIN_LED, HIGH);
  if (!paired) {
    vTaskDelay(10);
    digitalWrite(BUILTIN_LED, LOW);
    vTaskDelay(100);
  }
  
  
}
