
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <MQTT.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>


#define RELAY_PIN 12
#define LED_PIN 13 //LED_BUILTIN is built in LED

const char ssid[] = "Cosmo";
const char pass[] = "Fairline19!";
String device = "mqttsonoff";

WiFiClient net;
MQTTClient MQTTclient;


String inputString = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int RelayState  = LOW;

unsigned long tick = 0;
unsigned long UpdateCount = 6;
unsigned long CountDownSeconds = 0;
unsigned long CountDown = 0;
uint addr = 0;

// EEPROM data
struct { 
  int RelayState;
  unsigned long UpdateCount;
} SavedData;


void setup() {

  pinMode(RELAY_PIN, OUTPUT);
  
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);

  WiFi.begin(ssid, pass);

  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported by Arduino.
  // You need to set the IP address directly.
  MQTTclient.begin("192.168.1.71", net);
  MQTTclient.onMessage(messageReceived);

  WiFiConnect();

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into 
  // a 512-byte-array cache in RAM
  EEPROM.begin(512);

  
  // read bytes (i.e. sizeof(data) from "EEPROM"),
  // in reality, reads from byte-array cache
  // cast bytes into structure called data
  EEPROM.get(addr,SavedData);

  if(SavedData.RelayState == HIGH)
  {        
    control("ON");
  }
  else if(SavedData.RelayState == LOW)
  {
    control("OFF");
  }

}


void WiFiConnect() {
  
  Serial.print("checking wifi...");
  Serial.println(ssid);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
  }
  
  Serial.println("\nWiFi Connected! ");
  Serial.println( WiFi.localIP());

  Serial.print("\nMQTT connecting...");
  while (!MQTTclient.connect(WiFi.localIP().toString().c_str() , "try", "try")) {
    Serial.print(".");
    delay(800);
    digitalWrite(LED_PIN, LOW);
    delay(200);
    digitalWrite(LED_PIN, HIGH);
  }

  Serial.println("\nMQTT Connected!");

  MQTTclient.subscribe("/" + device + "/control");  

  MQTTclient.publish("/" + device + "/status", "start");
  MQTTclient.publish("/" + device + "/localip", "/" + device + "/localip/" + WiFi.localIP().toString());
  
  digitalWrite(LED_PIN,HIGH);  
}


void loop() {
  
  MQTTclient.loop();

  delay(10);  // <- fixes some issues with WiFi stability

  if (!MQTTclient.connected()) {
    WiFiConnect();
  }

  if( (tick % (UpdateCount * 10))== 0 )
  {
     SendData();
  }

  if( (CountDownSeconds > 0) && ( (tick % 10) == 0) )
  {    
    if( CountDown == 0 )
    {
      CountDownSeconds = 0;
      control("OFF");  
    }
    else
      CountDown--;
         
  }
  
  tick++;

  delay(90); 
}

void SendData()
{
    digitalWrite(LED_PIN, LOW);

    int rssi = WiFi.RSSI();
    
    JSONVar myArray;
    
    myArray[0] = RelayState;
    myArray[1] = rssi;
    myArray[2] = CountDown;
    myArray[3] = tick;
    myArray[4] = UpdateCount;

    String jsonString = JSON.stringify(myArray);
    
    MQTTclient.publish("/" + device + "/sensors", jsonString );
    Serial.println("/" + device + "/sensors" + jsonString );

    digitalWrite(LED_PIN, HIGH);
}


void control(String payload) 
{
  String topic = "/" + device + "/control";
  DeviceControl(topic, payload);
}

void  messageReceived(String &topic, String &payload) {

  DeviceControl(topic, payload);
}
  
void DeviceControl(String topic, String payload) {
  
  Serial.println("incoming: topic:" + topic + "  Payload:" + payload);

  digitalWrite(LED_PIN, LOW);

  if(topic.startsWith("/" + device + "/control"))
  {    
    if(payload.startsWith("CountDown"))
    {
      CountDownSeconds = (long)payload.substring(10).toInt();
     
      CountDown = CountDownSeconds;

      if(CountDownSeconds > 0)
      {
        RelayState  = HIGH;
        digitalWrite(RELAY_PIN, RelayState);
        MQTTclient.publish("/" + device + "/status", "on");        
      }
      else
      {
        RelayState  = LOW;
        digitalWrite(RELAY_PIN, RelayState);
        MQTTclient.publish("/" + device + "/status", "off");       
      }
      
      SendData();
    }
    else if(payload.startsWith("UpdateCount"))
    {
      UpdateCount =(long)payload.substring(12).toInt();
      if(UpdateCount > 120) UpdateCount = 120;
      if(UpdateCount < 1) UpdateCount = 1;
      SaveEEPROM();
    }
    else if(payload.startsWith("read"))
    {
      MQTTclient.publish("/" + device + "/localip", "/" + device + "/localip/" + WiFi.localIP().toString());
      
      if(RelayState == HIGH)
      {        
        MQTTclient.publish("/" + device + "/status", "on");
      }
      else if(RelayState == LOW)
      {
        MQTTclient.publish("/" + device + "/status", "off");
      }
      
      SendData();
    }
    else if(topic.indexOf("control") > 0)
    {         
        if(payload.startsWith("toggle"))
        {        
          if(RelayState == HIGH)
            RelayState  = LOW;
          else
            RelayState  = HIGH;         
            
          if(RelayState == HIGH)
          {        
            MQTTclient.publish("/" + device + "/status", "on");
          }
          else if(RelayState == LOW)
          {
            MQTTclient.publish("/" + device + "/status", "off");
          }

          digitalWrite(RELAY_PIN, RelayState);
         
        }
        else if(payload.startsWith("ON"))
        {        
          CountDown = 0;
          CountDownSeconds = 0;
          RelayState  = HIGH;
          digitalWrite(RELAY_PIN, RelayState);
          MQTTclient.publish("/" + device + "/status", "on");        
        }
        else if(payload.startsWith("OFF"))
        {
          CountDown = 0;
          CountDownSeconds = 0;
          RelayState  = LOW;
          digitalWrite(RELAY_PIN, RelayState);
          MQTTclient.publish("/" + device + "/status", "off");         
        }
        
        SendData();
        
        SaveEEPROM();
       
    }
  
    MQTTclient.publish("/" + device + "/status", topic + "/" + payload);
    
    digitalWrite(LED_PIN, HIGH);
  }
}

void SaveEEPROM()
{
 
    SavedData.UpdateCount = UpdateCount;
    SavedData.RelayState = RelayState;

    // replace values in byte-array cache with modified data
    // no changes made to flash, all in local byte-array cache
    EEPROM.put(addr,SavedData);
    
    // actually write the content of byte-array cache to
    // hardware flash.  flash write occurs if and only if one or more byte
    // in byte-array cache has been changed, but if so, ALL 512 bytes are 
    // written to flash
    EEPROM.commit();  
}
