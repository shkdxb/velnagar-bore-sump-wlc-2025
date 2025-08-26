/*
  Example animated analogue meters using a ILI9341 TFT LCD screen

  Needs Font 2 (also Font 4 if using large scale label)

  Make sure all the display driver and pin connections are correct by
  editing the User_Setup.h file in the TFT_eSPI library folder.

  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  #########################################################################
*/

// #include <TFT_eSPI.h> // Hardware-specific library
// #include <SPI.h>

// TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define TFT_GREY 0x5AEB

#define LOOP_PERIOD 35         // Display updates every 35 ms
float ltx = 0;                 // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update

int old_analog = -999;  // Value last displayed
int old_digital = -999; // Value last displayed

int value[6] = {0, 0, 0, 0, 0, 0};
int old_value[6] = {-1, -1, -1, -1, -1, -1};
int d = 0;

void intitializeTFTMeter();
void tftMeterUpdate();
void analogMeter(int quadrant);
void plotNeedle(int value, byte ms_delay, int quadrant);
void analogMeter1();
void plotNeedle1(int value, byte ms_delay);

void intitializeTFTMeter()
{
  // tft.init();
  // tft.setRotation(0);
  // Serial.begin(57600); // For debug
  // tft.fillScreen(TFT_BLACK);

  analogMeter(0); // Draw analogue meter
  analogMeter(1); // Draw analogue meter
  analogMeter(2); // Draw analogue meter
  analogMeter(3); // Draw analogue meter

  // Draw 6 linear meters
  // byte d = 40;
  // plotLinear("A0", 0, 160);
  // plotLinear("A1", 1 * d, 160);
  // plotLinear("A2", 2 * d, 160);
  // plotLinear("A3", 3 * d, 160);
  // plotLinear("A4", 4 * d, 160);
  // plotLinear("A5", 5 * d, 160);

  updateTime = millis(); // Next update time
}

