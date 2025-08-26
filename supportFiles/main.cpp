#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <JC_Button.h>
// #include <LibPrintf.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WebServer.h>

WebServer server(80);

// --------------------- Pin Definitions (ESP32) -------------------------
// GPIO6 to GPIO11 → Used for flash memory (SPI), do not use.
// GPIO1 & GPIO3 → Used for Serial; only use if you're not using USB serial.
// GPIO12,    GPIO15 → Need careful use during boot(strapping pins).
// GPIO16 and GPIO17 Serial2
// GPIO21 GPIO22 SDA and SCL
// 5,18, 19, 23 SPI
// Pin 34 ~ 39 INPUT only  - no output or INPUT_PULLUP or INPUT_PULLDOWN
//
#define FLOAT_BOHT_PIN 13
#define FLOAT_SUGT_PIN 12
#define FLOAT_SOHT_PIN 18
#define MOTOR_STATUS_LED 14
#define SUMP_MOTOR_RELAY_PIN 19
#define BORE_MOTOR_RELAY_PIN 4

#define KEY_SET 26
#define KEY_UP 25
#define KEY_DOWN 27
#define SW_AUTO 32
#define SW_MANUAL 33

#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define I2C_SDA 21
#define I2C_SCL 22

// // --------------------- Globals -------------------------
// Button btnSET(KEY_SET);
// Button btnUP(KEY_UP);
// Button btnDN(KEY_DOWN);
// pin assignments
const byte DN_PIN(KEY_DOWN), // connect a button switch from this pin to ground
    UP_PIN(KEY_UP),          // ditto
    SET_PIN(KEY_SET);

Button btnSET(SET_PIN), btnUP(UP_PIN), btnDN(DN_PIN); // define the buttons

const unsigned long
    REPEAT_FIRST(500), // ms required before repeating on long press
    REPEAT_INCR(100);  // repeat interval for long press
const int
    MIN_COUNT(0),
    MAX_COUNT(59);

bool calibCancelled = 0;

LiquidCrystal_I2C lcd(0x3F, 16, 2); // Address may be 0x3F on some modules

// HardwareSerial PZEMSerial(2);
// PZEM004Tv30 pzem(PZEMSerial);
// PZEM004Tv30 pzem(&Serial2);
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN); // RX, TX

struct Settings
{
    float overVoltage = 250.0;
    float underVoltage = 180.0;
    float overCurrent = 6.5;
    float underCurrent = 0.3;
    float minPF = 0.3;
    unsigned int onTime = 5;
    unsigned int offTime = 15;
    bool dryRun = false;
    bool detectVoltage = false;
    bool detectCurrent = false;
    bool cyclicTimer = false;
} settings;

float voltage = 0, current = 0, power = 0, pf = 0, energy = 0;
char errorMessage[17] = "No ERROR";

bool inMenu = false;
bool motorRunning = false;
bool manulallyON = 0;
bool calibMode = 1;
int systemMode = 0;
int menuIndex = 0;
int error = 0; // 0:Noerror, 1:OHT LOW, 2:UHT LOW, 3:OV or UV, 4:OC or UC, 5:Dry run
unsigned long lastOnTime = 0;
unsigned long lastOffTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastPzemRead = 0;
unsigned long lastInteractionTime = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lasterrorTime = 0;
unsigned long buttonPressStart = 0;
unsigned long lastRepeatTime = 0;
const unsigned long pzemReadInterval = 1000;
const unsigned long repeatInterval = 200;
bool ledState = false;
const uint8_t totalMenuItems = 11;
static uint8_t screenIndex = 0;

// --------------------- Function Declarations -------------------------
void scrollMessage(const char *message, uint8_t row, uint16_t delayMs);
void loadSettings();
void saveSettings();
void showStatusScreen();
void showMenu();
void onSetClick();
void onUpClick();
void onDownClick();
int checkSystemStatus();
void blinkLED(int type);
void buttonCheck();
void calibrateMotor();
void readPzemValues();
void handleRootNEW();
void handleRoot();
void handleRoot1();
void handleOn();
void handleOff();
void handleSettings();
void handleRestart();
void setup();
void loop();

void scrollMessage(const char *message, uint8_t row, uint16_t delayMs = 300)
{
    static uint32_t lastUpdate = 0;
    static uint8_t index = 0;
    static const char *prevMessage = nullptr;

    // Reset scrolling if a new message is passed
    if (message != prevMessage)
    {
        index = 0;
        prevMessage = message;
    }

    uint8_t messageLen = strlen(message);

    if (millis() - lastUpdate >= delayMs)
    {
        lastUpdate = millis();

        char displayBuffer[17]; // 16 chars + null terminator

        if (messageLen <= 16)
        {
            // No need to scroll, just center it
            uint8_t pad = (16 - messageLen) / 2;
            memset(displayBuffer, ' ', sizeof(displayBuffer));
            memcpy(displayBuffer + pad, message, messageLen);
        }
        else
        {
            // Scroll if message is longer than 16 characters
            strncpy(displayBuffer, message + index, 16);
            displayBuffer[16] = '\0';
            index++;
            if (index > messageLen - 16)
            {
                index = 0;
            }
        }

        lcd.setCursor(0, row);
        lcd.print(displayBuffer);
    }
}

