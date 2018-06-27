#pragma once

const int buttonsAlarmOnOff[][4] = 
{{10, 18, 70, 34},
 {10, 58, 70, 34}};

const int buttonsAlarmHours[][4] = 
{
  {100, 18, 28, 19},
  {100, 38, 28, 19},
  {100, 58, 28, 19},
  {100, 78, 28, 19},
  {100, 98, 28, 19},
  {100, 118, 28, 19},
  {100, 138, 28, 19},
  {100, 158, 28, 19},
  {100, 178, 28, 19},
  {100, 198, 28, 19},
  {100, 218, 28, 19},
  {100, 238, 28, 19},
  {140, 18, 28, 19},
  {140, 38, 28, 19},
  {140, 58, 28, 19},
  {140, 78, 28, 19},
  {140, 98, 28, 19},
  {140, 118, 28, 19},
  {140, 138, 28, 19},
  {140, 158, 28, 19},
  {140, 178, 28, 19},
  {140, 198, 28, 19},
  {140, 218, 28, 19},
  {140, 238, 28, 19}};
  
const int buttonsAlarmMinutes[][4] = 
{
  {200, 18, 28, 19},
  {200, 38, 28, 19},
  {200, 58, 28, 19},
  {200, 78, 28, 19},
  {200, 98, 28, 19},
  {200, 118, 28, 19},
  {200, 138, 28, 19},
  {200, 158, 28, 19},
  {200, 178, 28, 19},
  {200, 198, 28, 19},
  {200, 218, 28, 19},
  {200, 238, 28, 19},
};
  
const char *buttonsAlarmHoursNames[] = 
{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
"12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"};

const char *buttonsAlarmMinutesNames[] = 
{"0", "5", "10", "15", "20", "25", "30", "35", "40", "45", "50", "55"};

void drawOption(const char *text, const int *button, int corner, int size, bool selected)
{
  gfx.TextSize(size);
  gfx.TextColor(GRAY);
  int len = strlen(text);
  int x = (button[2] - len * 8 * size) / 2 + button[0] + 1;
  int y = (button[3] - 16 * size) / 2 + button[1] + 1;
  gfx.RoundRectFilled(
      button[0], button[1], 
      button[0] + button[2], button[1] + button[3], corner, selected ? LIGHTGREEN : BLACK);
  gfx.MoveTo(x, y);
  gfx.print(text);
}

int updateAlarmOnOff()
{
    drawOption("OFF", buttonsAlarmOnOff[0], 8, 2, !alarmOn);
    drawOption("ON", buttonsAlarmOnOff[1], 8, 2, alarmOn);
}

int updateAlarmHours()
{
  for(int i = 0; i < 24; i++)
    drawOption(buttonsAlarmHoursNames[i], buttonsAlarmHours[i], 4, 1, alarmHours == i);
}
      
int updateAlarmMinutes()
{
  for(int i = 0; i < 12; i++)
    drawOption(buttonsAlarmMinutesNames[i], buttonsAlarmMinutes[i], 4, 1, alarmMinutes == i * 5);
}

int updateAlarmUI()
{
  gfx.RoundRectFilled(125, 282, 240, 320, 15, LIGHTBLUE);
  gfx.TextSize(2);
  gfx.TextColor(GRAY);
  gfx.MoveTo(140, 286);
  gfx.print("Color");
}

bool testButton(int x, int y, const int * button)
{
  return x >= button[0] && y >= button[1] && x < button[0] + button[2] && y < button[1] + button[3];
}

void alarmTouch(int tx, int ty)
{
  for(int i = 0; i < 2; i++)
    if(testButton(tx, ty, buttonsAlarmOnOff[i]))
    {
      bool newOn = i == 1;
      if(newOn != alarmOn)
      {
        alarmChanged = true;
        alarmOn = newOn;
        updateAlarmOnOff();
        writeEEPROMAlarm();
      }
    }
  for(int i = 0; i < 24; i++)
    if(testButton(tx, ty, buttonsAlarmHours[i]))
    {
      if(i != alarmHours)
      {
        alarmChanged = true;
        alarmHours = i;
        updateAlarmHours();
        writeEEPROMAlarm();
      }
    }    
  for(int i = 0; i < 12; i++)
    if(testButton(tx, ty, buttonsAlarmMinutes[i]))
    {
      if(i * 5 != alarmMinutes)
      {
        alarmChanged = true;
        alarmMinutes = i * 5;
        updateAlarmMinutes();
        writeEEPROMAlarm();
      }
    }
}