void tftMeterUpdate()
{
  if (updateTime <= millis())
  {
    updateTime = millis() + LOOP_PERIOD;

    d += 4;
    if (d >= 360)
      d = 0;

    // value[0] = map(analogRead(A0), 0, 1023, 0, 100); // Test with value form Analogue 0

    // Create a Sine wave for testing
    value[0] = 150 + 150 * sin((d + 0) * 0.0174532925);
    value[1] =150 + 150 * sin((d + 60) * 0.0174532925);
    value[2] = 150 + 150 * sin((d + 120) * 0.0174532925);
    value[3] = 150 + 150 * sin((d + 180) * 0.0174532925);
    value[4] = 150 + 150 * sin((d + 240) * 0.0174532925);
    value[5] = 150 + 150 * sin((d + 300) * 0.0174532925);

    // unsigned long t = millis();

    // plotPointer();

    plotNeedle(value[0], 0, 0);
    plotNeedle(value[1], 0, 1);
    plotNeedle(value[2], 0, 2);
    plotNeedle(value[3], 0, 3);

    // Serial.println(millis()-t); // Print time taken for meter update
  }
}
// =============================================================
// Draw analogue meter in quadrant (0=TL,1=TR,2=BL,3=BR)
// =============================================================
void analogMeter(int quadrant)
{
  int HALF_W = DISPLAY_WIDTH / 2;
  int HALF_H = DISPLAY_HEIGHT / 2;

  // Quadrant origin
  int originX = (quadrant % 2) * HALF_W;
  int originY = (quadrant / 2) * HALF_H;

  // Local meter size
  int meterW = HALF_W;
  int meterH = HALF_H;

  // Center of dial (pivot)
  int cx = originX + meterW / 2;
  int cy = originY + meterH * 0.70;
  int radius = meterH * 0.45;

  // Background
  tft.fillRect(originX, originY, meterW, meterH, TFT_GREY);
  tft.fillRect(originX + 3, originY + 3, meterW - 6, meterH - 6,TFT_ORANGE);

  tft.setTextColor(TFT_BLACK);

  // -------- Step 1: Draw colored zones --------
  for (int i = -50; i < 50; i += 5)
  {
    int tl = meterH * 0.10;

    float sx = cos((i - 90) * DEG_TO_RAD);
    float sy = sin((i - 90) * DEG_TO_RAD);
    float sx2 = cos((i + 5 - 90) * DEG_TO_RAD);
    float sy2 = sin((i + 5 - 90) * DEG_TO_RAD);

    int x0 = cx + sx * (radius + tl);
    int y0 = cy + sy * (radius + tl);
    int x1 = cx + sx * radius;
    int y1 = cy + sy * radius;

    int x2 = cx + sx2 * (radius + tl);
    int y2 = cy + sy2 * (radius + tl);
    int x3 = cx + sx2 * radius;
    int y3 = cy + sy2 * radius;

    // Convert angle step to value range (0..300)
    float v1 = map(i, -50, 50, 0, 300);
    float v2 = map(i + 5, -50, 50, 0, 300);

    // Red zone ≤180 and ≥250
    if ((v1 < 180 && v2 <= 180) || (v1 >= 250 && v2 > 250))
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_RED);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_RED);
    }

    // Green zone 200–240
    if (v1 >= 200 && v2 <= 240)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }
  }

  // -------- Step 2: Draw arc --------
  for (int a = -50; a < 50; a++)
  {
    float sx = cos((a - 90) * DEG_TO_RAD);
    float sy = sin((a - 90) * DEG_TO_RAD);
    float sx2 = cos((a + 1 - 90) * DEG_TO_RAD);
    float sy2 = sin((a + 1 - 90) * DEG_TO_RAD);

    int x1 = cx + sx * radius;
    int y1 = cy + sy * radius;
    int x2 = cx + sx2 * radius;
    int y2 = cy + sy2 * radius;

    tft.drawLine(x1, y1, x2, y2, TFT_BLACK);
  }

  // -------- Step 3: Draw ticks & labels --------
  for (int i = -50; i <= 50; i += 5)
  {
    int tl = (i % 25 == 0) ? meterH * 0.10 : meterH * 0.06;

    float sx = cos((i - 90) * DEG_TO_RAD);
    float sy = sin((i - 90) * DEG_TO_RAD);

    int x0 = cx + sx * (radius + tl);
    int y0 = cy + sy * (radius + tl);
    int x1 = cx + sx * radius;
    int y1 = cy + sy * radius;

    tft.drawLine(x0, y0, x1, y1, TFT_BLACK);

    if (i % 25 == 0)
    {
      int lx = cx + sx * (radius + tl + meterH * 0.12);
      int ly = cy + sy * (radius + tl + meterH * 0.12);

      switch (i / 25)
      {
      case -2:
        tft.drawCentreString("0", lx, ly, 2);
        break;
      case -1:
        tft.drawCentreString("75", lx, ly, 2);
        break;
      case 0:
        tft.drawCentreString("150", lx, ly, 2);
        break;
      case 1:
        tft.drawCentreString("225", lx, ly, 2);
        break;
      case 2:
        tft.drawCentreString("300", lx, ly, 2);
        break;
      }
    }
  }

  // -------- Step 4: Labels & frame --------
  // tft.drawCentreString("Voltage", cx, cy + meterH * 0.15, 2); // just below pivot
  tft.drawString("Volts", originX + meterW * 0.65, originY + meterH * 0.85, 2);
  tft.drawRect(originX + 3, originY + 3, meterW - 6, meterH - 6, TFT_BLACK);

  // Start needle at 0
  plotNeedle(0, 0, quadrant);
}

