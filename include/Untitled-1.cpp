#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library

#define TFT_GREY 0x5AEB

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

int menuIndex = 0;    // Current selected item
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
const char *labels[] = {"Bore ", "Sump ", "Motor ", "Underground ", "Overhead ", "Tank",
                        "VOLT Detect", "OVER Voltage", "UNDER Voltage", "AMP Detect",
                        "OVER Current", "UNDER Current", "DRY Run Detection", "MIN PF",
                        "CYCLIC Timer", "ON Time", "OFF Time", "POWER ON Delay",
                        "Voltage", "Current", "Power", "PF", "Energy", "Mode"};

#define MENU_COUNT 12
#define STATUS_COUNT 22
#define ROWS_PER_PAGE 6
#define ROW_HEIGHT 36 // row spacing

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
void drawStatusScreen(bool isBore)
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);

    // Title
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 5);
    tft.print("Water Level Controller");
    tft.print(currentPage + 1);
    // Labels
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (isBore)
    {
        for (int i = 0; i < ROWS_PER_PAGE; i++)
        {
            int item = currentPage * ROWS_PER_PAGE + i;
            int y = 30 + i * ROW_HEIGHT;
            tft.setCursor(10, y);
            tft.print(labels[0]);
            tft.print(labels[item]);
        }
    }

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

void setup()
{
    tft.init();
    tft.setRotation(1); // Landscape
    drawMenuLabels();
    drawAllValues();
}

void loop()
{
    // DEMO: cycle through items with simulated edits
    delay(2000);
    // Simulate value change depending on menuIndex
    switch (menuIndex)
    {
    case 0:
        boreSettings.detectVoltage = !boreSettings.detectVoltage;
        break;
    case 1:
        boreSettings.overVoltage += 5;
        break;
    case 2:
        boreSettings.underVoltage -= 5;
        break;
    case 3:
        boreSettings.detectCurrent = !boreSettings.detectCurrent;
        break;
    case 4:
        boreSettings.overCurrent += 0.5;
        break;
    case 5:
        boreSettings.underCurrent -= 0.5;
        break;
    case 6:
        boreSettings.dryRun = !boreSettings.dryRun;
        break;
    case 7:
        boreSettings.minPF += 0.1;
        break;
    case 8:
        boreSettings.cyclicTimer = !boreSettings.cyclicTimer;
        break;
    case 9:
        boreSettings.onTime += 1;
        break;
    case 10:
        boreSettings.offTime += 1;
        break;
    case 11:
        boreSettings.PowerOnDelay += 5;
        break;
    }
    // Enter edit mode
    editing = true;
    updateValue(menuIndex, true);
    delay(1000);

    // Exit edit mode
    editing = false;
    updateValue(menuIndex, false);

    // Move to next item
    menuIndex++;
    if (menuIndex >= MENU_COUNT)
        menuIndex = 0;

    // If we crossed page boundary → redraw new page
    int newPage = menuIndex / ROWS_PER_PAGE;
    if (newPage != currentPage)
    {
        currentPage = newPage;
        drawMenuLabels();
        drawAllValues();
    }
}

