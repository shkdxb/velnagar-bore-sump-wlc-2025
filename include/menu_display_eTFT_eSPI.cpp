
// String tickerMsg = "IP: " + WiFi.localIP() + WiFi.localIP().toString() + " | Bore Error:" + boreErrorMessage + " | Sump error: " + sumpErrorMessage;
int tickerX;
int tickerHeight = 20; // reserve 36px for ticker

// int menuIndex = 0;    // Current selected item
bool editing = false; // Edit mode flag
int currentPage = 0;  // Page number

// ==== Labels ====
/*
0	Bore
1	Sump
2	Motor
3	Underground
4	Overhead
5	Tank
6	VOLT Detect
7	OVER Voltage
8	UNDER Voltage
9	AMP Detect
10	OVER Current
11	UNDER Current
12	DRY Run Detection
13	MIN PF
14	CYCLIC Timer
15	ON Time
16	OFF Time
17	POWER ON Delay
18	Voltage
19	Current
20	Power
21	PF
22	Energy
23	Mode
*/
const char *labels[] = {"Bore", "Sump", "Motor", "U-ground", "O-head", "Tank",
                        "VOLT Detect", "OVER Voltage", "UNDER Voltage", "AMP Detect",
                        "OVER Current", "UNDER Current", "DRYRun Detect", "MIN PF",
                        "CYCLIC Timer", "ON Time", "OFF Time", "P-ON Delay",
                        "Voltage", "Current", "Power", "PF", "Energy", "Mode"};

#define MENU_COUNT 12
#define STATUS_COUNT 22
#define ROWS_PER_PAGE 5
#define ROW_HEIGHT 36 // row spacing

int drawTickerLine(TFT_eSprite &sprite);
void updateTicker();
void drawMenuLabels();
void updateValue(int i, bool highlight);
void drawAllValues();
void drawStatusLine(int row, const char *label, const String &value, uint16_t fg = TFT_CYAN, uint16_t bg = TFT_BLACK, bool highlight = false);
void drawStatusScreen(bool isBore);

int drawTickerLine(TFT_eSprite &sprite)
{
    sprite.fillSprite(TFT_BLACK);
    sprite.setCursor(0, 0);

    int msgW = 0;

    auto printAndMeasure = [&](const String &txt)
    {
        sprite.print(txt);
        return sprite.textWidth(txt);
    };

    auto printAndMeasureFmt = [&](const char *fmt, ...)
    {
        char buf[64]; // adjust size if needed
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        sprite.print(buf);
        return sprite.textWidth(buf);
    };

    // IP
    sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    msgW += printAndMeasure("IP: ");
    msgW += printAndMeasure(WiFi.localIP().toString());
    msgW += printAndMeasure(" | Bore Status: ");

    // Bore
    if (boreMode == 2)
    {
        sprite.setTextColor(TFT_RED, TFT_BLACK);
        msgW += printAndMeasure(boreErrorMessage);
    }
    else
    {
        sprite.setTextColor(TFT_GREEN, TFT_BLACK);
        if (boreMode == 1)
        {
            msgW += printAndMeasureFmt("Waiting - Remaining Time: %d Min",
                                       boreSettings.offTime - ((millis() - boreLastOffTime) / (1000 * 60)));
        }
        else if (boreMode == 3)
        {
            msgW += printAndMeasureFmt("ON - Remaining Time: %d Min",
                                       boreSettings.onTime - ((millis() - boreLastOnTime) / (1000 * 60)));
        }
        else if (boreMode == 4)
        {
            msgW += printAndMeasure("OFF");
        }
        else
        {
            msgW += printAndMeasure("NA");
        }
    }

    // Sump
    sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    msgW += printAndMeasure(" | Sump Status: ");

    if (sumpMode == 2)
    {
        sprite.setTextColor(TFT_RED, TFT_BLACK);
        msgW += printAndMeasure(sumpErrorMessage);
    }
    else
    {
        sprite.setTextColor(TFT_GREEN, TFT_BLACK);
        if (sumpMode == 1)
        {
            msgW += printAndMeasureFmt("Waiting - Remaining Time: %d Min",
                                       sumpSettings.offTime - ((millis() - sumpLastOffTime) / (1000 * 60)));
        }
        else if (sumpMode == 3)
        {
            msgW += printAndMeasureFmt("ON - Remaining Time: %d Min",
                                       sumpSettings.onTime - ((millis() - sumpLastOnTime) / (1000 * 60)));
        }
        else if (sumpMode == 4)
        {
            msgW += printAndMeasure("OFF");
        }
        else
        {
            msgW += printAndMeasure("NA");
        }
    }

    return msgW;
}