void analogMeter1(int quadrant)
{
  int HALF_W = DISPLAY_WIDTH / 2;
  int HALF_H = DISPLAY_HEIGHT / 2;

  // Quadrant origin
  int originX = (quadrant % 2) * HALF_W;
  int originY = (quadrant / 2) * HALF_H;

  // Local meter size
  int meterW = HALF_W;
  int meterH = HALF_H;

  // Center of dial
  int cx = originX + meterW / 2;
  int cy = originY + meterH * 0.70; // pivot ~70% down
  int radius = meterH * 0.45;

  // Background
  tft.fillRect(originX, originY, meterW, meterH, TFT_GREY);
  tft.fillRect(originX + 3, originY + 3, meterW - 6, meterH - 6, TFT_WHITE);

  tft.setTextColor(TFT_BLACK);

  // Draw ticks -50 to +50 degrees
  for (int i = -50; i <= 50; i += 5)
  {
    int tl = (i % 25 == 0) ? meterH * 0.10 : meterH * 0.06;

    float sx = cos((i - 90) * DEG_TO_RAD);
    float sy = sin((i - 90) * DEG_TO_RAD);

    int x0 = cx + sx * (radius + tl);
    int y0 = cy + sy * (radius + tl);
    int x1 = cx + sx * radius;
    int y1 = cy + sy * radius;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * DEG_TO_RAD);
    float sy2 = sin((i + 5 - 90) * DEG_TO_RAD);
    int x2 = cx + sx2 * (radius + tl);
    int y2 = cy + sy2 * (radius + tl);
    int x3 = cx + sx2 * radius;
    int y3 = cy + sy2 * radius;

    // ---- ZONES ----
    if (i >= 0 && i < 25)
    {
      // Green zone
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }
    if (i >= 25 && i < 50)
    {
      // Orange zone
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_ORANGE);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_ORANGE);
    }

    // Short/long tick
    tft.drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Labels
    if (i % 25 == 0)
    {
      int lx = cx + sx * (radius + tl + meterH * 0.12);
      int ly = cy + sy * (radius + tl + meterH * 0.12);

      switch (i / 25)
      {
      case -2:
        tft.drawCentreString("0", lx, ly, 1);
        break;
      case -1:
        tft.drawCentreString("75", lx, ly, 1);
        break;
      case 0:
        tft.drawCentreString("150", lx, ly, 1);
        break;
      case 1:
        tft.drawCentreString("225", lx, ly, 1);
        break;
      case 2:
        tft.drawCentreString("300", lx, ly, 1);
        break;
      }
    }

    // Arc of scale (connect tick marks)
    if (i < 50)
    {
      int ax = cx + sx2 * radius;
      int ay = cy + sy2 * radius;
      tft.drawLine(ax, ay, x1, y1, TFT_BLACK);
    }
  }

  // Title and unit labels
  // Title text just below the pivot
  tft.drawCentreString("230 V", cx, cy + meterH * 0.05, 2);
  tft.drawString("Volts", originX + meterW * 0.65, originY + meterH * 0.85, 2);
  tft.drawRect(originX + 3, originY + 3, meterW - 6, meterH - 6, TFT_BLACK);

  // Start needle at 0
  plotNeedle(0, 0, quadrant);
}
// =============================================================
// Update needle for given quadrant (smooth movement)
// =============================================================
void plotNeedle(int value, byte ms_delay, int quadrant)
{
  int HALF_W = DISPLAY_WIDTH / 2;
  int HALF_H = DISPLAY_HEIGHT / 2;

  // Quadrant origin
  int originX = (quadrant % 2) * HALF_W;
  int originY = (quadrant / 2) * HALF_H;

  int meterW = HALF_W;
  int meterH = HALF_H;

  int cx = originX + meterW / 2;
  int cy = originY + meterH * 0.70;
  int radius = meterH * 0.45;

  // --- Persistent state per quadrant
  static float ltx[4] = {0, 0, 0, 0};
  static int osx[4] = {0, 0, 0, 0};
  static int osy[4] = {0, 0, 0, 0};
  static int old_val[4] = {-999, -999, -999, -999};

  // Clamp
  if (value < 0)
    value = 0;
  if (value > 300)
    value = 300;

  while (old_val[quadrant] != value)
  {
    if (old_val[quadrant] < value)
      old_val[quadrant]++;
    else
      old_val[quadrant]--;

    if (ms_delay == 0)
      old_val[quadrant] = value;

    float sdeg = map(old_val[quadrant], 0, 300, -150, -30);
    float sx = cos(sdeg * DEG_TO_RAD);
    float sy = sin(sdeg * DEG_TO_RAD);
    float tx = tan((sdeg + 90) * DEG_TO_RAD);

    // Erase old needle
    tft.drawLine(cx, cy, osx[quadrant] - 1, osy[quadrant], TFT_WHITE);
    tft.drawLine(cx, cy, osx[quadrant], osy[quadrant], TFT_WHITE);
    tft.drawLine(cx, cy, osx[quadrant] + 1, osy[quadrant], TFT_WHITE);

    // Store new coords
    osx[quadrant] = sx * (radius - 2) + cx;
    osy[quadrant] = sy * (radius - 2) + cy;

    // Draw new needle
    tft.drawLine(cx, cy, osx[quadrant] - 1, osy[quadrant], TFT_RED);
    tft.drawLine(cx, cy, osx[quadrant], osy[quadrant], TFT_MAGENTA);
    tft.drawLine(cx, cy, osx[quadrant] + 1, osy[quadrant], TFT_RED);

    delay(ms_delay);
  }

  // Value text
  char buf[8];
  sprintf(buf, "%3d", value);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawRightString(buf, originX + meterW * 0.25, originY + meterH * 0.85, 2);
}
// #########################################################################
//  Draw a linear meter on the screen
// #########################################################################
// void plotLinear(char *label, int x, int y)
// {
//   int w = 36;
//   tft.drawRect(x, y, w, 155, TFT_GREY);
//   tft.fillRect(x + 2, y + 19, w - 3, 155 - 38, TFT_WHITE);
//   tft.setTextColor(TFT_CYAN, TFT_BLACK);
//   tft.drawCentreString(label, x + w / 2, y + 2, 2);