void loadSettings()
{
    EEPROM.begin(512); // Allocate 512 bytes
    EEPROM.get(0, settings);
    if (settings.overVoltage < 100 || settings.overVoltage > 300)
    {
        settings = Settings(); // load defaults if invalid
        EEPROM.put(0, settings);
        EEPROM.commit(); // <-- IMPORTANT on ESP32
    }
}

void saveSettings()
{
    EEPROM.put(0, settings);
    EEPROM.commit(); // <-- IMPORTANT on ESP32
}

void showStatusScreen()
{
    lcd.clear();
    static bool alternateScreen = false;
    static unsigned long lastToggleTime = 0;
    const unsigned long toggleInterval = 1000; // 1 second
    switch (screenIndex)
    {
    case 0:
        lcd.setCursor(0, 0);
        lcd.print("V:");
        lcd.print(voltage);
        lcd.print(" I:");
        lcd.print(current);

        lcd.setCursor(0, 1);
        lcd.print("IP:");
        lcd.print(WiFi.localIP());
        // lcd.print("PF:");
        // lcd.print(pf, 2); // PF with 2 decimal places
        // lcd.print(" M:");
        // lcd.print(motorRunning);
        break;

    case 1:
        lcd.setCursor(0, 0);
        lcd.print("PF:");
        lcd.print(pf, 2); // PF with 2 decimal places

        lcd.setCursor(0, 1);
        lcd.print("Power:");
        lcd.print(power);
        lcd.print(" W");
        // lcd.print("Energy: ");
        // lcd.print(energy);
        // lcd.print(" W");
        break;

    case 2:
        lcd.setCursor(0, 0);
        lcd.print("UGT:");
        lcd.print(digitalRead(FLOAT_SUGT_PIN) ? "OK" : "LOW");
        lcd.print(" OHT:");
        lcd.print(digitalRead(FLOAT_BOHT_PIN) ? "OK" : "LOW");

        lcd.setCursor(0, 1);
        lcd.print("Mode:");
        lcd.print(systemMode == 0 ? "AUTO" : systemMode == 1 ? "Manual"
                                                             : "Calib");
        break;

    case 3:
        lcd.setCursor(0, 0);
        Serial.print("System State: ");
        Serial.println(error);
        Serial.println();
        if (error >= 2) // UGT,OV,UC,OV,UV,DRY errors
        {
            lcd.print("ERROR:");
            lcd.setCursor(0, 1);
            if (error >= 3)
            {
                if (millis() - lastToggleTime >= toggleInterval)
                {
                    alternateScreen = !alternateScreen;
                    lastToggleTime = millis();

                    // lcd.setCursor(0, 1);
                    // lcd.print("                "); // Clear line

                    lcd.setCursor(0, 1);
                    if (alternateScreen)
                    {
                        lcd.print(errorMessage);
                    }
                    else
                    {
                        lcd.print("SET key resets  ");
                    }
                }
            }
            else
            {
                lcd.setCursor(0, 1);
                lcd.print(errorMessage);
            }
        }
        else
        {
            if (motorRunning && settings.onTime > 0)
            {
                unsigned long elapsed = ((millis() - lastOnTime) / 1000); // in sec
                unsigned long remaining = settings.onTime * 60 - elapsed; // in sec
                lcd.print("ON Time Left:");
                lcd.setCursor(0, 1);
                // remaining > 600 ? remaining : remaining<0 ?0:remaining*60;
                lcd.print(remaining > 600 ? remaining / 60 : remaining < 0 ? 0
                                                                           : remaining);

                lcd.print(remaining > 600 ? " min" : " sec");
            }
            else if (!motorRunning && settings.offTime > 0 && !digitalRead(FLOAT_BOHT_PIN))
            {
                unsigned long elapsed = ((millis() - lastOffTime) / 1000);
                unsigned long remaining = settings.offTime * 60 - elapsed;
                lcd.print("OFF Time Left:");
                lcd.setCursor(0, 1);
                lcd.print(remaining > 600 ? remaining / 60 : remaining < 0 ? 0
                                                                           : remaining);

                lcd.print(remaining > 600 ? " min" : " sec");
            }
            else
            {
                lcd.print("System Idle...");
                lcd.setCursor(0, 1);
                lcd.print("                ");
            }
        }        
        break;
    case 4:
        lcd.setCursor(0, 0);
        lcd.print("VOLTDETECT:");
        lcd.print(settings.detectVoltage ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("AMP DETECT:");
        lcd.print(settings.detectCurrent ? "ON " : "OFF");
        break;
    case 5:
        lcd.setCursor(0, 0);
        lcd.print("DRY DETECT:");
        lcd.print(settings.dryRun ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("Timer     :");
        lcd.print(settings.cyclicTimer ? "ON " : "OFF");
        break;
    }
}

void showMenu()
{
    printf("inMenu: %d", inMenu);
    printf(",  menuIndex: %d \n", menuIndex);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Menu mode:");
    lcd.setCursor(0, 1);
    char buffer[4];
    switch (menuIndex)
    {
    case 0:
        lcd.print("VOLT Detect: ");
        snprintf(buffer, sizeof(buffer), "%s", settings.detectVoltage == 1 ? "ON" : "OFF");
        lcd.print(buffer);
        break;
    case 1:
        lcd.print("Over Volt:");
        lcd.print(settings.overVoltage, 1);
        break;
    case 2:
        lcd.print("Under Volt:");
        lcd.print(settings.underVoltage, 1);
        break;
    case 3:
        lcd.print("AMP Detect: ");
        snprintf(buffer, sizeof(buffer), "%s", settings.detectCurrent == 1 ? "ON" : "OFF");
        lcd.print(buffer);
        break;
    case 4:
        lcd.print("Over Curr:");
        lcd.print(settings.overCurrent, 1);
        break;
    case 5:
        lcd.print("Under Curr:");
        lcd.print(settings.underCurrent, 1);
        break;
    case 6:
        lcd.print("Dry Detect: ");
        snprintf(buffer, sizeof(buffer), "%s", settings.dryRun == 1 ? "ON" : "OFF");
        lcd.print(buffer);
        break;
    case 7:
        lcd.print("Min PF:");
        lcd.print(settings.minPF, 2);
        break;
    case 8:
        lcd.print("Cylic Timer: ");
        snprintf(buffer, sizeof(buffer), "%s", settings.cyclicTimer == 1 ? "ON" : "OFF");
        lcd.print(buffer);
        break;
    case 9:
        lcd.print("ON Time:");
        lcd.print(settings.onTime);
        lcd.print(" min");
        break;
    case 10:
        lcd.print("OFF Time:");
        lcd.print(settings.offTime);
        lcd.print(" min");
        break;
    }
}

void onSetClick()
{
    Serial.print("Menu Index: ");
    Serial.print(menuIndex);
    Serial.print("  IN Menu: ");
    Serial.println(inMenu);
    if (!inMenu)
    {
        buttonPressStart = millis();
    }
    else
    {
        menuIndex++;
        if ((menuIndex == 1 || menuIndex == 2) && !settings.detectVoltage)
        {
            menuIndex = 3;
        }
        else if ((menuIndex == 4 || menuIndex == 5) && !settings.detectCurrent)
        {
            menuIndex = 6;
        }
        else if ((menuIndex == 7) && !settings.dryRun)
        {
            menuIndex = 8;
        }
        else if ((menuIndex == 9 || menuIndex == 10) && !settings.cyclicTimer)
        {
            menuIndex = 11;
        }

        if (menuIndex > totalMenuItems)
        {
            inMenu = false;
            menuIndex = 0;
            saveSettings();
            showStatusScreen();
        }
    }
    lastInteractionTime = millis();
}

void onUpClick()
{   // hide menus based on value
    // settings.detectVoltage(case 0==0)->hide case 1 and 2
    // settings.detectCurrent(case 3==0)->hide case 4 and 5
    // settings.dryRun(case 6==0)->hide case 7 and 8
    // settings.cyclicTimer(case 8)->hide case 9 and 10
    if (!inMenu)
        return;

    switch (menuIndex)
    {
    case 0:
        settings.detectVoltage = !settings.detectVoltage;
        break;
    case 1:
        settings.overVoltage += 1.0;
        break;
    case 2:
        settings.underVoltage += 1.0;
        break;
    case 3:
        settings.detectCurrent = !settings.detectCurrent;
        break;
    case 4:
        settings.overCurrent += 0.1;
        break;
    case 5:
        settings.underCurrent += 0.1;
        break;
    case 6:
        settings.dryRun = !settings.dryRun;
        break;
    case 7:
        settings.minPF += 0.01;
        break;
    case 8:
        settings.cyclicTimer = !settings.cyclicTimer;
        break;
    case 9:
        settings.onTime += 1;
        settings.onTime < 1 ? settings.onTime = 1 : settings.onTime;
        break;
    case 10:
        settings.offTime += 1;
        settings.offTime < 1 ? settings.offTime = 1 : settings.offTime;
        break;
    }

    lastInteractionTime = millis();
    showMenu();
}

void onDownClick()
{
    if (!inMenu)
        return;

    switch (menuIndex)
    {
    case 0:
        settings.detectVoltage = !settings.detectVoltage;
        break;
    case 1:
        settings.overVoltage -= 1.0;
        break;
    case 2:
        settings.underVoltage -= 1.0;
        break;
    case 3:
        settings.detectCurrent = !settings.detectCurrent;
        break;
    case 4:
        settings.overCurrent -= 0.1;
        break;
    case 5:
        settings.underCurrent -= 0.1;
        break;
    case 6:
        settings.dryRun = !settings.dryRun;
        break;
    case 7:
        settings.minPF -= 0.01;
        break;
    case 8:
        settings.cyclicTimer = !settings.cyclicTimer;
        break;
    case 9:
        settings.onTime -= 1;
        settings.onTime < 1 ? settings.onTime = 1 : settings.onTime;
        break;
    case 10:
        settings.offTime -= 1;
        settings.offTime < 1 ? settings.offTime = 1 : settings.offTime;
        break;
    }
    lastInteractionTime = millis();
    showMenu();
}

int checkSystemStatus()
{
    if ((voltage < settings.underVoltage || voltage > settings.overVoltage) && settings.detectVoltage)
    {
        if (voltage < settings.underVoltage)
        {
            strcpy(errorMessage, "LOW Voltage");
        }
        if (voltage > settings.overVoltage)
        {
            strcpy(errorMessage, "HIGH Voltage");
        }
        return 3; // Voltage out of range
    }
    if (motorRunning)
    {
        if ((current > settings.overCurrent) && settings.detectCurrent)
        {
            strcpy(errorMessage, "Over current");
            return 4; // Over current
        }

        if ((current < settings.underCurrent) && settings.detectCurrent)
        {
            strcpy(errorMessage, "Under current");
            return 5; // Under current
        }

        if (current < settings.underCurrent && pf < settings.minPF && settings.dryRun)
        {
            strcpy(errorMessage, "Dry run");
            return 6; // Dry run
        }
    }
    if (!digitalRead(FLOAT_SUGT_PIN))
    {

        strcpy(errorMessage, "UGT empty");
        return 2; // UGT empty
    }
    if (!digitalRead(FLOAT_BOHT_PIN))
    {
        strcpy(errorMessage, "OHT LOW");
        return 1; // OHT low
    }

    lasterrorTime = millis();
    return 0; // All OK
}
int countOfErrorLED;
// Type 1: on/off of 500ms 2:3on,1off, 3:ON, 4:OFF
void blinkLED(int type)
{
    if (type == 1)
    {
        if (millis() - lastBlinkTime >= 500)
        {                         // 500ms blink interval
            ledState = !ledState; // toggle state
            digitalWrite(MOTOR_STATUS_LED, ledState);
            lastBlinkTime = millis();
        }
    }
    else if (type == 2)
    {
        if (countOfErrorLED < 6)
        {
            if (millis() - lastBlinkTime >= 250)
            {
                printf("count of error led: %d \n", countOfErrorLED);
                ledState = !ledState; // toggle state
                digitalWrite(MOTOR_STATUS_LED, ledState);
                lastBlinkTime = millis();
                countOfErrorLED++;
            }
        }
        else
        {
            digitalWrite(MOTOR_STATUS_LED, LOW);
            if (millis() - lastBlinkTime > 2000)
            {
                lastBlinkTime = millis();
                countOfErrorLED = 0;
            }
        }
    }
    else if (type == 3)
    {
        digitalWrite(MOTOR_STATUS_LED, HIGH);
    }
    else if (type == 4)
    {
        digitalWrite(MOTOR_STATUS_LED, LOW);
    }
}

void buttonCheck()
{
    static int
        count,         // the number that is adjusted
        lastCount(-1); // previous value of count (initialized to ensure it's different when the sketch starts)
    static unsigned long
        rpt(REPEAT_FIRST); // a variable time that is used to drive the repeats for long presses
    enum states_t
    {
        WAIT,
        INCR,
        DECR,
        MENU
    }; // states for the state machine
    static states_t STATE; // current state machine state
    btnUP.read();          // read the buttons
    btnDN.read();
    btnSET.read();
    if (btnSET.wasPressed())
    {
        Serial.println("Set button pressed");
    }
    if (btnUP.wasPressed())
    {
        Serial.println("UP button pressed");
    }
    else if (btnDN.wasPressed())
    {
        Serial.println("DOWN button pressed");
    }

    if (count != lastCount) // print the count if it has changed
    {
        lastCount = count;
        Serial.println(count, DEC);
    }

    switch (STATE)
    {
    case WAIT: // wait for a button event
        if (btnSET.wasPressed())
            STATE = MENU;
        if (btnUP.wasPressed())
            STATE = INCR;
        else if (btnDN.wasPressed())
            STATE = DECR;
        else if (btnUP.wasReleased()) // reset the long press interval
            rpt = REPEAT_FIRST;
        else if (btnDN.wasReleased())
            rpt = REPEAT_FIRST;
        else if (btnUP.pressedFor(rpt)) // check for long press
        {
            rpt += REPEAT_INCR; // increment the long press interval
            STATE = INCR;
        }
        else if (btnDN.pressedFor(rpt))
        {
            rpt += REPEAT_INCR;
            STATE = DECR;
        }
        break;

    case INCR:
        ++count; // increment the counter
        onUpClick();
        count = min(count, MAX_COUNT); // but not more than the specified maximum
        STATE = WAIT;
        break;

    case DECR:
        --count; // decrement the counter
        onDownClick();
        count = max(count, MIN_COUNT); // but not less than the specified minimum
        STATE = WAIT;
        break;
    case MENU:

        // showMenu();

        if (inMenu)
        {
            menuIndex++;
            if ((menuIndex == 1 || menuIndex == 2) && !settings.detectVoltage)
            {
                menuIndex = 3;
            }
            else if ((menuIndex == 4 || menuIndex == 5) && !settings.detectCurrent)
            {
                menuIndex = 6;
            }
            else if ((menuIndex == 7) && !settings.dryRun)
            {
                menuIndex = 8;
            }
            else if ((menuIndex == 9 || menuIndex == 10) && !settings.cyclicTimer)
            {
                menuIndex = 11;
            }
            else if (menuIndex >= totalMenuItems)
            {
                inMenu = false;
                menuIndex = 0;
                saveSettings();
                showStatusScreen();
            }
            showMenu(); // call your menu function
        }
        else
        {
            if (error >= 3)
            {
                error = 0;
                showStatusScreen();
            }
            else
            {
                inMenu = true;
                menuIndex = 0;
                showMenu(); // call your menu function
            }
        }
        STATE = WAIT;
        break;
    }
}

void calibrateMotor()
{
    if (calibCancelled || error >=2)
    {
        return;
    }
    systemMode = 2;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Calibrating.....");
    lcd.setCursor(0, 1);
    lcd.print("Waiting for intialize.");
    while (digitalRead(KEY_SET) == HIGH)
    {
        if (digitalRead(KEY_UP) == LOW || digitalRead(KEY_DOWN) == LOW)
        {
            lcd.setCursor(0, 1);
            lcd.print("Change Sw 2 AUTO");
            calibCancelled = true;
            return;
        }
    }
    digitalWrite(BORE_MOTOR_RELAY_PIN, HIGH);
    blinkLED(3);

    motorRunning = true;
    lastOnTime = millis();
    int currentSec = (millis() - lastOnTime) / 1000;
    while (millis() - lastOnTime < 20000)
    {
        currentSec = (millis() - lastOnTime) / 1000;
        lcd.setCursor(0, 1);
        lcd.print("Wait for ");
        lcd.print(20 - currentSec);
        lcd.print(" sec");
    }

    const int samples = 5;
    const unsigned long sampleDelay = 500; // ms between samples

    float sumV = 0, sumI = 0, sumPF = 0;

    Serial.println("Starting auto-calibration...");
    lcd.setCursor(0, 0);
    lcd.print("Starting Auto   ");
    lcd.setCursor(0, 1);
    lcd.print("     Calibration");

    for (int i = 0; i < samples; i++)
    {
        float v = pzem.voltage();
        float i_ = current; // pzem.current();
        float pf = pf;      // pzem.pf();

        if (isnan(v) || isnan(i_) || isnan(pf))
        {
            Serial.println("Error: Invalid PZEM reading (NaN)");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Error:");
            lcd.setCursor(0, 1);
            lcd.print("PZEM Reading ERR");
            digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
            blinkLED(4);
            delay(500);
            return;
        }

        sumV += v;
        sumI += i_;
        sumPF += pf;
        delay(sampleDelay);
    }

    // Stop motor after calibration
    digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
    blinkLED(4);

    lastOffTime = millis();
    // Compute averages
    voltage = sumV / samples;
    current = sumI / samples;
    pf = sumPF / samples;

    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    pf = pzem.pf();
    energy = pzem.energy();

    // Calculate with ±20% margins and validate
    settings.minPF = max(0.1, pf - pf * 0.2);
    settings.overCurrent = current + current * 0.2;
    settings.underCurrent = max(0.1, current - current * 0.2);
    settings.overVoltage = voltage + voltage * 0.2;
    settings.underVoltage = max(50.0, voltage - voltage * 0.2);
    settings.offTime = 1;
    settings.onTime = 1;
    // Save to EEPROM
    saveSettings();

    // Feedbackprintf("Calibration completed successfully:\n");
    printf("Min PF: %.2f\n", settings.minPF);
    printf("Over Current: %.2f A\n", settings.overCurrent);
    printf("Under Current: %.2f A\n", settings.underCurrent);
    printf("Over Voltage: %.1f V\n", settings.overVoltage);
    printf("Under Voltage: %.1f V\n", settings.underVoltage);

    // Serial.println("Calibration completed successfully:");
    // Serial.print("Min PF: ");
    // Serial.println(settings.minPF);
    // Serial.print("Over Current: ");
    // Serial.println(settings.overCurrent);
    // Serial.print("Under Current: ");
    // Serial.println(settings.underCurrent);
    // Serial.print("Over Voltage: ");
    // Serial.println(settings.overVoltage);
    // Serial.print("Under Voltage: ");
    // Serial.println(settings.underVoltage);

    while (digitalRead(SW_AUTO))
    {
        lcd.setCursor(0, 0);
        lcd.print("Setting Saved   ");
        lcd.setCursor(0, 1);
        lcd.print("Change Sw 2 AUTO");
        digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
        motorRunning = false;
        calibMode = 0;
    }
}

void readPzemValues()
{
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    pf = pzem.pf();
    energy = pzem.energy();
}

void handleRootNEW()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Water Level Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css" rel="stylesheet">
</head>
<body class="bg-light">

<div class="container mt-4">
  <h2 class="mb-4 text-center">Water Level Controller</h2>
)rawliteral";

    // Live Data Section
    html += "<div class='card mb-4'><div class='card-body'>";
    html += "<h5 class='card-title'>Live Data</h5>";
    html += "<div class='row'>";
    html += "<div class='col-md-4'><strong>Voltage:</strong> " + String(voltage, 1) + " V</div>";
    html += "<div class='col-md-4'><strong>Current:</strong> " + String(current, 2) + " A</div>";
    html += "<div class='col-md-4'><strong>Power:</strong> " + String(power, 1) + " W</div>";
    html += "</div><div class='row'>";
    html += "<div class='col-md-4'><strong>PF:</strong> " + String(pf, 2) + "</div>";
    html += "<div class='col-md-4'><strong>UGT:</strong> " + String(digitalRead(FLOAT_SUGT_PIN) ? "OK" : "LOW") + "</div>";
    html += "<div class='col-md-4'><strong>OHT:</strong> " + String(digitalRead(FLOAT_BOHT_PIN) ? "OK" : "LOW") + "</div>";
    html += "</div><div class='row'>";
    html += "<div class='col-md-4'><strong>Motor:</strong> " + String(motorRunning ? "ON" : "OFF") + "</div>";
    html += "<div class='col-md-8'><strong>Error:</strong> " + String(errorMessage) + "</div>";
    html += "</div></div></div>";

    // Settings Form
    html += R"rawliteral(
<form action='/settings' method='POST' class='card p-3 mb-4'>
  <h5 class='mb-3'>Settings</h5>
  <div class='row g-3'>

    <div class='col-md-4'>
      <label class='form-label'>Over Voltage</label>
      <input class='form-control' type='number' name='ov' step='0.1' value=')rawliteral" +
            String(settings.overVoltage) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>Under Voltage</label>
      <input class='form-control' type='number' name='uv' step='0.1' value=')rawliteral" +
            String(settings.underVoltage) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>Over Current</label>
      <input class='form-control' type='number' name='oc' step='0.1' value=')rawliteral" +
            String(settings.overCurrent) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>Under Current</label>
      <input class='form-control' type='number' name='uc' step='0.1' value=')rawliteral" +
            String(settings.underCurrent) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>Min PF</label>
      <input class='form-control' type='number' name='pf' step='0.01' value=')rawliteral" +
            String(settings.minPF) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>On Time (min)</label>
      <input class='form-control' type='number' name='ot' value=')rawliteral" +
            String(settings.onTime) + R"rawliteral('>
    </div>

    <div class='col-md-4'>
      <label class='form-label'>Off Time (min)</label>
      <input class='form-control' type='number' name='ft' value=')rawliteral" +
            String(settings.offTime) + R"rawliteral('>
    </div>

  </div>

  <hr>

  <div class='form-check'>
    <input class='form-check-input' type='checkbox' name='dryRun' )rawliteral" +
            (settings.dryRun ? "checked" : "") + R"rawliteral(>
    <label class='form-check-label'>Enable Dry Run</label>
  </div>

  <div class='form-check'>
    <input class='form-check-input' type='checkbox' name='voltage' )rawliteral" +
            (settings.detectVoltage ? "checked" : "") + R"rawliteral(>
    <label class='form-check-label'>Detect Voltage</label>
  </div>

  <div class='form-check'>
    <input class='form-check-input' type='checkbox' name='current' )rawliteral" +
            (settings.detectCurrent ? "checked" : "") + R"rawliteral(>
    <label class='form-check-label'>Detect Current</label>
  </div>

  <div class='form-check mb-3'>
    <input class='form-check-input' type='checkbox' name='cyclic' )rawliteral" +
            (settings.cyclicTimer ? "checked" : "") + R"rawliteral(>
    <label class='form-check-label'>Cyclic Timer</label>
  </div>

  <button class='btn btn-primary' type='submit'>Save Settings</button>