void updateTicker()
{
    static unsigned long lastScroll = 0;
    if (millis() - lastScroll <= 40) // bottom scroll text
    {
        return;
    }

    tickerSprite.fillSprite(TFT_BLACK);
    // draw message into sprite
    tickerSprite.setCursor(tickerX, 4); // small margin top
    // tickerSprite.print(tickerMsg);
    int boreRemmainingOffTime = (boreSettings.offTime * 60) - ((millis() - boreLastOffTime) / 1000); // in sec
    int boreRemmainingOnTime = (boreSettings.onTime * 60) - ((millis() - boreLastOnTime) / 1000);    // in sec
    int sumpRemmainingOffTime = (sumpSettings.offTime * 60) - ((millis() - sumpLastOffTime) / 1000); // in sec
    int sumpRemmainingOnTime = (sumpSettings.onTime * 60) - ((millis() - sumpLastOnTime) / 1000);    // in sec

    String tickerMsg1 = String("IP: ") + WiFi.localIP().toString();
    String tickerMsg2 = " | Bore Status:" +
                        (boreMode == 1
                             ? String("Waiting - Remaining Time: ") + (boreRemmainingOffTime > 60 ? String(boreRemmainingOffTime / 60) + " Min" : String(boreRemmainingOffTime) + " Sec")
                         : boreMode == 2
                             ? boreErrorMessage
                         : boreMode == 3
                             ? String("ON - Remaining Time: ") + (boreRemmainingOnTime > 60 ? String(boreRemmainingOnTime / 60) + " Min" : String(boreRemmainingOnTime) + " Sec")
                         : boreMode == 4
                             ? "OFF"
                             : "NA");
    String tickerMsg3 = " | Sump Status:" +
                        (sumpMode == 1
                             ? String("Waiting - Remaining Time: ") + (sumpRemmainingOffTime > 60 ? String(sumpRemmainingOffTime / 60) + " Min" : String(sumpRemmainingOffTime) + " Sec")
                         : sumpMode == 2
                             ? sumpErrorMessage
                         : sumpMode == 3
                             ? String("ON - Remaining Time: ") + (sumpRemmainingOnTime > 60 ? String(sumpRemmainingOnTime / 60) + " Min" : String(sumpRemmainingOnTime) + " Sec")
                         : sumpMode == 4
                             ? "OFF"
                             : "NA");

    // --- Measure pixel width properly ---
    int msgW = tickerSprite.textWidth(tickerMsg1) +
               tickerSprite.textWidth(tickerMsg2) +
               tickerSprite.textWidth(tickerMsg3);

    // --- IP section ---
    tickerSprite.setTextColor(TFT_GREEN, TFT_BLACK);
    tickerSprite.print(tickerMsg1);

    // --- Bore error ---
    tickerSprite.setTextColor((boreError >= 3) ? TFT_RED : TFT_BLUE, TFT_BLACK);
    tickerSprite.print(tickerMsg2);

    // --- Sump error ---
    tickerSprite.setTextColor((sumpError >= 3) ? TFT_RED : TFT_MAGENTA, TFT_BLACK);
    tickerSprite.print(tickerMsg3);

    // push sprite at bottom of screen
    tickerSprite.pushSprite(0, tft.height() - tickerHeight);
    // move left
    tickerX -= 4; // scroll speed

    if (tickerX < -msgW)
    {
        tickerX = tft.width(); // restart from right
    }
    lastScroll=millis();
}

