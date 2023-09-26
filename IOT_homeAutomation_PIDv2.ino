

#ifdef ENABLE_DEBUG
#define DEBUG_ESP_PORT Serial
#define NODEBUG_WEBSOCKETS
#define NDEBUG
#endif

#include <Arduino.h>
#include "DHT.h"

#include <WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <map>

#define WIFI_SSID "Kafka 3"
#define WIFI_PASS "11223344"
#define APP_KEY "82367567-01d9-4f6e-bcd1-f56c43867562"                                          // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET "60211e7f-76d4-44f5-9fd6-af20c85e55de-9dacc631-dfde-496e-99c8-a4f901a654ec"  // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define DHTTYPE DHT11

//Enter the device IDs here
#define device_ID_1 "63a28c972ee87004c2d03c38"
#define device_ID_2 "63a28ff52ee87004c2d0421c"
#define device_ID_3 "63a29014e0013e205089ab90"
// define the GPIO connected with Relays and switches
#define RelayPin1 23  //D23
#define RelayPin2 19  //D22
#define RelayPin3 34  //D21
#define DHTPIN 4      //D4 for DHT SENSOR
#define RelayPin4 32  //D19
// #define fanpin 25 // D25
#define smokesensepin 2  //D3
// #define buzzpin 18
/*LED pin defined*/

const int ledPin = 16;

#define SwitchPin1 13  //D13
#define SwitchPin2 12  //D12
#define SwitchPin3 14  //D14
//#define SwitchPin4 27  //D27

#define wifiLed 2  //D2


// comment the following line if you use a toggle switches instead of tactile buttons
//#define TACTILE_BUTTON 1

#define BAUD_RATE 9600

#define DEBOUNCE_TIME 250

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

typedef struct {  // struct for the std::map below
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;



std::map<String, deviceConfig_t> devices = {
  //{deviceId, {relayPIN,  flipSwitchPIN}}
  { device_ID_1, { RelayPin1, SwitchPin1 } },
  { device_ID_2, { RelayPin2, SwitchPin2 } },
  // {device_ID_3, {  RelayPin3, SwitchPin3 }},
  //{device_ID_4, {  RelayPin4, SwitchPin4 }}
};

typedef struct {  // struct for the std::map below
  String deviceId;
  bool lastFlipSwitchState;
  unsigned long lastFlipSwitchChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;  // this map is used to map flipSwitch PINs to deviceId and handling debounce and last flipSwitch state checks
                                                 // it will be setup in "setupFlipSwitches" function, using informations from devices map

void setupRelays() {
  for (auto &device : devices) {            // for each device (relay, flipSwitch combination)
    int relayPIN = device.second.relayPIN;  // get the relay pin
    pinMode(relayPIN, OUTPUT);              // set relay pin to OUTPUT
    digitalWrite(relayPIN, HIGH);
  }
}
DHT dht(DHTPIN, DHTTYPE);

void setupDHT() {
  pinMode(RelayPin4, OUTPUT);
  digitalWrite(RelayPin4, HIGH);
}

void handleEmergency() {

  // delay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print(F("%  Temperature: "));
  Serial.println(t);
  Serial.print(F("°C "));
  Serial.println(f);
  if (t > 45) {
    digitalWrite(RelayPin4, LOW);  //CHECK IF WORKS
  } else {
    digitalWrite(RelayPin4, HIGH);
  }
}
void setupFlipSwitches() {
  for (auto &device : devices) {          // for each device (relay / flipSwitch combination)
    flipSwitchConfig_t flipSwitchConfig;  // create a new flipSwitch configuration

    flipSwitchConfig.deviceId = device.first;     // set the deviceId
    flipSwitchConfig.lastFlipSwitchChange = 0;    // set debounce time
    flipSwitchConfig.lastFlipSwitchState = true;  // set lastFlipSwitchState to false (LOW)--

    int flipSwitchPIN = device.second.flipSwitchPIN;  // get the flipSwitchPIN

    flipSwitches[flipSwitchPIN] = flipSwitchConfig;  // save the flipSwitch config to flipSwitches map
    pinMode(flipSwitchPIN, INPUT_PULLUP);            // set the flipSwitch pin to INPUT
  }
}

bool onPowerState(String deviceId, bool &state) {
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN;  // get the relay pin for corresponding device
  digitalWrite(relayPIN, !state);             // set the new relay state
  return true;
}

void handleFlipSwitches() {
  unsigned long actualMillis = millis();                                          // get actual millis
  for (auto &flipSwitch : flipSwitches) {                                         // for each flipSwitch in flipSwitches map
    unsigned long lastFlipSwitchChange = flipSwitch.second.lastFlipSwitchChange;  // get the timestamp when flipSwitch was pressed last time (used to debounce / limit events)

    if (actualMillis - lastFlipSwitchChange > DEBOUNCE_TIME) {  // if time is > debounce time...

      int flipSwitchPIN = flipSwitch.first;                              // get the flipSwitch pin from configuration
      bool lastFlipSwitchState = flipSwitch.second.lastFlipSwitchState;  // get the lastFlipSwitchState
      bool flipSwitchState = digitalRead(flipSwitchPIN);                 // read the current flipSwitch state
      if (flipSwitchState != lastFlipSwitchState) {                      // if the flipSwitchState has changed...
#ifdef TACTILE_BUTTON
        if (flipSwitchState) {  // if the tactile button is pressed
#endif
          flipSwitch.second.lastFlipSwitchChange = actualMillis;  // update lastFlipSwitchChange time
          String deviceId = flipSwitch.second.deviceId;           // get the deviceId from config
          int relayPIN = devices[deviceId].relayPIN;              // get the relayPIN from config
          bool newRelayState = !digitalRead(relayPIN);            // set the new relay State
          digitalWrite(relayPIN, newRelayState);                  // set the trelay to the new state

          SinricProSwitch &mySwitch = SinricPro[deviceId];  // get Switch device from SinricPro
          mySwitch.sendPowerStateEvent(!newRelayState);     // send the event
#ifdef TACTILE_BUTTON
        }
#endif
        flipSwitch.second.lastFlipSwitchState = flipSwitchState;  // update lastFlipSwitchState
      }
    }
  }
}
int smokedata = 0;
float t = 0;

void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  digitalWrite(wifiLed, HIGH);
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro() {
  for (auto &device : devices) {
    const char *deviceId = device.first.c_str();
    SinricProSwitch &mySwitch = SinricPro[deviceId];
    mySwitch.onPowerState(onPowerState);
  }

  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);
}



void setup() {
  Serial.begin(BAUD_RATE);
  Serial.println(F("DHTxx test!"));
  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;)
      ;
  }

  delay(500);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(4, 35);
  display.print(" MEASUREMENT SYSTEM ");
  display.setCursor(26, 54);
  display.print(" Loading");
  display.display();

  for (int i = 0; i < 4; i++) {
    display.print(".");
    display.display();
    delay(500);
  }
  display.clearDisplay();

  pinMode(wifiLed, OUTPUT);
  pinMode(smokesensepin, INPUT);
  digitalWrite(wifiLed, LOW);
  // pinMode(buzzpin,OUTPUT);
  // digitalWrite(buzzpin, HIGH);

  // configure LED PWM functionalitites
  ledcSetup(ledChannel, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(ledPin, ledChannel);


  setupRelays();
  setupFlipSwitches();
  setupDHT();
  setupWiFi();
  setupSinricPro();
}

