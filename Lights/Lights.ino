#include <ESP8266WiFi.h>
#define MAXSUBSCRIPTIONS 4
#define SUBSCRIPTIONDATALEN 20
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

/************************* SETUP *********************************/
const char* wifiSSID = "YourWifiName";
const char* wifiPassword = "YourWifiPassword";

const char* mqttServer = "192.168.0.10";
const int   mqttPort = 1883;                   //8883 for SSL
const char* mqttUser = "username";
const char* mqttPassword = "password";
const char* colorFeed = "home/sleepingRoom/lightColor";
const char* alarmFeed = "home/sleepingRoom/lightAlarm";
const char* time32Feed = "home/time/time32";
const char* discoFeed = "home/sleepingRoom/disco";
const int pixelPin = 5;
const int pixelCount = 104 + 13 + 104 + 13; //104 pixels long side, 13 pixels short side

const bool notifyAfterAlarm = true;
const bool notifyAfterAlarmMillis = 100000; //ten seconds 
const int EEPROMCommitDelay = 3000; //write after 3sec no change to color
/************************* Other settings ****************************/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(pixelCount, pixelPin, NEO_GRB + NEO_KHZ800);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, mqttServer, mqttPort, mqttUser, mqttPassword);
Adafruit_MQTT_Subscribe color = Adafruit_MQTT_Subscribe(&mqtt, colorFeed);
Adafruit_MQTT_Subscribe alarm = Adafruit_MQTT_Subscribe(&mqtt, alarmFeed);
Adafruit_MQTT_Subscribe time32 = Adafruit_MQTT_Subscribe(&mqtt, time32Feed);
Adafruit_MQTT_Subscribe discoSub = Adafruit_MQTT_Subscribe(&mqtt, discoFeed);

bool alarmOn = false;
bool alarmActive = false;
bool timeSet = false;
long currentTimeMillis = 0;
long currentTime = 0;
long alarmTime = 0;
int notify = 0;
int EEPROMCommit = 0;
int disco = 0;

const int DAY = 24 * 60 * 60;

#include "Audio.h"

//{color, duration},
const int alarmColorKeyframes[][2] = {
  {0x000000, 5},
  {0x200000, 5},
  {0x800000, 5},
  {0xD03000, 3},
  {0xff6000, 2},
  {0xffa050, 5},
  {0xffd080, 50},
  };
const int alarmColorKeyframeCount = 7;//sizeof(alarmColorKeyframes) / sizeof(int);
const int durationScale = 600;

unsigned char startColor[3] = {0, 0, 0};
unsigned char currentColor[3] = {0};
unsigned char targetColor[3] = {255, 255, 255};
int phase = 0;

void readEEPROM()
{
  Serial.println("Read EEPROM");  
  targetColor[0] = EEPROM.read(0);
  targetColor[1] = EEPROM.read(1);
  targetColor[2] = EEPROM.read(2);
  alarmOn = EEPROM.read(4) != 0;
  ((char*)&alarmTime)[0] = EEPROM.read(5);
  ((char*)&alarmTime)[1] = EEPROM.read(6);
  ((char*)&alarmTime)[2] = EEPROM.read(7);
  ((char*)&alarmTime)[3] = EEPROM.read(8);
}

void writeEEPROMAlarm()
{
  EEPROM.write(4, alarmOn ? 1 : 0);
  EEPROM.write(5, ((char*)&alarmTime)[0]);
  EEPROM.write(6, ((char*)&alarmTime)[1]);
  EEPROM.write(7, ((char*)&alarmTime)[2]);
  EEPROM.write(8, ((char*)&alarmTime)[3]);
  EEPROMCommit = EEPROMCommitDelay;
}

void writeEEPROMColor()
{
  EEPROM.write(0, targetColor[0]);
  EEPROM.write(1, targetColor[1]);
  EEPROM.write(2, targetColor[2]);
  EEPROMCommit = EEPROMCommitDelay;  
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  mqtt.subscribe(&color);
  mqtt.subscribe(&time32);
  mqtt.subscribe(&alarm);
  mqtt.subscribe(&discoSub);
  strip.begin();
  strip.show();
  EEPROM.begin(16);
  readEEPROM();
}