// ==== Draw Labels for Current Page ====
void drawMenuLabels()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);

    // Title
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 5);
    tft.print("Bore Settings Pg ");
    tft.print(currentPage + 1);

    // Labels
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    for (int i = 0; i < ROWS_PER_PAGE; i++)
    {
        int item = currentPage * ROWS_PER_PAGE + i;
        if (item >= MENU_COUNT)
            break;
        int y = 30 + i * ROW_HEIGHT;
        tft.setCursor(10, y);
        tft.print(labels[item]);
    }
    currentPage++;
}
// ==== Draw/Update a Value ====
void updateValue(int i, bool highlight)
{
    int relRow = i % ROWS_PER_PAGE;
    int y = 30 + relRow * ROW_HEIGHT;
    int valX = 230; // shifted a bit right
    String val;

    switch (i)
    {
    case 0:
        val = boreSettings.detectVoltage ? "ON" : "OFF";
        break;
    case 1:
        val = String(boreSettings.overVoltage, 1) + " V";
        break;
    case 2:
        val = String(boreSettings.underVoltage, 1) + " V";
        break;
    case 3:
        val = boreSettings.detectCurrent ? "ON" : "OFF";
        break;
    case 4:
        val = String(boreSettings.overCurrent, 1) + " A";
        break;
    case 5:
        val = String(boreSettings.underCurrent, 1) + " A";
        break;
    case 6:
        val = boreSettings.dryRun ? "ON" : "OFF";
        break;
    case 7:
        val = String(boreSettings.minPF, 2);
        break;
    case 8:
        val = boreSettings.cyclicTimer ? "ON" : "OFF";
        break;
    case 9:
        val = String(boreSettings.onTime) + " Min";
        break;
    case 10:
        val = String(boreSettings.offTime) + " Min";
        break;
    case 11:
        val = String(boreSettings.PowerOnDelay) + " Sec";
        break;
    }

    // Clear old area
    tft.fillRect(valX - 2, y - 2, 150, 28, TFT_BLACK);

    // Draw new value
    if (highlight)
    {
        tft.fillRect(valX - 2, y - 2, 150, 28, TFT_BLUE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
    }
    else
    {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
    }
    tft.setCursor(valX, y);
    tft.print(val);
}

// ==== Redraw values for current page ====
void drawAllValues()
{
    for (int i = currentPage * ROWS_PER_PAGE; i < (currentPage + 1) * ROWS_PER_PAGE; i++)
    {
        if (i >= MENU_COUNT)
            break;
        updateValue(i, (menuIndex == i) && editing);
    }
}

// void setup()
// {
//     tft.init();
//     tft.setRotation(1); // Landscape
//     drawMenuLabels();
//     drawAllValues();
// }

// void loop()
// {
//     // DEMO: cycle through items with simulated edits
//     delay(2000);
//     // Simulate value change depending on menuIndex
//     switch (menuIndex)
//     {
//     case 0:
//         boreSettings.detectVoltage = !boreSettings.detectVoltage;
//         break;
//     case 1:
//         boreSettings.overVoltage += 5;
//         break;
//     case 2:
//         boreSettings.underVoltage -= 5;
//         break;
//     case 3:
//         boreSettings.detectCurrent = !boreSettings.detectCurrent;
//         break;
//     case 4:
//         boreSettings.overCurrent += 0.5;
//         break;
//     case 5:
//         boreSettings.underCurrent -= 0.5;
//         break;
//     case 6:
//         boreSettings.dryRun = !boreSettings.dryRun;
//         break;
//     case 7:
//         boreSettings.minPF += 0.1;
//         break;
//     case 8:
//         boreSettings.cyclicTimer = !boreSettings.cyclicTimer;
//         break;
//     case 9:
//         boreSettings.onTime += 1;
//         break;
//     case 10:
//         boreSettings.offTime += 1;
//         break;
//     case 11:
//         boreSettings.PowerOnDelay += 5;
//         break;
//     }
//     // Enter edit mode
//     editing = true;
//     updateValue(menuIndex, true);
//     delay(1000);

//     // Exit edit mode
//     editing = false;
//     updateValue(menuIndex, false);

//     // Move to next item
//     menuIndex++;
//     if (menuIndex >= MENU_COUNT)
//         menuIndex = 0;

//     // If we crossed page boundary → redraw new page
//     int newPage = menuIndex / ROWS_PER_PAGE;
//     if (newPage != currentPage)
//     {
//         currentPage = newPage;
//         drawMenuLabels();
//         drawAllValues();
//     }
// }

// void drawStatusLine(int row, const char *label, const String &value)
// {
//     int y = 30 + row * ROW_HEIGHT;
//     tft.setCursor(10, y);
//     tft.setTextColor(TFT_WHITE, TFT_BLACK);
//     tft.print(label);

//     int valX = 235; // align values
//     tft.setCursor(valX, y);
//     tft.setTextColor(TFT_CYAN, TFT_BLACK);
//     tft.print(value);
// }
void drawStatusLine(int row, const char *label, const String &value, uint16_t fg, uint16_t bg, bool highlight)
{
    int y = 40 + row * ROW_HEIGHT;
    int valX = 230;
    tft.setCursor(10, y);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(label);

    // If highlight needed, draw a "band" behind value
    if (highlight)
    {
        int textHeight = 16;             // for text size 2 (approx 8px * 2)
        int bandHeight = textHeight + 6; // add padding
        int bandY = y - 2;               // adjust to center nicely
        tft.fillRect(valX - 4, bandY, 80, bandHeight, bg);
    }

    // Print value over background
    tft.setCursor(valX, y);
    tft.setTextColor(fg, highlight ? bg : TFT_BLACK);
    tft.print(value);
}

void drawStatusScreen(bool isBore)
{
    // Serial.println("ENtering drawStatusScreen()");
    Settings &s = isBore ? boreSettings : sumpSettings;
    String prefix = isBore ? labels[0] : labels[1];
    // tickerMsg = "IP: " + WiFi.localIP().toString() + " | Bore Error:" + boreErrorMessage + " | Sump error: " + sumpErrorMessage;

    String lines[24];
    int count = 0;
    lines[count++] = prefix + " " + String(labels[18]) + ": " + String(voltage, 0) + " V";                                                                      // item 0
    lines[count++] = prefix + " " + String(labels[19]) + ": " + String(isBore ? (boreMotorRunning ? current : 0) : (sumpMotorRunning ? current : 0), 1) + " A"; // item 1
    lines[count++] = prefix + " " + String(labels[21]) + ": " + String(isBore ? (boreMotorRunning ? pf : 0) : (sumpMotorRunning ? pf : 0));                     // item 2

    // Tank + Motor item 3
    lines[count++] = prefix + " " + String(labels[4]) + " " + String(labels[5]) + ": " + (isBore ? digitalRead(FLOAT_BORE_OHT_PIN) ? "FULL" : "LOW" : digitalRead(FLOAT_SUMP_OHT_PIN) ? "FULL"
                                                                                                                                                                                      : "LOW"); // item 3
    if (!isBore)
    {                                                                                                                                              // item 4
        lines[count++] = prefix + " " + String(labels[3]) + " " + String(labels[5]) + ": " + (digitalRead(FLOAT_SUMP_UGT_PIN) ? "FULL" : "EMPTY"); // or LOW
    }
    // item 4 or 5
    lines[count++] = prefix + " " + String(labels[2]) + ": " + String(isBore ? (boreMode == 3 ? "ON" : boreMode == 4 ? "OFF"
                                                                                                   : boreMode == 1   ? "Waiting"
                                                                                                   : boreMode == 2   ? "Critic"
                                                                                                                     : 0)
                                                                             : (sumpMode == 3 ? "ON" : sumpMode == 4 ? "OFF"
                                                                                                   : sumpMode == 1   ? "Waiting"
                                                                                                   : sumpMode == 2   ? "Critic"
                                                                                                                     : 0)); // or OFF
    // Voltage detect item
    lines[count++] = prefix + " " + String(labels[6]) + ": " + (s.detectVoltage ? "ON" : "OFF");
    if (s.detectVoltage)
    {
        lines[count++] = prefix + " " + String(labels[7]) + ": " + String(s.overVoltage, 0) + " V";
        lines[count++] = prefix + " " + String(labels[8]) + ": " + String(s.underVoltage, 0) + " V";
    }

    // Current detect
    lines[count++] = prefix + " " + String(labels[9]) + ": " + (s.detectCurrent ? "ON" : "OFF");
    if (s.detectCurrent)
    {
        lines[count++] = prefix + " " + String(labels[10]) + ": " + String(s.overCurrent, 1) + " A";
        lines[count++] = prefix + " " + String(labels[11]) + ": " + String(s.underCurrent, 1) + " A";
    }

    // Dry run detect
    lines[count++] = prefix + " " + String(labels[12]) + ": " + (s.dryRun ? "ON" : "OFF");
    if (s.dryRun)
    {
        lines[count++] = prefix + " " + String(labels[13]) + ": " + String(s.minPF, 2);
    }

    // Timer
    lines[count++] = prefix + " " + String(labels[14]) + ": " + (s.cyclicTimer ? "ON" : "OFF");
    if (s.cyclicTimer)
    {
        lines[count++] = prefix + " " + String(labels[15]) + ": " + String(s.onTime) + " Min";
        lines[count++] = prefix + " " + String(labels[16]) + ": " + String(s.offTime) + " Min";
    }
    lines[count++] = prefix + " " + String(labels[17]) + ": " + String(s.PowerOnDelay) + " Sec";

    // Draw current page, leaving bottom 20px free
    // tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, tft.width(), tft.height() - tickerHeight, TFT_BLACK);
    // tft.setTextSize(2);
    // tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    // tft.setCursor(10, 5);
    // tft.print(isBore ? labels[0] : labels[1]); // Bore or Sump
    // tft.print(" Status Page ");
    // tft.print(currentPage + 1);

    String header = String(isBore ? labels[0] : labels[1]) + " Status Page " + String(currentPage + 1);

    tft.setTextSize(2);              // use the same size you’ll actually draw with
    int w = tft.textWidth(header);   // pixel width of the string
    int x = (DISPLAY_WIDTH - w) / 2; // center horizontally
    int y = 5;                       // top margin

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(x, y);
    tft.print(header);

    // Draw rows but stop before ticker area
    for (int i = 0; i < ROWS_PER_PAGE; i++)
    {
        int item = currentPage * ROWS_PER_PAGE + i;
        if (item >= count)
            break;

        int sep = lines[item].indexOf(':');

        // if (sep > 0)
        // {
        //     String label = lines[item].substring(0, sep);
        //     String value = lines[item].substring(sep + 2);
        //     drawStatusLine(i, label.c_str(), value);
        // }
        if (sep > 0)
        {
            String label = lines[item].substring(0, sep);
            String value = lines[item].substring(sep + 2);

            // --- Default colors ---
            uint16_t fg = TFT_CYAN;
            uint16_t bg = TFT_BLACK;
            bool highlight = false;
            // Voltage check (your line[0] -> voltage)
            // if (item == 0)
            // {
            //     float v = value.toFloat();
            //     if (v > s.overVoltage || v < s.underVoltage)
            //     {
            //         highlight = true;
            //     }
            // }

            // // Current check (your line[1] -> current)
            // if (item == 1)
            // {
            //     float c = value.toFloat();
            //     if (c > s.overCurrent || c < s.underCurrent)
            //     {
            //         highlight = true;
            //     }
            // }

            // if (item == 2)
            // { // PF line
            //     float p = value.toFloat();
            //     if (p < s.minPF)
            //     {
            //         highlight = true;
            //     }
            // }
            // if (item == 3 && value == "LOW")
            // {
            //         highlight = true;
            // }
            // if (item == 4 && value == "EMPTY")
            // {
            //         highlight = true;
            // }
            // if ((item == 4 && isBore) || (item == 5 && !isBore))
            // {
            //     if (value == "Critical" || value == "Waiting")
            //     {
            //         highlight = true;
            //     }
            // }

            // Voltage check
            if (label.indexOf("Voltage") >= 0 && label.indexOf("Detect") < 0)
            {
                float v = value.toFloat();
                if (v > s.overVoltage || v < s.underVoltage)
                {

                    highlight = true;
                    bg = TFT_RED;
                }
            }

            // Current check
            else if (label.indexOf("Current") >= 0 && label.indexOf("Detect") < 0)
            {
                float c = value.toFloat();
                if (c > s.overCurrent || c < s.underCurrent)
                {

                    highlight = true;
                    bg = TFT_RED;
                }
            }

            // PF check
            else if (label.indexOf("PF") >= 0)
            {
                float p = value.toFloat();
                if (p < s.minPF)
                {
                    highlight = true;
                    bg = TFT_RED;
                }
            }
            // Tank status check
            else if (label.indexOf("Tank") >= 0)
            {
                if (value == "FULL")
                {
                    fg = TFT_BLACK; // Green text
                    bg = TFT_GREEN; // No background highlight
                }
                else if (value == "EMPTY" || value == "LOW")
                {
                    // fg = TFT_CYAN; // Normal cyan text
                    // optionally:
                    // fg = TFT_CYAN;
                    bg = TFT_RED; // uncomment if you want LOW to show warning
                }
                highlight = true;
            }
            // Motor status check
            else if (label.indexOf("Motor") >= 0)
            {
                highlight = true;
                if (value == "Critic")
                {
                    bg = TFT_RED;
                }
                else if (value == "Waiting")
                {
                    bg = TFT_YELLOW;
                    fg = TFT_BLACK;
                }
                else if (value == "ON")
                {
                    bg = TFT_GREEN;
                    fg = TFT_BLACK;
                }
            }

            drawStatusLine(i, label.c_str(), value, fg, bg, highlight);
        }
    }

    // Advance to next page
    currentPage++;
    if (currentPage * ROWS_PER_PAGE >= count)
    {
        currentPage = 0;
        if (isBore)
        {
            showingBore = 0;
        }
        else
        {
            showingBore = 1;
        }
    }
}
