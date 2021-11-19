//WiFiManager Library for Interactive Web Portal
#include <WiFiManager.h>

//Firebase ESP Client Libraries
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

//Random generated Device ID to be used in Firebase RTDB
#define DEVICE_ID String("R0ZBDF9R1")

//Firebase RTDB URL AND API KEY Definition
#define DATABASE_URL "https://smartplug-edr-default-rtdb.europe-west1.firebasedatabase.app/"
#define DATABASE_SECRET "RpRUiHTZX0jrnJ1o9wxEtVWs1l0fxxSpKIYZIXW9"

#define DEBOUNCE_TIME 100
#define ONDEMAND_PORTAL_TIMEOUT 90 
#define WIFI_CONNECTION_FUNCDELAY 500
#define WIFI_CONNECTION_TIMEOUT 20000
#define WIFI_LED_BLINKING_TIME 75

#define WIFI_LED D0
#define RELAY_LED D1
#define WIFI_BUTTON D5
#define RELAY_BUTTON D6
#define RELAY D7

//No Delay Library Callback Functions Declaration
void LEDBlinkCallback();
void FirebaseTimerCallback();

//Firebase Library Objects
FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

//Non Blocking Delay Variables
unsigned long LEDMillis = 0;
unsigned long FirebaseMillis = 0;
unsigned long RELAY_BUTTON_MILLIS = 0;
unsigned long WIFI_BUTTON_MILLIS = 0;
unsigned long configPortalMillis = 0;
unsigned long WiFiConnectionMillis = 0;
unsigned long WiFiTimeoutMillis = 0;
unsigned long WiFiLEDMillis = 0;

//WiFiManager Library Object
WiFiManager wifimanager;

//Variables
String RTDBpath = "/" + DEVICE_ID;
bool isFirebaseConnected = false;
bool isStreamStarted = false;
bool relayStatus = false;
bool isOnDemandPortalOpen = false;
bool isWiFibegin = false;
int count = 0;
int state = LOW;      // the current state of the output pin
int reading;           // the current reading from the input pin
int previous = HIGH;    // the previous reading from the input pin


void WiFiSetup()
{
  //Explicitly set mode, esp defaults to STA+AP
  WiFi.mode(WIFI_STA);  

  //Set Portal Non-Blocking Mode
  wifimanager.setConfigPortalBlocking(false);
}

void ConnectWiFi()
{
  if((WiFi.status() != WL_CONNECTED))
  {
    
    if(millis() - WiFiConnectionMillis > WIFI_CONNECTION_FUNCDELAY)
    {
      WiFiConnectionMillis = millis();
      Serial.println("Connecting to WiFi...");
      if(!isWiFibegin)
      {
        WiFiTimeoutMillis = millis();
        WiFi.begin();
        isWiFibegin = true;
      }
    }
    
    if((millis() - WiFiTimeoutMillis > WIFI_CONNECTION_TIMEOUT) && (WiFi.status() != WL_CONNECTED))
    {
     Serial.println("Cannot connect to WiFi. Starting configuration portal...");
     wifimanager.startConfigPortal();
    }
   }
}