void stopNotify()
{
  alarmActive = false;
  if(notify <= 0) return;
  notify = 0;
  phase = 255;
}

void loop()
{
  static int lastDisconnect = 0;
  static int timeAtAlarm = 0;
  static bool wasConnected = false;
  static bool wasConnectedMQTT = false;
  static int lastUpdate = 0;
  int t = millis();
  static int time = 0;
  int dt = t - time;
  if (time == 0) dt = 0;
  time = t;

  if(disco != 4 || time - lastUpdate > 1000)  //disco mode 4 needs performance
  {
    lastUpdate = time;
    if (WiFi.status() == WL_CONNECTED)
    {
      if(!wasConnected)
        Serial.println("WiFi connected.");
      wasConnected = true;
      if (mqtt.connected())
      {
        if(!wasConnectedMQTT)
          Serial.println("MQTT connected.");
        wasConnectedMQTT = true;
        Adafruit_MQTT_Subscribe *subscription;
        while ((subscription = mqtt.readSubscription(20)))
        {
          if (subscription == &color)
          {
            Serial.print("Color: ");
            int c = atoi((char *)color.lastread);
            stopNotify();
            phase = 0;
            startColor[0] = currentColor[0]; startColor[1] = currentColor[1]; startColor[2] = currentColor[2];
            targetColor[0] = c >> 16; targetColor[1] = (c >> 8) & 255; targetColor[2] = c & 255;
            Serial.println(c, 16);
            writeEEPROMColor();
          }
          else if (subscription == &alarm)
          {
            Serial.print("Alarm: ");
            int a = atoi((char *)alarm.lastread);
            stopNotify();
            if (a < 0)
            {
              alarmOn = false;
            }
            else
            {
              alarmOn = true;
              alarmTime = a;
              //comment in for simple testing
              //alarmTime = currentTime + 3;
            }
            Serial.println(alarmTime);
            writeEEPROMAlarm();
          }
          else if (subscription == &time32)
          {
            //Serial.print("Time: ");
            unsigned int t = atoi((char *)time32.lastread);
            int hours = (t >> 12) & 31;
            int minutes = (t >> 6) & 63;
            int seconds = t & 63;
            currentTime = seconds + (minutes + hours * 60) * 60;
            timeSet = true;
            //Serial.println(currentTime);
          }
          else if (subscription == &discoSub)
          {
            Serial.print("Disco: ");
            disco = atoi((char *)discoSub.lastread);
            Serial.println(disco);
            if(disco == 0) phase = 0;
          }
        }
      }
      else
      {
        if(wasConnectedMQTT)
          Serial.println("MQTT disconnected.");
        wasConnectedMQTT = false;
        if (lastDisconnect > 1000)
        {
          if(mqtt.connect())
            mqtt.disconnect();
          lastDisconnect = 0;
        }
      }
      lastDisconnect += dt;
    }
    else
    {
      if(wasConnected)
        Serial.println("WiFi disconnected.");    
      wasConnected = false;
    }
  }
/**/

  //nothing changed for a while? write the EEPROM
  if(EEPROMCommit > 0)
  {
      EEPROMCommit -= dt;
      if(EEPROMCommit <= 0)
      {
        Serial.println("Write EEPROM");
        EEPROM.commit();
      }
  }

  //update time
  int oldTime = currentTime;
  currentTimeMillis += dt;
  currentTime += currentTimeMillis / 1000;
  currentTime = currentTime % DAY;
  currentTimeMillis = currentTimeMillis % 1000;
  //check for alarm
  if(alarmOn && timeSet)
  {
    if(!alarmActive)
    {
      int ot = oldTime;
      int ct = currentTime;
      if(ot > alarmTime)
      {
        ot -= DAY;
        ct -= DAY;
      }
      if(ct < ot)
        ct += DAY;
      if(ot < alarmTime && ct >= alarmTime)
      {
        Serial.print("ALARM!");
        alarmActive = true;
        timeAtAlarm = time;
      }
    }
    if(alarmActive)
    {
      int at = time - timeAtAlarm;
      int tsum = 0;
      for(int i = 0; i < alarmColorKeyframeCount; i++)
      {
        if(at >= tsum && at < tsum + alarmColorKeyframes[i][1] * durationScale)
        {
          int duration = alarmColorKeyframes[i][1] * durationScale;
          int c0 = alarmColorKeyframes[i][0];
          int c1 = alarmColorKeyframes[(i < alarmColorKeyframeCount - 1) ? i + 1 : i][0];
          float f1 = float(at - tsum) / duration;
          float f0 = 1.f - f1;
          float c[3];
          c[0] = (c0 >> 16) * f0 + (c1 >> 16) * f1; 
          c[1] = ((c0 >> 8) & 255) * f0 + ((c1 >> 8) & 255) * f1; 
          c[2] = (c0 & 255) * f0 + (c1 & 255) * f1;
          int ci[3];
          ci[0] = int(c[0]); ci[1] = int(c[1]); ci[2] = int(c[2]);
          int g[3];
          g[0] = (1 - (c[0] - ci[0])) * pixelCount; 
          g[1] = (1 - (c[1] - ci[1])) * pixelCount; 
          g[2] = (1 - (c[2] - ci[2])) * pixelCount;
          
          for (int j = 0; j < pixelCount; j++)
          {
            strip.setPixelColor(j, strip.Color(ci[0] + (j >= g[0] ? 1 : 0), ci[1] + (j >= g[1] ? 1 : 0), ci[2] + (j >= g[2] ? 1 : 0)));
          }
          strip.show();
          break;
        }
        if(i == alarmColorKeyframeCount - 1)
        {
          if(notifyAfterAlarm)
            notify = notifyAfterAlarmMillis;
            alarmActive = false;
        }
        tsum += alarmColorKeyframes[i][1] * durationScale;
      }
    }
  }

  if(notify > 0)
  {
    notify -= dt;
    if(notify <= 0)
    {
      phase = 255;
    }
    else
    {
      for (int i = 0; i < pixelCount; i++)
      {
        if((i + notify / 2) % pixelCount > pixelCount / 2)
          strip.setPixelColor(i, strip.Color(255, 0, 0));
        else
          strip.setPixelColor(i, strip.Color(0, 0, 255));
      }
      strip.show();
    }
  }
  
  if (phase < 256)
  {
    phase = min(256, phase + dt);
    int p0 = 256 - phase;
    currentColor[0] = (startColor[0] * p0 + targetColor[0] * phase) >> 8;
    currentColor[1] = (startColor[1] * p0 + targetColor[1] * phase) >> 8;
    currentColor[2] = (startColor[2] * p0 + targetColor[2] * phase) >> 8;
    for (int i = 0; i < pixelCount; i++)
      strip.setPixelColor(i, strip.Color(currentColor[0], currentColor[1], currentColor[2]));
    strip.show();
  }

  switch(disco)
  {
    case 1:
    {
      //int p = time / 10;
      int cs[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 0, 255}, {255, 255, 0}, {0, 255, 255}};
      for (int i = 0; i < pixelCount; i++)
      {
        int *c = cs[(((i + t / 10) / 20)) % 6];
        strip.setPixelColor(i, strip.Color(c[0], c[1], c[2]));     
      }
      strip.show(); 
      break;
    }
    case 2:
    {
      //int p = time / 10;
      int cs[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 0, 255}, {255, 255, 0}, {0, 255, 255}};
      for (int i = 0; i < pixelCount; i++)
      {
        int *c = cs[((i / 20) + t / 100) % 6];
        strip.setPixelColor(i, strip.Color(c[0], c[1], c[2]));     
      }
      strip.show(); 
      break;
    }
    case 3:
    {      
    for (int i = 0; i < pixelCount; i++)
      {
        strip.setPixelColor(i, strip.Color(sin(t * 0.002 + i * 0.01) * 127 + 128, sin(t * 0.003 + i * 0.03) * 127 + 128, sin(t * 0.001 + i * 0.02) * 127 + 128));     
      }
      strip.show(); 
      break;
    }
    case 4:
    {
      analyze();
      static int oldTime = 0;
      if(time - oldTime >= 30)
      {
        oldTime = time;
        display();
      }
      break;
    }
  }
}