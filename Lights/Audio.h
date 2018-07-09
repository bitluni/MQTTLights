#pragma once
const int msPerPixel = 4;
int currentMaxValue = 0;
int maxValues[pixelCount] = {0};
int colors[pixelCount][3] = {0};

void analyze()
{
  static int color[3] = {0};
  static bool under = true;
  static double level = 512;
  static int maxValue = 0;
  int a = analogRead(A0);
  level = level * 0.999 + a * 0.001;
  a -= int(level);
  int time = millis();
  static int oldTime = 0;

  if(time - oldTime >= msPerPixel)
  {
     maxValues[currentMaxValue] = maxValue;
     if(maxValue > 200 && under)
     {
        color[0] = rand() & 255;
        color[1] = rand() & 255;
        color[2] = rand() & 255;
        under = false;
     }
     else
      under = true;
     colors[currentMaxValue][0] = color[0];
     colors[currentMaxValue][1] = color[1];
     colors[currentMaxValue][2] = color[2];
     maxValue = 0;
     currentMaxValue = (currentMaxValue + 1) % pixelCount;
     oldTime = time;
  }

  maxValue = max(maxValue, abs(a));
}

void display()
{ 
  for (int i = 0; i < pixelCount; i++)
  {
    int j = (i + currentMaxValue) % pixelCount;
    int v = maxValues[j];
    int w = max(0, min((v * v - 300) / 100, 256));
    int r = (w * colors[j][0]) >> 8;
    int g = (w * colors[j][1]) >> 8;
    int b = (w * colors[j][2]) >> 8;
    strip.setPixelColor(i, strip.Color(r, g, b));     
  }
  strip.show(); 
  Serial.println(maxValues[currentMaxValue]);
}

