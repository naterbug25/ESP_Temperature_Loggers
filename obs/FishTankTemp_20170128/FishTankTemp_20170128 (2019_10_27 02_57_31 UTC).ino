#include "Average.h"

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4 /*-(Connect to Pin 4 )-*/
OneWire ourWire(ONE_WIRE_BUS);
DallasTemperature sensors(&ourWire);
byte Fahr; // Stored Temp value
byte Test = 80;

#define CNT 10
int TempRetrieve;
//int Temp[CNT];
int TempSent;

#define RED_PIN 5
#define GREEN_PIN 6
#define BLUE_PIN 3
int r = 255, g = 255, b = 255;

const int LED_ind = 7; // Led Indicator Pin
Average <int>Temp(10);

/*=============================================
  Refresh Temp Intervals
  ============================================= */
unsigned long Refresh_Temp_Interval = 1000; // the time we need to wait
unsigned long Temp_PreviousMillis = 0; // millis() returns an unsigned long

/*=============================================
  RF Setup
  ============================================= */
RF24 radio(9, 10);
const uint64_t pipes[2] = {0xB3B4B5B6CD , 0xB3B4B5B6CD};

typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles
role_e role = role_pong_back;              // The role of the current running sketch


//int const BLUE_PIN=3, RED_PIN=5,  GREEN_PIN=6;




void setup() {
  pinMode(LED_ind, OUTPUT);
  digitalWrite(LED_ind, HIGH);
  delay(100);
  digitalWrite(LED_ind, LOW);
  delay(100);
  digitalWrite(LED_ind, HIGH);
  delay(100);
  digitalWrite(LED_ind, LOW);

  Serial.begin(115200); // Serial Screen
  printf_begin();
  sensors.begin();
  //wdt_enable(WDTO_1S);
  // Setup and configure rf radio

  radio.begin();
  // radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();               // Allow optional ack payloads
  radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
  radio.openWritingPipe(pipes[1]);        // Both radios listen on the same pipes by default, and switch when writing
  radio.openReadingPipe(1, pipes[0]);
  radio.startListening();                 // Start listening
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
  radio.setPALevel(RF24_PA_MAX);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  Brightness_R = 250;
  Brightness_G = 250;
  Brightness_B = 250;

  digitalWrite(LED_ind, LOW);

  Brightness_B = constrain(Brightness_B, 0, 255);
  Brightness_G = constrain(Brightness_G, 0, 255);
  Brightness_R = constrain(Brightness_R, 0, 255);

  analogWrite (RED_PIN, 255);     // send the red brightness level to the red LED's pin
  analogWrite (GREEN_PIN, 255);
  analogWrite (BLUE_PIN, 255);
  delay(2);

}


void loop(void) {





  unsigned long Temp_Current_Millis = millis(); // grab current time
  if (Temp_Current_Millis - Temp_PreviousMillis > Refresh_Temp_Interval) {
    Temp_PreviousMillis = Temp_Current_Millis;
    TempRetrieve = sensors.getTempFByIndex(0);
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.print(" Degrees F from Probe: ");
    Serial.println(TempRetrieve);
    for (int i = 0; i < 10; i++)
    {
      Temp.push(TempRetrieve);
    }
    Serial.print("Mean: ");
    Serial.println(Temp.mean());
    TempSent = (Temp.mean());
    Temp_PreviousMillis = millis();
  }

  if (( role == role_pong_back ) && (TempSent > -30) && (TempSent < 110))  {

    byte pipeNo;
    byte gotByte;
    byte gotTempByte;
    byte  testByte;


    while ( radio.available(&pipeNo)) {

      radio.read( &gotByte, 1 );
      Serial.print(" gotByte: ");
      Serial.println(gotByte);

      //      if (gotByte != 80) {
      //        Serial.println(" Break!!!" );
      //        break;
      //      }
      delay(10);
      // *** Send Temp ***
      if (gotByte == 80) {
        //radio.writeAckPayload(pipeNo, &Test, 1 );
        // delay(5);

        digitalWrite(LED_ind, HIGH);
        delay(500);
        digitalWrite(LED_ind, LOW);
        radio.writeAckPayload(pipeNo, &TempSent, 1 );
        delay(50);
        Serial.print(" Sending Temp: " );
        Serial.println(TempSent);
        //        FLUSH_RX;
        //        FLUSH_TX;
      }
      // Set LED to White
      if (gotByte == 90) {
        // White
        r = 255, g = 255, b = 255;
        RGB(r, g, b);
      }
      // Set LED to Yellow
      if (gotByte == 95) {
        r = 255, g = 255, b = 0;
        RGB(r, g, b);
      }
      // Set LED to Green
      if (gotByte == 100) {
        r = 0; g = 255; b = 0;
        RGB(r, g, b);
      }
      // Set LED to Teal
      if (gotByte == 105) {
        r = 0; g = 255; b = 255;
        RGB(r, g, b);
      }

      // Set LED to Blue
      if (gotByte == 105) {
        r = 0; g = 0; b = 255;
        RGB(r, g, b);
      }

      // Set LED to Purple
      if (gotByte == 110) {
        r = 255; g = 0; b = 255;
        RGB(r, g, b);
      }

      // Set LED to Red
      if (gotByte == 115) {
        r = 255; g = 0; b = 0;
        RGB(r, g, b);
      }
      }
    }





  }

  void RGB(int R, int G, int B) {
    analogWrite(RED_PIN, R);
    analogWrite(GREEN_PIN, G);
    analogWrite(BLUE_PIN, B);
    delay(25);
  }