//   for (int i = 0; i < 110; i += 10)
//   {
//     tft.drawFastHLine(x + 20, y + 27 + i, 6, TFT_BLACK);
//   }

//   for (int i = 0; i < 110; i += 50)
//   {
//     tft.drawFastHLine(x + 20, y + 27 + i, 9, TFT_BLACK);
//   }

//   tft.fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 - 5, TFT_RED);
//   tft.fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 + 5, TFT_RED);

//   tft.drawCentreString("---", x + w / 2, y + 155 - 18, 2);
// }

// // #########################################################################
// //  Adjust 6 linear meter pointer positions
// // #########################################################################
// void plotPointer(void)
// {
//   int dy = 187;
//   byte pw = 16;

//   tft.setTextColor(TFT_GREEN, TFT_BLACK);

//   // Move the 6 pointers one pixel towards new value
//   for (int i = 0; i < 6; i++)
//   {
//     char buf[8]; dtostrf(value[i], 4, 0, buf);
//     tft.drawRightString(buf, i * 40 + 36 - 5, 187 - 27 + 155 - 18, 2);

//     int dx = 3 + 40 * i;
//     if (value[i] < 0) value[i] = 0; // Limit value to emulate needle end stops
//     if (value[i] > 100) value[i] = 100;

//     while (!(value[i] == old_value[i])) {
//       dy = 187 + 100 - old_value[i];
//       if (old_value[i] > value[i])
//       {
//         tft.drawLine(dx, dy - 5, dx + pw, dy, TFT_WHITE);
//         old_value[i]--;
//         tft.drawLine(dx, dy + 6, dx + pw, dy + 1, TFT_RED);
//       }
//       else
//       {
//         tft.drawLine(dx, dy + 5, dx + pw, dy, TFT_WHITE);
//         old_value[i]++;
//         tft.drawLine(dx, dy - 6, dx + pw, dy - 1, TFT_RED);
//       }
//     }
//   }
// }
