#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// ==== TFT Pins ====
#define TFT_DC 15
#define TFT_CS 5
#define TFT_RST -1
#define TFT_MOSI 23
#define TFT_CLK 18
#define TFT_MISO 19

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// ==== Settings ====
struct Settings
{
    bool detectVoltage = false;
    float overVoltage = 280;
    float underVoltage = 180;
    bool detectCurrent = false;
    float overCurrent = 18.6;
    float underCurrent = 11.2;
    bool dryRun = true;
    float minPF = 0.8;
    bool cyclicTimer = true;
    int onTime = 15;
    int offTime = 30;
    int PowerOnDelay = 60;
};
Settings boreSettings;

int menuIndex = 0;    // Current selected item
bool editing = false; // Edit mode flag
int currentPage = 0;  // Page number

// ==== Labels ====
const char *labels[] = {
    "VOLT Detect", "OVER Voltage", "UNDER Voltage", "AMP Detect",
    "OVER Current", "UNDER Current", "DRY Run Detection", "MIN PF",
    "CYCLIC Timer", "ON Time", "OFF Time", "POWER ON Delay"};
#define MENU_COUNT 12
#define ROWS_PER_PAGE 6
#define ROW_HEIGHT 36 // row spacing

// ==== Draw Labels for Current Page ====
void drawMenuLabels()
{
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextSize(2);

    // Title
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.setCursor(10, 5);
    tft.print("Bore Settings Pg ");
    tft.print(currentPage + 1);

    // Labels
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    for (int i = 0; i < ROWS_PER_PAGE; i++)
    {
        int item = currentPage * ROWS_PER_PAGE + i;
        if (item >= MENU_COUNT)
            break;
        int y = 30 + i * ROW_HEIGHT;
        tft.setCursor(10, y);
        tft.print(labels[item]);
    }
}

// ==== Draw/Update a Value ====
void updateValue(int i, bool highlight)
{
    int relRow = i % ROWS_PER_PAGE;
    int y = 30 + relRow * ROW_HEIGHT;
    int valX = 230;
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
    tft.fillRect(valX - 2, y - 2, 120, 28, ILI9341_BLACK);

    // Draw new value
    if (highlight)
    {
        tft.fillRect(valX - 2, y - 2, 120, 28, ILI9341_BLUE);
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    }
    else
    {
        tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
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
    tft.begin();
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