void streamCallback(FirebaseStream data)
{
  //Write stream data to relay
  relayStatus = !relayStatus;

  if(data.to<int>() == 0) relayStatus = 0;
  else if(data.to<int>() == 1) relayStatus = 1;

  Serial.println();
  Serial.println("-----NEW STREAM DATA RECIEVED-----");
  printResult(data); //addons/RTDBHelper.h
  Serial.println("-----NEW STREAM DATA RECIEVED-----");
  Serial.println();
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void resetData()
{
  Firebase.RTDB.setInt(&fbdo, RTDBpath + "/data", 0);
}

void StreamSetup()
{
  resetData();
  
  if (!Firebase.RTDB.beginStream(&stream, RTDBpath + "/data"))
  {
    Serial.printf("Stream Begin Error, %s\n\n", stream.errorReason().c_str());
  }
  else
  {
    Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
    isStreamStarted = true;
  }
}

void FirebaseSetup()
{
  Serial.println("Starting Firebase Setup");
  
  //Try to reconnect if WiFi gets disconnected
  Firebase.reconnectWiFi(true);

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  
  //Start Firebase RTDB Connection
  Firebase.begin(&config, &auth); 

  isFirebaseConnected = true;
  Serial.println("Firebase Setup OK");
}

void relayButtonToggle()
{
  reading = digitalRead(RELAY_BUTTON);
  
  if (reading == HIGH && previous == LOW && millis() - RELAY_BUTTON_MILLIS > DEBOUNCE_TIME) {
    if (state == HIGH)
    {
      state = LOW;
      relayStatus = state;
      Firebase.RTDB.setIntAsync(&fbdo, RTDBpath + "/data", state);
    }
    else
    {
      state = HIGH;
      relayStatus = state;
      Firebase.RTDB.setIntAsync(&fbdo, RTDBpath + "/data", state);
    }
    RELAY_BUTTON_MILLIS = millis();    
  }
  
  previous = reading;
}

void wifiButton()
{
  if((digitalRead(WIFI_BUTTON)) && (WiFi.status() == WL_CONNECTED) && (isOnDemandPortalOpen == false))
  {
    configPortalMillis = millis();
    wifimanager.startConfigPortal();
    isOnDemandPortalOpen = true;
  }

  if((isOnDemandPortalOpen) && (millis() - configPortalMillis > ONDEMAND_PORTAL_TIMEOUT*1000))
  {
    wifimanager.stopConfigPortal();
    isOnDemandPortalOpen = false;
  }
}

void WiFiLED()
{
  if((WiFi.status() != WL_CONNECTED) && (millis() - WiFiLEDMillis > WIFI_LED_BLINKING_TIME))
  {
    WiFiLEDMillis = millis();
    digitalWrite(WIFI_LED, !digitalRead(WIFI_LED));
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(WIFI_LED, HIGH);
  }
}

void setup()
{
  //Start Debug Serial
  Serial.begin(9600);

  //WiFi Setup
  WiFiSetup();

  //Starting Firebase related proccesses if WiFi Connection is successfull
  if(WiFi.status() == WL_CONNECTED) FirebaseSetup();

  //Set Onboard LED Output
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(WIFI_LED, OUTPUT);
  pinMode(RELAY_LED, OUTPUT);
  pinMode(WIFI_BUTTON, INPUT);
  pinMode(RELAY_BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
}

void loop()
{
  //WiFi Manager 
  wifimanager.process();

  //Connect to WiFi Network
  ConnectWiFi();

  //Connect to Firebase if the connection was made after the void setup function
  if((WiFi.status() == WL_CONNECTED) && (isFirebaseConnected == false)) FirebaseSetup();

  //Start Stream if Firebase connection is successful (Once)
  if(Firebase.ready() && (!isStreamStarted)) StreamSetup();

  //Relay Process
  relayButtonToggle();
  digitalWrite(RELAY, relayStatus);
  digitalWrite(RELAY_LED, relayStatus);
  
  //WiFi Button and LED Functions
  wifiButton();
  WiFiLED();
  
  //LED Blink
  if(millis() - LEDMillis > 1000)
  {
    LEDMillis = millis();
    LEDBlinkCallback();
  }

  //Sending test data to Firebase every second
  if(isFirebaseConnected && (Firebase.ready()) && (millis() - FirebaseMillis > 1000))
  {
    FirebaseMillis = millis();
    FirebaseTimerCallback();
  }
}

//LED Blink Callback Function
void LEDBlinkCallback()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void FirebaseTimerCallback()
{
  Serial.printf("Set int... %s\n", Firebase.RTDB.setIntAsync(&fbdo, RTDBpath + "/count", count++) ? "ok" : fbdo.errorReason().c_str());
}