</form>
)rawliteral";

    // Action buttons
    html += R"rawliteral(
  <div class="d-grid gap-2">
    <a href="/on" class="btn btn-success">Start Motor</a>
    <a href="/off" class="btn btn-danger">Stop Motor</a>
    <a href="/restart" class="btn btn-secondary">Restart ESP32</a>
  </div>
</div>

</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

void handleRoot()
{
    String html = "<!DOCTYPE html><html><head><title>WLC</title>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;padding:10px;max-width:600px;margin:auto;}h1{font-size:1.5em;}label{display:block;margin-top:10px;}input[type=number]{width:100%;padding:4px;}input[type=submit]{margin-top:15px;padding:6px 12px;}hr{margin:20px 0;}a{margin-right:10px;text-decoration:none;color:blue;}</style>";
    html += "</head><body>";
    html += "<h1>Water Level Controller</h1>";

    html += "<h2>Live Data</h2>";
    html += "<table border='0' cellpadding='5'>";
    html += "<tr><td><strong>Voltage:</strong></td><td>" + String(voltage, 1) + " V</td></tr>";
    html += "<tr><td><strong>Current:</strong></td><td>" + String(current, 2) + " A</td></tr>";
    html += "<tr><td><strong>Power:</strong></td><td>" + String(power, 1) + " W</td></tr>";
    html += "<tr><td><strong>PF:</strong></td><td>" + String(pf, 2) + "</td></tr>";
    html += "<tr><td><strong>UGT:</strong></td><td>" + String(digitalRead(FLOAT_SUGT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>OHT:</strong></td><td>" + String(digitalRead(FLOAT_BOHT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Motor:</strong></td><td>" + String(motorRunning ? "ON" : "OFF") + "</td></tr>";
    html += "<tr><td><strong>Error:</strong></td><td>" + String(errorMessage) + "</td></tr>";
    html += "</table><hr>";

    // Settings Form
    html += "<hr><form action='/settings' method='POST'>";
    html += "<label>Over Voltage:<input type='number' step='0.1' name='ov' value='" + String(settings.overVoltage) + "'></label>";
    html += "<label>Under Voltage:<input type='number' step='0.1' name='uv' value='" + String(settings.underVoltage) + "'></label>";
    html += "<label>Over Current:<input type='number' step='0.1' name='oc' value='" + String(settings.overCurrent) + "'></label>";
    html += "<label>Under Current:<input type='number' step='0.1' name='uc' value='" + String(settings.underCurrent) + "'></label>";
    html += "<label>Min PF:<input type='number' step='0.01' name='pf' value='" + String(settings.minPF) + "'></label>";
    html += "<label>On Time (min):<input type='number' name='ot' value='" + String(settings.onTime) + "'></label>";
    html += "<label>Off Time (min):<input type='number' name='ft' value='" + String(settings.offTime) + "'></label>";

    html += "<label><input type='checkbox' name='dryRun' " + String(settings.dryRun ? "checked" : "") + "> Enable Dry Run</label>";
    html += "<label><input type='checkbox' name='voltage' " + String(settings.detectVoltage ? "checked" : "") + "> Detect Voltage</label>";
    html += "<label><input type='checkbox' name='current' " + String(settings.detectCurrent ? "checked" : "") + "> Detect Current</label>";
    html += "<label><input type='checkbox' name='cyclic' " + String(settings.cyclicTimer ? "checked" : "") + "> Cyclic Timer</label>";

    html += "<input type='submit' value='Save Settings'>";
    html += "</form>";

    // Action Buttons
    html += "<hr><a href='/on'>Motor ON</a> | <a href='/off'>Motor OFF</a> | <a href='/restart'>Restart ESP32</a>";

    html += "</body></html>";
    Serial.println("HAndle Root received"); // or OFF
    server.send(200, "text/html", html);
}

// Status page
void handleRoot1()
{
    String html = "<h1>Water Level Controller</h1>";
    html += "<p>Voltage: " + String(voltage, 2) + " V</p>";
    html += "<p>Current: " + String(current, 2) + " A</p>";
    html += "<p>Power: " + String(power, 2) + " W</p>";
    html += "<p>PF: " + String(pf, 2) + "</p>";
    html += "<p>UGT: " + String(digitalRead(FLOAT_SUGT_PIN) ? "OK" : "LOW") + "</p>";
    html += "<p>OHT: " + String(digitalRead(FLOAT_BOHT_PIN) ? "OK" : "LOW") + "</p>";
    html += "<p>Motor: " + String(motorRunning ? "ON" : "OFF") + "</p>";
    html += "<p>Error: " + String(errorMessage) + "</p>";

    html += "<hr><form action='/settings' method='POST'>";
    html += "Over Voltage: <input type='number' step='0.1' name='ov' value='" + String(settings.overVoltage) + "'><br>";
    html += "Under Voltage: <input type='number' step='0.1' name='uv' value='" + String(settings.underVoltage) + "'><br>";
    html += "Over Current: <input type='number' step='0.1' name='oc' value='" + String(settings.overCurrent) + "'><br>";
    html += "Under Current: <input type='number' step='0.1' name='uc' value='" + String(settings.underCurrent) + "'><br>";
    html += "Min PF: <input type='number' step='0.01' name='pf' value='" + String(settings.minPF) + "'><br>";
    html += "On Time (min): <input type='number' name='ot' value='" + String(settings.onTime) + "'><br>";
    html += "Off Time (min): <input type='number' name='ft' value='" + String(settings.offTime) + "'><br>";

    html += "<label><input type='checkbox' name='dryRun' " + String(settings.dryRun ? "checked" : "") + "> Enable Dry Run</label><br>";
    html += "<label><input type='checkbox' name='voltage' " + String(settings.detectVoltage ? "checked" : "") + "> Detect Voltage</label><br>";
    html += "<label><input type='checkbox' name='current' " + String(settings.detectCurrent ? "checked" : "") + "> Detect Current</label><br>";
    html += "<label><input type='checkbox' name='cyclic' " + String(settings.cyclicTimer ? "checked" : "") + "> Cyclic Timer</label><br>";

    html += "<input type='submit' value='Save Settings'></form>";
    // html += "<hr><a href='/on'>Motor ON</a> | <a href='/off'>Motor OFF</a>";
    html += "<hr><a href='/on'>Motor ON</a> | <a href='/off'>Motor OFF</a> | <a href='/restart'>Restart ESP32</a>";

    // html += "<p><a href='/restart'>Restart ESP32</a></p>";
    server.send(200, "text/html", html);
}

void handleOn()
{
    if (!motorRunning)
    {
        digitalWrite(BORE_MOTOR_RELAY_PIN, HIGH);
        blinkLED(3);
        motorRunning = true;
        lastOnTime = millis();
        Serial.println("Motor turned ON"); // or OFF
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleOff()
{
    if (motorRunning)
    {
        digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
        blinkLED(4);
        motorRunning = false;
        lastOffTime = millis();
        Serial.println("Motor turned OFF"); // or OFF
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleSettings()
{
    if (server.method() == HTTP_POST)
    {
        settings.overVoltage = server.arg("ov").toFloat();
        settings.underVoltage = server.arg("uv").toFloat();
        settings.overCurrent = server.arg("oc").toFloat();
        settings.underCurrent = server.arg("uc").toFloat();
        settings.minPF = server.arg("pf").toFloat();
        settings.onTime = server.arg("ot").toInt();
        settings.offTime = server.arg("ft").toInt();
        settings.dryRun = server.hasArg("dryRun");
        settings.detectVoltage = server.hasArg("voltage");
        settings.detectCurrent = server.hasArg("current");
        settings.cyclicTimer = server.hasArg("cyclic");
        saveSettings();
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleRestart()
{
    server.sendHeader("Location", "/");
    server.send(200, "text/html", "<h1>Restarting ESP32...</h1>");
    server.send(303); // HTTP 303 See Other
    delay(500);       // allow the redirect to go through
    ESP.restart();
}

// --------------------- Setup -------------------------
void setup()
{
    Serial.begin(115200);
    // PZEMSerial.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    // Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    Serial2.begin(9600);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Water Ctrl Start");

    pinMode(BORE_MOTOR_RELAY_PIN, OUTPUT);
    pinMode(MOTOR_STATUS_LED, OUTPUT);
    pinMode(SUMP_MOTOR_RELAY_PIN, OUTPUT);
    digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
    digitalWrite(SUMP_MOTOR_RELAY_PIN, LOW);
    digitalWrite(MOTOR_STATUS_LED, LOW);

    pinMode(SW_AUTO, INPUT_PULLUP);
    pinMode(SW_MANUAL, INPUT_PULLUP);
    pinMode(FLOAT_BOHT_PIN, INPUT_PULLUP);
    pinMode(FLOAT_SUGT_PIN, INPUT_PULLUP);
    pinMode(FLOAT_SOHT_PIN, INPUT_PULLUP);
    // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    // res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap

    if (!res)
    {
        Serial.println("Failed to connect");
        ESP.restart();
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }

    Serial.println("Connected: " + WiFi.SSID());
    Serial.print("ESP32 MAC Address:");
    Serial.println(WiFi.macAddress());

    // Now WiFi is connected; start web server

    server.on("/", handleRoot);
    server.on("/on", handleOn);
    server.on("/off", handleOff);
    server.on("/settings", HTTP_POST, handleSettings);
    server.on("/restart", handleRestart);
    server.begin();
    Serial.println("Web server started at " + WiFi.localIP().toString());

    server.begin();

    btnSET.begin();
    btnUP.begin();
    btnDN.begin();

    loadSettings();
    printf("System Booted on ESP32\n");
}

// --------------------- Main Loop -------------------------
void loop()
{
    server.handleClient();
    buttonCheck();
    if ((millis() - lastOnTime > 5000) && error < 3)//elay the cheking to avoid in rush current
    {
        error = checkSystemStatus();
    }

    if (!inMenu && millis() - lastScreenSwitch >= 5000)
    {
        showStatusScreen();
        if (error >= 3)
        {
            screenIndex = 3;
            // reset error conditions if OV or UV
            if (error == 3)
            {
                error = checkSystemStatus();
            }
            else
            {//wait for 1Hr and check again if Dry, OC or UC
                if (millis() - lasterrorTime > 60 * 60 * 1000UL)
                {
                    error = checkSystemStatus();
                }
            }
        }
        else
        {
            screenIndex = (screenIndex + 1) % 6;
        }
        lastScreenSwitch = millis();
        printf("V:%.2f I:%.2f PF:%.2f P:%.2f UGT:%d OHT:%d Motor:%d ERROR:%d\n",
               voltage,
               current,
               pf,
               power,
               digitalRead(FLOAT_SUGT_PIN),
               digitalRead(FLOAT_BOHT_PIN),
               digitalRead(BORE_MOTOR_RELAY_PIN),
               error);
    }

    if (!inMenu && millis() - lastPzemRead >= pzemReadInterval)
    {
        readPzemValues();
        lastPzemRead = millis();
    }

    if (isnan(energy))
        energy = 0.0;
    if (isnan(voltage))
        voltage = 0.0;
    if (isnan(current))
        current = 0.0;
    if (isnan(power))
        power = 0.0;
    if (isnan(pf))
        pf = 0.0;

    if (digitalRead(SW_MANUAL) && digitalRead(SW_AUTO) && calibMode)
    {
        calibrateMotor();
    }
    else if (!digitalRead(SW_AUTO) && digitalRead(SW_MANUAL))
    {
        systemMode = 0;
        if (manulallyON)
        {
            digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
            motorRunning = false;
            lastOffTime = millis();
        }

        if (motorRunning)
        {
            if (settings.onTime < 1)
            {
                settings.onTime = 10;
            }           
            if ((settings.cyclicTimer ? millis() - lastOnTime >= settings.onTime * 60000UL : 0) || error >= 2)
            {
                Serial.println("More than 60sec");
                digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
                if (error >= 3)
                {
                    blinkLED(2);
                }
                else
                {
                    blinkLED(4);
                }
                motorRunning = false;
                lastOffTime = millis();
            }
        }
        else
        {
            if (settings.offTime < 1)
            {
                settings.offTime = 10;
            }
            if (error == 1)
            {
                blinkLED(1);
            }
            if (settings.cyclicTimer ? millis() - lastOffTime >= settings.offTime * 60000UL : 1 && error == 1)
            {
                digitalWrite(BORE_MOTOR_RELAY_PIN, HIGH);
                motorRunning = true;
                lastOnTime = millis();
                blinkLED(3);
            }
            else if (error >= 2)
            {
                blinkLED(2);
            }
        }
    }
    else if (!digitalRead(SW_MANUAL) && digitalRead(SW_AUTO))
    {
        systemMode = 1;
        if (error == 1)
        {
            digitalWrite(BORE_MOTOR_RELAY_PIN, HIGH);
            motorRunning = true;
            manulallyON = true;
            lastOnTime = millis();
        }
        else
        {
            digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
            motorRunning = false;
            lastOffTime = millis();
        }
    }
    else
    {
        lcd.setCursor(0, 0);
        lcd.print("System in Calib");

        lcd.setCursor(0, 1);
        lcd.print("Change Sw 2 AUTO");
    }
}
