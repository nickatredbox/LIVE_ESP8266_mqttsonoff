
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>


#define REALY_PIN 12
#define LED_PIN 13 //LED_BUILTIN is built in LED

const char ssid[] = "Cosmo";
const char pass[] = "Fairline19!";

WiFiClient net;
MQTTClient MQTTclient;

String inputString = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int RelayState  = LOW;

uint tick = 0;


uint addr = 0;

// EEPROM data
struct { 
  int RelayState;
} SavedData;


void setup() {
  
  pinMode(REALY_PIN, OUTPUT);
  
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
    RelayState  = HIGH;
    digitalWrite(REALY_PIN, RelayState);
    MQTTclient.publish("/mqttsonoff/status", "on");
  }
  else if(SavedData.RelayState == LOW)
  {
    RelayState  = LOW;
    digitalWrite(REALY_PIN, RelayState);
    MQTTclient.publish("/mqttsonoff/status", "off");
  }

}


void loop() {
  
  MQTTclient.loop();

  delay(10);  // <- fixes some issues with WiFi stability

  if (!MQTTclient.connected()) {
    WiFiConnect();
  }

  if( ((tick++) % 10)== 0 )
  {
     
      digitalWrite(LED_PIN, LOW);
  
      int rssi = WiFi.RSSI();
      
      JSONVar myArray;
      
      myArray[0] = RelayState;
      myArray[1] = rssi;
  
      String jsonString = JSON.stringify(myArray);
      
      MQTTclient.publish("/mqttsonoff/sensors", jsonString );
      Serial.println("/mqttsonoff/sensors" + jsonString );
  
      digitalWrite(LED_PIN, HIGH);
  }

  delay(100); 
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

  MQTTclient.subscribe("/mqttsonoff/control");  

  MQTTclient.publish("/mqttsonoff/status", "start");
  MQTTclient.publish("/mqttsonoff/localip", "/mqttsonoff/localip/" + WiFi.localIP().toString());
  
  digitalWrite(LED_PIN,HIGH);  
}

void messageReceived(String &topic, String &payload) {
  
  Serial.println("incoming: topic:" + topic + "  Payload:" + payload);

  digitalWrite(LED_PIN, LOW);

  if(topic.startsWith("/mqttsonoff/control"))
  {
    if(payload.startsWith("read"))
    {
      MQTTclient.publish("/mqttsonoff/localip", "/mqttsonoff/localip/" + WiFi.localIP().toString());
      
      if(RelayState == HIGH)
      {        
        MQTTclient.publish("/mqttsonoff/status", "on");
      }
      else if(RelayState == LOW)
      {
        MQTTclient.publish("/mqttsonoff/status", "off");
      }
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
            MQTTclient.publish("/mqttsonoff/status", "on");
          }
          else if(RelayState == LOW)
          {
            MQTTclient.publish("/mqttsonoff/status", "off");
          }

          digitalWrite(REALY_PIN, RelayState);
        }
        else if(payload.startsWith("ON"))
        {        
          RelayState  = HIGH;
          digitalWrite(REALY_PIN, RelayState);
          MQTTclient.publish("/mqttsonoff/status", "on");
        }
        else if(payload.startsWith("OFF"))
        {
          RelayState  = LOW;
          digitalWrite(REALY_PIN, RelayState);
          MQTTclient.publish("/mqttsonoff/status", "off");
        }

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
  
    MQTTclient.publish("/mqttsonoff/status", topic + "/" + payload);
    
    digitalWrite(LED_PIN, HIGH);
  }
}
