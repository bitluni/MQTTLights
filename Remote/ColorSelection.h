#pragma once

short RGB24ToRGB16(int rgb)
{
  return ((rgb >> 3) & 0b11111) | (((rgb >> 10) & 0b111111) << 5) | ((rgb >> 8) & 0b1111100000000000);
}

int angleToColor(int angle)
{
  static const int colors[6][3] = {{0, 0, 255}, {0, 255, 255}, {0, 255, 0}, {255, 255, 0}, {255, 0, 0}, {255, 0, 255}};
  int f = (angle & 255) * 6;
  int ci0 = f >> 8;
  int ci1 = ci0 + 1;
  if(ci1 == 6) ci1 = 0;
  int s1 = f & 255;
  int s0 = 256 - s1;
  return  ((int)((colors[ci0][0] * s0 + colors[ci1][0] * s1) >> 8)) | 
          ((int)((colors[ci0][1] * s0 + colors[ci1][1] * s1) >> 8) << 8) |
          ((int)((colors[ci0][2] * s0 + colors[ci1][2] * s1) >> 8) << 16);
}

int getColor()
{
  int c = angleToColor(angle);
  int gr = 255;
  int s = saturation;
  int is = 256 - saturation;
  int r = ((c & 255) * s + is * gr) >> 8;
  int g = (((c >> 8) & 255) * s + is * gr) >> 8;
  int b = (((c >> 16) & 255) * s + is * gr) >> 8;
  return r | (g << 8) | (b << 16); 
}

int getFinalColor()
{
  int c = getColor();
  int br = brightness;
  int r = ((c & 255) * br) >> 8;
  int g = (((c >> 8) & 255) * br) >> 8;
  int b = (((c >> 16) & 255) * br) >> 8;
  return r | (g << 8) | (b << 16); 
}

void updateBrightness()
{
  int c = getColor();
  int s = 65536 / 199;
  int pos = ((brightness * 199) >> 8) + 20;
  for(int i = 0; i < 200; i++)
  {
    int r = ((c & 255) * s * i) >> 16;
    int g = (((c >> 8) & 255) * s * i) >> 16;
    int b = (((c >> 16) & 255) * s * i) >> 16;
    int d = abs(i + 20 - pos);
    if(d > 8 && d < 16)
    {
      r = g = b = d * 8;
    }
    gfx.Vline(i + 20, 0, 38, RGB24ToRGB16(r | (g << 8) | (b << 16)));
  }
  for(int i = 0; i < 20; i++)
  {
    int r = 0;
    int g = 0;
    int b = 0;
    int d = abs(i - pos);
    if(d > 8 && d < 16)
    {
      r = g = b = d * 8;
    }
    gfx.Vline(i, 0, 38, RGB24ToRGB16(r | (g << 8) | (b << 16)));
    r = c & 255;
    g = (c >> 8) & 255;
    b = c >> 16;
    d = abs(220 + i - pos);
    if(d > 8 && d < 16)
    {
      r = g = b = d * 8;
    }
    gfx.Vline(220 + i, 0, 38, RGB24ToRGB16(r | (g << 8) | (b << 16)));
  }
}


void updateColorSpace(int x0, int y0, int x1, int y1)
{
  gfx.setGRAM(x0, y0, x1 - 1, y1 - 1);
  for(int yi = y0; yi < y1; yi++)
    for(int xi = x0; xi < x1; xi++)
    {
      int x = xi - 120;
      int y = yi - 160;
      int r2 = x*x + y*y;      
      if(r2 < 120 * 120)
      {
          int x2 = xi - px;
          int y2 = yi - py;
          int r3 = x2*x2 + y2*y2; 
          if(r3 < 18 * 18 && r3 > 10 * 10)
            gfx.WrGRAM16((r3 >> 5) * 0b0000100001000001);
          else
            gfx.WrGRAM16(colorWheel[(y >> 1) + 60][(x >> 1) + 60]);
      }
      else
        gfx.WrGRAM16(0);
    }  
}

int updateColorField()
{
  gfx.RectangleFilled(0, 282, 120, 320, RGB24ToRGB16(getFinalColor()));
}

int updateColorUI()
{
  gfx.RoundRectFilled(125, 282, 240, 320, 15, LIGHTBLUE);
  gfx.TextColor(GRAY);
  gfx.MoveTo(140, 286);
  gfx.TextSize(2);
  gfx.print("Alarm");
}

void touchColorScreen(int tx, int ty)
{
  if(ty >= 40 && ty < 280)
  {
    int ox = px;
    int oy = py;
    int dx = tx - 120;
    int dy = 160 - ty;
    float a = atan2(dx, dy);
    angle = (int)(a / M_PI * 128) & 255;
    float r = min(100., sqrt(dx * dx + dy * dy));
    saturation = (int)(r * 2.56);
    px = 120 + int(sin(a) * r);
    py = 160 - int(cos(a) * r);
    /*Serial.print(px);
    Serial.print(" ");
    Serial.print(py);
    Serial.print(" ");
    Serial.println(saturation);*/
    updateColorSpace(ox - 20, oy - 20, ox + 20, oy + 20);
    updateColorSpace(px - 20, py - 20, px + 20, py + 20);
    updateBrightness();
    updateColorField();
    int f = getFinalColor();
    if(f != finalColor)
    {
      colorChanged = true;
      finalColor = f;
      writeEEPROMColor();
    }
  }
  else
  if(ty < 35)
  {
    brightness = ((min(max(20, tx), 219) - 20) * 256) / 199;
    updateBrightness();
    updateColorField();
    int f = getFinalColor();
    if(f != finalColor)
    {
      colorChanged = true;
      finalColor = f;
      writeEEPROMColor();
    }
  }
}


int toGray(int c)
{
  const int sr = (int)(65536 * 0.299);
  const int sg = (int)(65536 * 0.587);
  const int sb = (int)(65536 * 0.114);
  return ((c & 255) * sr + ((c >> 8) & 255) * sg + (((c >> 16) & 255) * sb)) >> 16; 
}