void loop() {
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  if ( isnan(t) ) {
    Serial.println(F("Failed to read from DHT sensor!"));
    //t = ;
    delay(500);
      Serial.println(t);      
  t = dht.readTemperature();   
  return;
  }
  Serial.print(F("%  Temperature: "));
  Serial.println(t);
  // Serial.print(F("°C "));
  // Serial.println(f);

if ( t < 25) {
      ledcWrite(ledChannel, 0);
      delay(15);
t = dht.readTemperature();      
      SinricPro.handle();
  }  

  if (t >= 25 && t < 30) {
      ledcWrite(ledChannel, 120);
      delay(15);
t = dht.readTemperature();      
      SinricPro.handle();
  }

  if (t >= 30 && t < 35) {
      ledcWrite(ledChannel, 180);
      delay(15);
t = dht.readTemperature();      
      SinricPro.handle();
    
  }

  if (t >= 35) {
      ledcWrite(ledChannel, 240);
      delay(15);
    t = dht.readTemperature();   
SinricPro.handle();   
  }

//   if (t >= 40) {
//     for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++) {
//       // changing the LED brightness with PWM
//       ledcWrite(ledChannel, dutyCycle);
//       delay(15);
            
// t = dht.readTemperature(); 
// SinricPro.handle();
//    }
    
//   }





Serial.println("Smokedata");
// smokedata = analogRead(smokesensepin);
// Serial.println(smokedata);
smokedata = digitalRead(smokesensepin);

if (smokedata == 0) {
  Serial.println("Smoke Detected");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(" SMOKE DETECTED! ");
  display.display();
  display.clearDisplay();
} else {
  Serial.println(smokedata);
  display.setTextSize(1); 
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(" SMOKE NOT DETECTED! ");
  display.display();
  display.clearDisplay();
}


SinricPro.handle();
//handleEmergency();
//handleFlipSwitches();
}
