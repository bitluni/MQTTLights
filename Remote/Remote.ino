#include <ESP8266WiFi.h> 
#include <EEPROM.h>
#include "GFX4d.h"

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "colorWheel.h"

/************************* SETUP *********************************/
const char* wifiSSID = "YourWifiName";
const char* wifiPassword = "YourWifiPassword";

const char* mqttServer = "192.168.0.10";
const int   mqttPort = 1883;                   //8883 for SSL
const char* mqttUser = "username";
const char* mqttPassword = "password";
const char* colorFeed = "home/sleepingRoom/lightColor";
const char* alarmFeed = "home/sleepingRoom/lightAlarm";

const int BACKLIGHT_TIMEOUT = 10000;
const int EEPROMCommitDelay = 3000; //write after 3sec no change to color
/************************* Other settings ****************************/

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, mqttServer, mqttPort, mqttUser, mqttPassword);
Adafruit_MQTT_Publish color = Adafruit_MQTT_Publish(&mqtt, colorFeed);
Adafruit_MQTT_Publish alarm = Adafruit_MQTT_Publish(&mqtt, alarmFeed);
bool MQTTConnect();

GFX4d gfx = GFX4d();
int finalColor = 0;
bool colorChanged = false;

bool alarmChanged = false;
unsigned char alarmHours = 0;
unsigned char alarmMinutes = 0;
bool alarmOn = false;

short px = 120;
short py = 160;
unsigned char angle = 0;
short brightness = 256;
short saturation = 0;

int EEPROMCommit = 0;

enum Screen
{
  COLORPICKER = 0,
  ALARM = 1,
};

Screen currentScreen = COLORPICKER;

void readEEPROM()
{
  Serial.println("Read EEPROM");  
  angle = EEPROM.read(0);
  ((char*)&saturation)[0] = EEPROM.read(1);
  ((char*)&saturation)[1] = EEPROM.read(2);
  ((char*)&brightness)[0] = EEPROM.read(3);
  ((char*)&brightness)[1] = EEPROM.read(4);
  alarmOn = EEPROM.read(5) != 0;
  alarmHours = EEPROM.read(6);
  alarmMinutes = EEPROM.read(7);
  ((char*)&px)[0] = EEPROM.read(8);
  ((char*)&px)[1] = EEPROM.read(9);
  ((char*)&py)[0] = EEPROM.read(10);
  ((char*)&py)[1] = EEPROM.read(11);
}

void writeEEPROMAlarm()
{
  EEPROM.write(5, alarmOn ? 1 : 0);
  EEPROM.write(6, alarmHours);
  EEPROM.write(7, alarmMinutes);
  EEPROMCommit = EEPROMCommitDelay;
}

void writeEEPROMColor()
{
  EEPROM.write(0, angle);
  EEPROM.write(1, ((char*)&saturation)[0]);
  EEPROM.write(2, ((char*)&saturation)[1]);
  EEPROM.write(3, ((char*)&brightness)[0]);
  EEPROM.write(4, ((char*)&brightness)[1]);
  EEPROM.write(8, ((char*)&px)[0]);
  EEPROM.write(9, ((char*)&px)[1]);
  EEPROM.write(10, ((char*)&py)[0]);
  EEPROM.write(11, ((char*)&py)[1]);    
  EEPROMCommit = EEPROMCommitDelay;  
}

#include "ColorSelection.h"
#include "AlarmSelection.h"

void repaint()
{
  gfx.Cls();
  switch(currentScreen)
  {
    case COLORPICKER:
      updateColorSpace(0, 40, 240, 280);
      updateBrightness();
      updateColorField();
      updateColorUI();
    break;
    case ALARM:
      updateAlarmOnOff();
      updateAlarmHours();
      updateAlarmMinutes();
      updateAlarmUI();      
    break;
  }
}

void down(int tx, int ty)
{
  switch(currentScreen)
  {
    case COLORPICKER:
      touchColorScreen(tx, ty);
      if(tx > 125 && ty > 282)
      {
        currentScreen = ALARM;
        repaint();
      }
    break;
    case ALARM:
      alarmTouch(tx, ty);
      if(tx > 125 && ty > 282)
      {
        currentScreen = COLORPICKER;
        repaint();
      }
    break;
  }
}

void up()
{
  switch(currentScreen)
  {
    case COLORPICKER:
    break;
    case ALARM:
    break;
  }
}

void drag(int tx, int ty)
{
  switch(currentScreen)
  {
    case COLORPICKER:
      touchColorScreen(tx, ty);
    break;
    case ALARM:
      alarmTouch(tx, ty);
    break;
  }
}

void setup()
{
  WiFi.mode(WIFI_STA);  
  WiFi.begin(wifiSSID, wifiPassword);

  Serial.begin(115200);

  gfx.begin();
  gfx.Cls();
  gfx.ScrollEnable(false);
  gfx.BacklightOn(true);
  gfx.Orientation(PORTRAIT);
  //gfx.SmoothScrollSpeed(5);
  
  gfx.TextColor(WHITE); gfx.Font(2);  gfx.TextSize(2);

  gfx.touch_Set(TOUCH_ENABLE);
  EEPROM.begin(16);
  readEEPROM();  
  
  repaint();  
}

void loop()
{
  static bool inputLock = false;
  static bool touched = false;
  static int time = 0;
  int t = millis();
  int dt = t - time;
  time = t;
  static int backlightTimeOut = BACKLIGHT_TIMEOUT;
  backlightTimeOut -= dt;
  gfx.touch_Update();
  int tx = gfx.touch_GetX();
  int ty = gfx.touch_GetY();
  if (gfx.touch_GetPen() == TOUCH_PRESSED)
  {
    if(backlightTimeOut <= 0)
      inputLock = true;
    backlightTimeOut = BACKLIGHT_TIMEOUT;
    if(!inputLock)
    {
      if(!touched)
      {
        down(tx, ty);
        touched = true;
      }
      else
        drag(tx, ty);
    }
  }
  else
  {
    inputLock = false;
    if(touched)
    {
      touched = false;
      up();
    }
  }
  gfx.BacklightOn(backlightTimeOut > 0);

  static int lastDisconnect = 0;
  if(WiFi.status() == WL_CONNECTED && backlightTimeOut > 0)
  {
    if(mqtt.connected())
    {
      if(colorChanged)
      {
        color.publish(finalColor);
        colorChanged = false;
      }
      if(alarmChanged)
      {
        alarm.publish(alarmOn ? (alarmHours * 60 + alarmMinutes) * 60 : -1);
        alarmChanged = false;
      }
    }
    else
    {
      if(lastDisconnect > 1000)
      {
        if(mqtt.connect())
          mqtt.disconnect();
        lastDisconnect = 0;
      }
    }
    lastDisconnect += dt;
  }

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
}

