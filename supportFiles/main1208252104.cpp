// Modified for dual motor control: BORE and SUMP
#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
// #include <LiquidCrystal_I2C.h>
#include <LiquidCrystal_AIP31068_I2C.h>
#include <PZEM004Tv30.h>
#include <OneButton.h>
#include <WiFiManager.h>
#include <WebServer.h>

uint8_t realStaMac[6] = {0xCC, 0xDB, 0xA7, 0x2F, 0xEF, 0x4C};

// mac address AP:02:0f:b5:2f:ef:4c STA:cc:db:a7:2f:ef:4c
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
/*he ESP32 chip includes specific strapping pins:

GPIO 0 (must be LOW to enter boot mode)
GPIO 2 (must be floating or LOW during boot)
GPIO 4
GPIO 5 (must be HIGH during boot)
GPIO 12 (must be LOW during boot)
GPIO 15 (must be HIGH during boot)*/
// #define FLOAT_BORE_UGT_PIN 12
#define FLOAT_BORE_OHT_PIN 18
#define FLOAT_SUMP_UGT_PIN 12
#define FLOAT_SUMP_OHT_PIN 13

#define BORE_MOTOR_RELAY_PIN 4
#define SUMP_MOTOR_RELAY_PIN 19
#define BORE_MOTOR_STATUS_LED 14
#define SUMP_MOTOR_STATUS_LED 2

#define KEY_SET 26
#define KEY_UP 25
#define KEY_DOWN 27
#define SW_AUTO 33
#define SW_MANUAL 32

#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define I2C_SDA 21
#define I2C_SCL 22

// const byte DN_PIN(KEY_DOWN), UP_PIN(KEY_UP), SET_PIN(KEY_SET);
// Button btnSET(SET_PIN), btnUP(UP_PIN), btnDN(DN_PIN);
// === Button Setup ===
OneButton btnSet(KEY_SET, true);
OneButton btnUp(KEY_UP, true);
OneButton btnDown(KEY_DOWN, true);

// LiquidCrystal_I2C lcd(0x3F, 16, 2);
LiquidCrystal_AIP31068_I2C lcd(0x3E, 16, 2); // set the LCD address to 0x3E for a 20 chars and 4 line display
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

struct Settings
{
    float overVoltage = 250.0;
    float underVoltage = 180.0;
    float overCurrent = 6.5;
    float underCurrent = 0.3;
    float minPF = 0.3;
    unsigned int PowerOnDelay = 5; // sec
    unsigned int onTime = 5;       // min
    unsigned int offTime = 15;     // min
    bool dryRun = false;
    bool detectVoltage = false;
    bool detectCurrent = false;
    bool cyclicTimer = false;
};

Settings boreSettings, sumpSettings;

float voltage = 0, current = 0, power = 0, pf = 0, energy = 0;

char boreErrorMessage[17] = "No ERROR";
char sumpErrorMessage[17] = "No ERROR";

bool inMenu = false;
bool boreMotorRunning = false;
bool sumpMotorRunning = false;
bool manuallyON = false;
bool calibMode = 1;
int systemMode = 0;
int menuIndex = 0;
int boreError = 0;
int sumpError = 0;

unsigned long boreLastOnTime = 0;
unsigned long boreLastOffTime = 0;
unsigned long sumpLastOnTime = 0;
unsigned long sumpLastOffTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastPzemRead = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBoreErrorTime = 0;
unsigned long lastSumpErrorTime = 0;

const unsigned long pzemReadInterval = 1000;
const uint8_t totalMenuItems = 23;
uint8_t screenIndex = 0;
bool ledState = false;
unsigned long lastRepeatTime = 0;
unsigned long buttonHoldStartTime = 0;
bool upHeld = false;
bool downHeld = false;
bool powerFailed = 1;

void loadSettings();
void saveSettings();
void printSettings(const char *label, const Settings &s);
void showMenu();
void showStatusScreen();
void onSetClick();
void updateMenuValue(bool increse);
void readPzemValues();
int checkSystemStatus(bool isBore);
void controlMotor(bool isBore, bool isAuto);
void blinkLED(int type, bool isBore);
void handleRootold();
void handleRoot();
void handleOn();
void handleOff();
void handleSettings();
void handleRestart();
void calibrateMotor(bool bore);
void handleHeldRepeat();
void setup();
void loop();

void loadSettings()
{
    EEPROM.begin(512);
    EEPROM.get(0, boreSettings);
    EEPROM.get(sizeof(Settings), sumpSettings);

    // Validate boreSettings
    if (boreSettings.overVoltage < 100 || boreSettings.overVoltage > 300)
    {
        boreSettings = Settings();
        EEPROM.put(0, boreSettings);
    }
    // Validate sumpSettings
    if (sumpSettings.overVoltage < 100 || sumpSettings.overVoltage > 300)
    {
        sumpSettings = Settings();
        EEPROM.put(sizeof(Settings), sumpSettings);
        EEPROM.commit();
    }
}

void saveSettings()
{
    EEPROM.put(0, boreSettings);
    EEPROM.put(sizeof(Settings), sumpSettings);
    EEPROM.commit();
}
void printSettings(const char *label, const Settings &s)
{
    Serial.printf("---- %s ----\n", label);
    Serial.printf("detectVoltage: %d\n", s.detectVoltage);
    Serial.printf("detectCurrent: %d\n", s.detectCurrent);
    Serial.printf("dryRun       : %d\n", s.dryRun);
    Serial.printf("cyclicTimer  : %d\n", s.cyclicTimer);
    Serial.printf("minPF        : %.2f\n", s.minPF);
    Serial.printf("onTime       : %u\n", s.onTime);
    Serial.printf("offTime      : %u\n", s.offTime);
    Serial.printf("overCurrent  : %.2f\n", s.overCurrent);
    Serial.printf("underCurrent : %.2f\n", s.underCurrent);
    Serial.printf("overVoltage  : %.2f\n", s.overVoltage);
    Serial.printf("underVoltage : %.2f\n", s.underVoltage);
    Serial.printf("PowerOnDelay : %u\n", s.PowerOnDelay);
    Serial.println("------------------------");
}

void showMenu()
{
    // lcd.clear();
    lcd.setCursor(0, 0);

    bool isBore = menuIndex <= 11;
    Settings &s = isBore ? boreSettings : sumpSettings;
    uint8_t index = isBore ? menuIndex : menuIndex - 12;
    if (menuIndex <= 11)
    {
        lcd.print("Menu mode: Bore ");
    }
    else
    {
        lcd.print("Menu mode: Sump ");
    }

    lcd.setCursor(0, 1);
    char buffer[4];

    switch (index)
    {
    case 0:
        lcd.print("VOLT Detect: ");
        snprintf(buffer, sizeof(buffer), "%s", s.detectVoltage ? "ON " : "OFF");
        lcd.print(buffer);
        break;
    case 1:
        lcd.print("Over Volt: ");
        lcd.print(s.overVoltage, 1);
        break;
    case 2:
        lcd.print("Under Volt:");
        lcd.print(s.underVoltage, 1);
        break;
    case 3:
        lcd.print("AMP Detect  :");
        snprintf(buffer, sizeof(buffer), "%s", s.detectCurrent ? "ON " : "OFF");
        lcd.print(buffer);
        break;
    case 4:
        lcd.print("Over Curr:  ");
        lcd.print(s.overCurrent, 1);
        break;
    case 5:
        lcd.print("Under Curr: ");
        lcd.print(s.underCurrent, 1);
        break;
    case 6:
        lcd.print("Dry Detect  :");
        snprintf(buffer, sizeof(buffer), "%s", s.dryRun ? "ON " : "OFF");
        lcd.print(buffer);
        break;
    case 7:
        lcd.print("Min PF   : ");
        lcd.print(s.minPF, 2);
        break;
    case 8:
        lcd.print("Cyclic Timer:");
        snprintf(buffer, sizeof(buffer), "%s", s.cyclicTimer ? "ON " : "OFF");
        lcd.print(buffer);
        break;
    case 9:
        lcd.print("ON Time: ");
        lcd.print(s.onTime);
        lcd.print("   min");
        break;
    case 10:
        lcd.print("OFF Time:");
        lcd.print(s.offTime);
        lcd.print("  min");
        break;
    case 11:
        lcd.print("P.ON Dlay:");
        lcd.print(s.PowerOnDelay);
        lcd.print(" sec");
        break;
    }
}

void showStatusScreen()
{
    lcd.clear();
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
        break;

    case 1:
        lcd.setCursor(0, 0);
        lcd.print("PF:");
        lcd.print(pf, 2);
        lcd.print(" P:");
        lcd.print((int)power);
        lcd.print("W");
        lcd.setCursor(0, 1);
        lcd.print("Mode:");
        lcd.print(systemMode == 0 ? "AUTO" : systemMode == 1 ? "Manual"
                                                             : "Calib");
        break;
    case 2:
        lcd.setCursor(0, 0);
        if (boreError >= 2 || sumpError >= 2)
        {
            lcd.print(boreError >= 2 ? "BORE " : "SUMP ");
            lcd.print("ERROR:");
            lcd.setCursor(0, 1);
            lcd.print(boreError >= 2 ? boreErrorMessage : sumpErrorMessage);
            if (boreError >= 4 || sumpError >= 4)
            {
                delay(3000);
                lcd.setCursor(0, 1);
                lcd.print("SET key resets");
            }
        }
        else
        {
            if (boreMotorRunning)
            {
                unsigned long elapsed = (millis() - boreLastOnTime) / 1000;
                unsigned long remaining = boreSettings.onTime * 60 - elapsed;
                Serial.printf("boreSettings.onTime: %d remaining %lu\n", boreSettings.onTime, remaining);
                lcd.print("B-ON  Left:");
                lcd.print(remaining > 60 ? remaining / 60 : max(0UL, remaining));
                lcd.print(remaining > 60 ? "min" : "sec");
            }
            else if (!boreMotorRunning && boreError == 1)
            {
                unsigned long elapsed = (millis() - boreLastOffTime) / 1000;
                unsigned long remaining = boreSettings.offTime * 60 - elapsed;
                lcd.print("B-OFF Left:");
                lcd.print(remaining > 60 ? remaining / 60 : max(0UL, remaining));
                lcd.print(remaining > 60 ? "min" : "sec");
            }
            else
            {
                lcd.print("Bore Idle...");
            }
            if (sumpMotorRunning)
            {
                unsigned long elapsed = (millis() - sumpLastOnTime) / 1000;
                unsigned long remaining = sumpSettings.onTime * 60 - elapsed;
                lcd.setCursor(0, 1);
                lcd.print("S-ON  Left:");
                lcd.print(remaining > 60 ? remaining / 60 : max(0UL, remaining));
                lcd.print(remaining > 60 ? "min" : "sec");
            }
            else if (!sumpMotorRunning && sumpError == 1)
            {
                unsigned long elapsed = (millis() - sumpLastOffTime) / 1000;
                unsigned long remaining = sumpSettings.offTime * 60 - elapsed;
                lcd.setCursor(0, 1);
                lcd.print("S-OFF Left:");
                lcd.print(remaining > 60 ? remaining / 60 : max(0UL, remaining));
                lcd.print(remaining > 60 ? "min" : "sec");
            }
            else
            {
                lcd.setCursor(0, 1);
                lcd.print("Sump Idle...");
            }
        }
        break;
    case 3:
        lcd.setCursor(0, 0);
        lcd.print("BORE:");
        lcd.setCursor(0, 1);
        lcd.print("OHT :");
        lcd.print(digitalRead(FLOAT_BORE_OHT_PIN) ? "OK" : "LOW");
        break;
    case 4:
        lcd.setCursor(0, 0);
        lcd.print("SUMP:");
        lcd.setCursor(0, 1);
        lcd.print("UGT:");
        lcd.print(digitalRead(FLOAT_SUMP_UGT_PIN) ? "OK" : "LOW");
        lcd.print(" OHT:");
        lcd.print(digitalRead(FLOAT_SUMP_OHT_PIN) ? "OK" : "LOW");
        break;
    case 5:
        lcd.setCursor(0, 0);
        lcd.print("B-VOLT DETET:");
        lcd.print(boreSettings.detectVoltage ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("B-AMP DETECT:");
        lcd.print(boreSettings.detectCurrent ? "ON " : "OFF");
        break;
    case 6:
        lcd.setCursor(0, 0);
        lcd.print("B-DRY RUN   :");
        lcd.print(boreSettings.dryRun ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("B-TIMER     :");
        lcd.print(boreSettings.cyclicTimer ? "ON " : "OFF");
        break;
    case 7:
        lcd.setCursor(0, 0);
        lcd.print("S-VOLT DETET:");
        lcd.print(sumpSettings.detectVoltage ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("S-AMP DETECT:");
        lcd.print(sumpSettings.detectCurrent ? "ON " : "OFF");
        break;

    case 8:
        lcd.setCursor(0, 0);
        lcd.print("S-DRY RUN   :");
        lcd.print(sumpSettings.dryRun ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("S-TIMER     :");
        lcd.print(sumpSettings.cyclicTimer ? "ON " : "OFF");
        break;
    case 9:
        lcd.setCursor(0, 0);
        lcd.print("Bore Motor:  ");
        lcd.print(boreMotorRunning ? "ON " : "OFF");
        lcd.setCursor(0, 1);
        lcd.print("Sump Motor:  ");
        lcd.print(sumpMotorRunning ? "ON " : "OFF");
        break;
    }
}

void onSetClick()
{
    Serial.println("SET button pressed");
    if (!inMenu)
    {
        if (sumpError >= 3 || boreError >= 3)
        {
            boreError = checkSystemStatus(true);
            sumpError = checkSystemStatus(false);
        }
        else
        {
            inMenu = true;
            menuIndex = 0;
            showMenu();
        }
    }
    else
    {
        menuIndex++;
        Settings &s = (menuIndex <= 11) ? boreSettings : sumpSettings;

        auto skipMenuIf = [&](int idx1, int idx2, bool condition, int nextIdx)
        {
            if ((menuIndex == idx1 || menuIndex == idx2) && !condition)
                menuIndex = nextIdx;
        };

        auto skipMenuIfSingle = [&](int idx, bool condition, int nextIdx)
        {
            if (menuIndex == idx && !condition)
                menuIndex = nextIdx;
        };

        // Apply all skipping rules
        skipMenuIf(1, 2, s.detectVoltage, 3);
        skipMenuIf(4, 5, s.detectCurrent, 6);
        skipMenuIfSingle(7, s.dryRun, 8);
        skipMenuIf(9, 10, s.cyclicTimer, 11);
        skipMenuIf(13, 14, s.detectVoltage, 15);
        skipMenuIf(16, 17, s.detectCurrent, 18);
        skipMenuIfSingle(19, s.dryRun, 20);
        skipMenuIf(21, 22, s.cyclicTimer, 23);

        if (menuIndex > totalMenuItems)
        {
            inMenu = false;
            menuIndex = 0;
            saveSettings();
            showStatusScreen();
        }
        showMenu(); // call your menu function
    }
}
bool switchOffBoreMotor = 0;
bool switchOffSumpMotor = 0;

void updateMenuValue(bool increse)
{
    if (!inMenu)
    {
        if (increse)
        {
            switchOffBoreMotor = !switchOffBoreMotor;
            controlMotor(1, 0);
            Serial.printf("switchOff Bore Motor: %d \n", switchOffBoreMotor);
        }
        else
        {
            switchOffSumpMotor = !switchOffSumpMotor;
            controlMotor(0, 0);
            Serial.printf("switchOff Sump Motor: %d \n", switchOffSumpMotor);
        }

        return;
    }

    bool isBore = menuIndex <= 11;
    Settings &s = isBore ? boreSettings : sumpSettings;
    uint8_t index = isBore ? menuIndex : menuIndex - 12;

    switch (index)
    {
    case 0:
        s.detectVoltage = !s.detectVoltage;
        break;
    case 1:
        increse ? s.overVoltage += 1.0 : s.overVoltage -= 1.0;
        break;
    case 2:
        increse ? s.underVoltage += 1.0 : s.underVoltage -= 1.0;
        break;
    case 3:
        s.detectCurrent = !s.detectCurrent;
        break;
    case 4:
        increse ? s.overCurrent += 0.1 : s.overCurrent -= 0.1;
        break;
    case 5:
        increse ? s.underCurrent += 0.1 : s.underCurrent -= 0.1;
        break;
    case 6:
        s.dryRun = !s.dryRun;
        break;
    case 7:
        increse ? s.minPF += 0.01 : s.minPF -= 0.01;
        break;
    case 8:
        s.cyclicTimer = !s.cyclicTimer;
        break;
    case 9:
        increse ? s.onTime += 1 : s.onTime -= 1;
        if (s.onTime < 1)
            s.onTime = 1;
        break;
    case 10:
        increse ? s.offTime += 1 : s.offTime -= 1;
        if (s.offTime < 1)
            s.offTime = 1;
        break;
    case 11:
        increse ? s.PowerOnDelay += 1 : s.PowerOnDelay -= 1;
        if (s.PowerOnDelay < 1)
            s.PowerOnDelay = 1;
        break;
    }
    showMenu();
}

void readPzemValues()
{
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    pf = pzem.pf();
    energy = pzem.energy();

    if (isnan(voltage))
        voltage = 0.0;
    if (isnan(current))
        current = 0.0;
    if (isnan(power))
        power = 0.0;
    if (isnan(pf))
        pf = 0.0;
    if (isnan(energy))
        energy = 0.0;
}
// 0=No Error, 1=OHT Low, 2=UGT low, 3=OV or UV, 4=OC 5=UC, 6=Dyrun
int checkSystemStatus(bool isBore)
{
    const char *errorMsg = nullptr;
    Settings &s = isBore ? boreSettings : sumpSettings;
    bool motor = isBore ? boreMotorRunning : sumpMotorRunning;
    int ohtPin = isBore ? FLOAT_BORE_OHT_PIN : FLOAT_SUMP_OHT_PIN;
    unsigned long &lastErrTime = isBore ? lastBoreErrorTime : lastSumpErrorTime;
    char *targetMsg = isBore ? boreErrorMessage : sumpErrorMessage;

    int errorCode = 0;

    if ((voltage < s.underVoltage || voltage > s.overVoltage) && s.detectVoltage)
    {
        errorMsg = (voltage < s.underVoltage) ? "LOW Voltage" : "HIGH Voltage";
        errorCode = 3;
    }
    else if (motor && current > s.overCurrent && s.detectCurrent)
    {
        errorMsg = "Over current";
        errorCode = 4;
    }
    else if (motor && current < s.underCurrent && s.detectCurrent)
    {
        errorMsg = "Under current";
        errorCode = 5;
    }
    else if (motor && current < s.underCurrent && pf < s.minPF && s.dryRun)
    {
        errorMsg = "Dry run";
        errorCode = 6;
    }
    else if (!isBore)
    {
        if (!digitalRead(FLOAT_SUMP_UGT_PIN))
        {
            errorMsg = "UGT empty";
            errorCode = 2;
        }
        else if (!digitalRead(ohtPin))
        {
            errorMsg = "OHT LOW";
            errorCode = 1;
        }
    }
    else if (!digitalRead(ohtPin))
    {
        errorMsg = "OHT LOW";
        errorCode = 1;
    }

    lastErrTime = millis();
    if (errorMsg)
    {
        strcpy(targetMsg, errorMsg);
        return errorCode;
    }

    // No error
    strcpy(targetMsg, "OK");
    return 0;
}

void controlMotor(bool isBore, bool isAuto)
{
    Settings &s = isBore ? boreSettings : sumpSettings;
    bool &motor = isBore ? boreMotorRunning : sumpMotorRunning;
    unsigned long &lastOnTime = isBore ? boreLastOnTime : sumpLastOnTime;
    unsigned long &lastOffTime = isBore ? boreLastOffTime : sumpLastOffTime;
    int relayPin = isBore ? BORE_MOTOR_RELAY_PIN : SUMP_MOTOR_RELAY_PIN;
    int &error = isBore ? boreError : sumpError;
    bool &switchOffMotor = isBore ? switchOffBoreMotor : switchOffSumpMotor;

    error = checkSystemStatus(isBore);

    // ---- ON LOGIC ----
    if (!motor)
    { // Sump can only start if Bore is not running
        if ((!isBore && boreMotorRunning) || (isBore && sumpMotorRunning))
        {
            return;
        }
        // Bore gets priority → if Sump is running and Bore is ready, stop sump before starting bore
        bool readyToStart = (powerFailed || (s.cyclicTimer ? millis() - lastOffTime >= s.offTime * 60000UL : true)) && error == 1;
        Serial.printf("readyToStart: %d, powerFailed: %d, s.cyclicTimer: %d,  lastOffTime: %lu, s.offTime: %d, error: %d, isBore: %d \n", readyToStart, powerFailed, s.cyclicTimer, lastOffTime, s.offTime, error, isBore);
        // if (isBore && readyToStart && otherMotor)
        // {
        //     digitalWrite(otherRelayPin, LOW);
        //     otherMotor = false;
        //     otherLastOff = millis();
        // }

        if (isAuto)
        {
            if (s.offTime < 1)
                s.offTime = 1;

            if (readyToStart)
            {
                digitalWrite(relayPin, HIGH);
                motor = true;
                lastOnTime = millis();
                powerFailed = 0;
            }
        }
        else // Manual mode
        {
            if (error == 1)
            {
                digitalWrite(relayPin, HIGH);
                motor = true;
                lastOnTime = millis();
            }
        }
    }
    // Switch OFF MOTOR
    else
    {
        if (s.onTime < 1)
            s.onTime = 1;

        bool stopCondition = (error == 0 || error >= 2 || switchOffMotor);

        if (!isAuto && stopCondition)
        {
            digitalWrite(relayPin, LOW);
            motor = false;
            lastOffTime = millis();
        }
        else if (isAuto && (stopCondition || (s.cyclicTimer && millis() - lastOnTime >= s.onTime * 60000UL)))
        {
            Serial.printf("auto motor off: %d\n", isBore);
            digitalWrite(relayPin, LOW);
            motor = false;
            lastOffTime = millis();
        }
    }
}

int countOfErrorLED;
// Type 1: on/off of 500ms 2:3on,1off, 3:ON, 4:OFF
void blinkLED(int type, bool isBore)
{
    static bool ledStateBore = false;
    static bool ledStateSump = false;
    static unsigned long lastBlinkTime = 0;
    static int countOfErrorLED = 0;

    int ledPin = isBore ? BORE_MOTOR_STATUS_LED : SUMP_MOTOR_STATUS_LED;
    bool &ledState = isBore ? ledStateBore : ledStateSump;
    unsigned long now = millis();

    if (type == 1 || type == 2) // Blinking modes
    {
        unsigned long interval = (type == 1) ? 500 : 250; // blink speed
        if (type == 2 && countOfErrorLED >= 6)            // pause after 3 blinks
        {
            digitalWrite(ledPin, LOW);
            if (now - lastBlinkTime > 2000)
            {
                lastBlinkTime = now;
                countOfErrorLED = 0;
            }
            return;
        }

        if (now - lastBlinkTime >= interval)
        {
            ledState = !ledState;
            digitalWrite(ledPin, ledState);
            lastBlinkTime = now;
            if (type == 2)
                countOfErrorLED++;
        }
    }
    else if (type == 3) // ON
    {
        digitalWrite(ledPin, HIGH);
    }
    else if (type == 4) // OFF
    {
        digitalWrite(ledPin, LOW);
    }
}

void handleRootold()
{
    String html = "<!DOCTYPE html><html><head><title>WLC</title>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;padding:10px;max-width:600px;margin:auto;}h1{font-size:1.5em;}label{display:block;margin-top:10px;}input[type=number]{width:100%;padding:4px;}input[type=submit]{margin-top:15px;padding:6px 12px;}hr{margin:20px 0;}a{margin-right:10px;text-decoration:none;color:blue;}</style>";
    html += "</head><body>";
    html += "<h1>Water Level Controller</h1>";

    html += "<h2>Live Data</h2>";
    html += "<table border='0' cellpadding='5'>";
    html += "<tr><td><strong>Bore Voltage:</strong></td><td>" + String(voltage, 1) + " V</td></tr>";
    html += "<tr><td><strong>Bore Current:</strong></td><td>" + String(current, 2) + " A</td></tr>";
    html += "<tr><td><strong>Bore Power:</strong></td><td>" + String(power, 1) + " W</td></tr>";
    html += "<tr><td><strong>Bore PF:</strong></td><td>" + String(pf, 2) + "</td></tr>";
    html += "<tr><td><strong>Bore UGT:</strong></td><td>" + String(digitalRead(FLOAT_SUMP_UGT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Bore OHT:</strong></td><td>" + String(digitalRead(FLOAT_BORE_OHT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Bore Motor:</strong></td><td>" + String(boreMotorRunning ? "ON" : "OFF") + "</td></tr>";
    html += "<tr><td><strong>Bore Error:</strong></td><td>" + String(boreErrorMessage) + "</td></tr>";
    html += "</table><hr>";

    // Settings Form
    html += "<hr><form action='/boreSettings' method='POST'>";
    html += "<label>Over Voltage:<input type='number' step='0.1' name='bov' value='" + String(boreSettings.overVoltage) + "'></label>";
    html += "<label>Under Voltage:<input type='number' step='0.1' name='buv' value='" + String(boreSettings.underVoltage) + "'></label>";
    html += "<label>Over Current:<input type='number' step='0.1' name='boc' value='" + String(boreSettings.overCurrent) + "'></label>";
    html += "<label>Under Current:<input type='number' step='0.1' name='buc' value='" + String(boreSettings.underCurrent) + "'></label>";
    html += "<label>Min PF:<input type='number' step='0.01' name='bpf' value='" + String(boreSettings.minPF) + "'></label>";
    html += "<label>On Time (min):<input type='number' name='bot' value='" + String(boreSettings.onTime) + "'></label>";
    html += "<label>Off Time (min):<input type='number' name='bft' value='" + String(boreSettings.offTime) + "'></label>";

    html += "<label><input type='checkbox' name='boredryRun' " + String(boreSettings.dryRun ? "checked" : "") + "> Enable Dry Run</label>";
    html += "<label><input type='checkbox' name='borevoltage' " + String(boreSettings.detectVoltage ? "checked" : "") + "> Detect Voltage</label>";
    html += "<label><input type='checkbox' name='borecurrent' " + String(boreSettings.detectCurrent ? "checked" : "") + "> Detect Current</label>";
    html += "<label><input type='checkbox' name='borecyclic' " + String(boreSettings.cyclicTimer ? "checked" : "") + "> Cyclic Timer</label>";

    html += "<input type='submit' value='Save Settings'>";
    html += "</form>";

    // Action Buttons
    html += "<hr><a href='/on'>Bore Motor ON</a> | <a href='/off'>Bore  OFF</a> | <a href='/restart'>Restart ESP32</a>";

    html += "</body></html>";
    Serial.println("HAndle Root received"); // or OFF
    server.send(200, "text/html", html);
}
void handleRoot()
{
    String html = "<!DOCTYPE html><html><head><title>WLC</title>";
    html += "<meta http-equiv='refresh' content='30'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;padding:10px;max-width:800px;margin:auto;}h1{font-size:1.5em;}label{display:block;margin-top:10px;}input[type=number]{width:100%;padding:4px;}input[type=submit]{margin-top:15px;padding:6px 12px;}hr{margin:20px 0;}a{margin-right:10px;text-decoration:none;color:blue;}table{width:100%;}td{vertical-align:top;width:50%;}</style>";
    html += "</head><body>";
    html += "<h1>Water Level Controller</h1>";

    html += "<table><tr><td>"; // Left column (BORE)
    html += "<h2>Bore Live Data</h2>";
    html += "<table border='0' cellpadding='5'>";
    html += "<tr><td><strong>Bore Voltage:</strong></td><td>" + String(voltage, 1) + " V</td></tr>";
    html += "<tr><td><strong>Bore Current:</strong></td><td>" + String(current, 2) + " A</td></tr>";
    html += "<tr><td><strong>Bore Power:</strong></td><td>" + String(power, 1) + " W</td></tr>";
    html += "<tr><td><strong>Bore PF:</strong></td><td>" + String(pf, 2) + "</td></tr>";
    html += "<tr><td><strong>Bore UGT:</strong></td><td>" + String("N/A") + "</td></tr>";
    html += "<tr><td><strong>Bore OHT:</strong></td><td>" + String(digitalRead(FLOAT_BORE_OHT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Bore Motor:</strong></td><td>" + String(boreMotorRunning ? "ON" : "OFF") + "</td></tr>";
    html += "<tr><td><strong>Bore Error:</strong></td><td>" + String(boreErrorMessage) + "</td></tr>";
    html += "</table><hr>";

    html += "<form action='/settings' method='POST'>";
    html += "<label>Over Voltage:<input type='number' step='0.1' name='bov' value='" + String(boreSettings.overVoltage) + "'></label>";
    html += "<label>Under Voltage:<input type='number' step='0.1' name='buv' value='" + String(boreSettings.underVoltage) + "'></label>";
    html += "<label>Over Current:<input type='number' step='0.1' name='boc' value='" + String(boreSettings.overCurrent) + "'></label>";
    html += "<label>Under Current:<input type='number' step='0.1' name='buc' value='" + String(boreSettings.underCurrent) + "'></label>";
    html += "<label>Min PF:<input type='number' step='0.01' name='bpf' value='" + String(boreSettings.minPF) + "'></label>";
    html += "<label>On Time (min):<input type='number' name='bot' value='" + String(boreSettings.onTime) + "'></label>";
    html += "<label>Off Time (min):<input type='number' name='bft' value='" + String(boreSettings.offTime) + "'></label>";
    html += "<label>Power On Delay (sec):<input type='number' name='bod' value='" + String(boreSettings.PowerOnDelay) + "'></label>";

    html += "<label><input type='checkbox' name='boredryRun' " + String(boreSettings.dryRun ? "checked" : "") + "> Enable Dry Run</label>";
    html += "<label><input type='checkbox' name='borevoltage' " + String(boreSettings.detectVoltage ? "checked" : "") + "> Detect Voltage</label>";
    html += "<label><input type='checkbox' name='borecurrent' " + String(boreSettings.detectCurrent ? "checked" : "") + "> Detect Current</label>";
    html += "<label><input type='checkbox' name='borecyclic' " + String(boreSettings.cyclicTimer ? "checked" : "") + "> Cyclic Timer</label>";

    html += "</td><td>"; // Right column (SUMP)
    html += "<h2>Sump Live Data</h2>";
    html += "<table border='0' cellpadding='5'>";
    html += "<tr><td><strong>Sump Voltage:</strong></td><td>" + String(voltage, 1) + " V</td></tr>";
    html += "<tr><td><strong>Sump Current:</strong></td><td>" + String(current, 2) + " A</td></tr>";
    html += "<tr><td><strong>Sump Power:</strong></td><td>" + String(power, 1) + " W</td></tr>";
    html += "<tr><td><strong>Sump PF:</strong></td><td>" + String(pf, 2) + "</td></tr>";
    html += "<tr><td><strong>Sump UGT:</strong></td><td>" + String(digitalRead(FLOAT_SUMP_UGT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Sump OHT:</strong></td><td>" + String(digitalRead(FLOAT_SUMP_OHT_PIN) ? "OK" : "LOW") + "</td></tr>";
    html += "<tr><td><strong>Sump Motor:</strong></td><td>" + String(sumpMotorRunning ? "ON" : "OFF") + "</td></tr>";
    html += "<tr><td><strong>Sump Error:</strong></td><td>" + String(sumpErrorMessage) + "</td></tr>";
    html += "</table><hr>";

    html += "<label>Over Voltage:<input type='number' step='0.1' name='sov' value='" + String(sumpSettings.overVoltage) + "'></label>";
    html += "<label>Under Voltage:<input type='number' step='0.1' name='suv' value='" + String(sumpSettings.underVoltage) + "'></label>";
    html += "<label>Over Current:<input type='number' step='0.1' name='soc' value='" + String(sumpSettings.overCurrent) + "'></label>";
    html += "<label>Under Current:<input type='number' step='0.1' name='suc' value='" + String(sumpSettings.underCurrent) + "'></label>";
    html += "<label>Min PF:<input type='number' step='0.01' name='spf' value='" + String(sumpSettings.minPF) + "'></label>";
    html += "<label>On Time (min):<input type='number' name='sot' value='" + String(sumpSettings.onTime) + "'></label>";
    html += "<label>Off Time (min):<input type='number' name='sft' value='" + String(sumpSettings.offTime) + "'></label>";
    html += "<label>Power On Delay (sec):<input type='number' name='sod' value='" + String(sumpSettings.PowerOnDelay) + "'></label>";

    html += "<label><input type='checkbox' name='sumpdryRun' " + String(sumpSettings.dryRun ? "checked" : "") + "> Enable Dry Run</label>";
    html += "<label><input type='checkbox' name='sumpvoltage' " + String(sumpSettings.detectVoltage ? "checked" : "") + "> Detect Voltage</label>";
    html += "<label><input type='checkbox' name='sumpcurrent' " + String(sumpSettings.detectCurrent ? "checked" : "") + "> Detect Current</label>";
    html += "<label><input type='checkbox' name='sumpcyclic' " + String(sumpSettings.cyclicTimer ? "checked" : "") + "> Cyclic Timer</label>";

    html += "<input type='submit' value='Save Settings'>";
    html += "</form>";

    html += "</td></tr></table>";

    html += "<hr>";
    html += "<a href='/on?motor=bore'>Bore Motor ON</a> | ";
    html += "<a href='/off?motor=bore'>Bore Motor OFF</a> | ";
    html += "<a href='/on?motor=sump'>Sump Motor ON</a> | ";
    html += "<a href='/off?motor=sump'>Sump Motor OFF</a> | ";
    html += "<a href='/restart'>Restart ESP32</a>";

    html += "</body></html>";
    Serial.println("Handle Root received");
    server.send(200, "text/html", html);
}
void handleOn()
{
    String motor = server.hasArg("motor") ? server.arg("motor") : "";

    if (motor == "bore")
    {
        if (!boreMotorRunning)
        {
            digitalWrite(BORE_MOTOR_RELAY_PIN, HIGH);
            boreMotorRunning = true;
            boreLastOnTime = millis();
            Serial.println("Bore Motor turned ON");
        }
    }
    else if (motor == "sump")
    {
        if (!sumpMotorRunning)
        {
            digitalWrite(SUMP_MOTOR_RELAY_PIN, HIGH);
            sumpMotorRunning = true;
            sumpLastOnTime = millis();
            Serial.println("Sump Motor turned ON");
        }
    }
    else
    {
        server.send(400, "text/plain", "Missing or invalid motor parameter");
        return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
}
void handleOff()
{
    String motor = server.hasArg("motor") ? server.arg("motor") : "";

    if (motor == "bore")
    {
        digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
        boreMotorRunning = false;
        boreLastOffTime = millis();
        Serial.println("Bore Motor turned OFF");
    }
    else if (motor == "sump")
    {
        digitalWrite(SUMP_MOTOR_RELAY_PIN, LOW);
        sumpMotorRunning = false;
        sumpLastOffTime = millis();
        Serial.println("Sump Motor turned OFF");
    }
    else
    {
        server.send(400, "text/plain", "Missing or invalid motor parameter");
        return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
}
void handleSettings()
{
    if (server.method() == HTTP_POST)
    {
        boreSettings.overVoltage = server.arg("bov").toFloat();
        boreSettings.underVoltage = server.arg("buv").toFloat();
        boreSettings.overCurrent = server.arg("boc").toFloat();
        boreSettings.underCurrent = server.arg("buc").toFloat();
        boreSettings.minPF = server.arg("bpf").toFloat();
        boreSettings.onTime = server.arg("bot").toInt();
        boreSettings.offTime = server.arg("bft").toInt();
        boreSettings.PowerOnDelay = server.arg("bod").toInt();

        boreSettings.dryRun = server.hasArg("boredryRun");
        boreSettings.detectVoltage = server.hasArg("borevoltage");
        boreSettings.detectCurrent = server.hasArg("borecurrent");
        boreSettings.cyclicTimer = server.hasArg("borecyclic");

        sumpSettings.overVoltage = server.arg("sov").toFloat();
        sumpSettings.underVoltage = server.arg("suv").toFloat();
        sumpSettings.overCurrent = server.arg("soc").toFloat();
        sumpSettings.underCurrent = server.arg("suc").toFloat();
        sumpSettings.minPF = server.arg("spf").toFloat();
        sumpSettings.onTime = server.arg("sot").toInt();
        sumpSettings.offTime = server.arg("sft").toInt();
        sumpSettings.PowerOnDelay = server.arg("sod").toInt();

        sumpSettings.dryRun = server.hasArg("sumpdryRun");
        sumpSettings.detectVoltage = server.hasArg("sumpvoltage");
        sumpSettings.detectCurrent = server.hasArg("sumpcurrent");
        sumpSettings.cyclicTimer = server.hasArg("sumpcyclic");
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

bool calibCancelled = 0;
void calibrateMotor(bool bore)
{

    if (boreError >= 2 || sumpError >= 2)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Bore:");
        lcd.print(boreErrorMessage);
        lcd.setCursor(0, 1);
        lcd.print("SUMP:");
        lcd.print(sumpErrorMessage);
        return;
    }

    systemMode = 2;

    // Motor control pins and state references
    int relayPin = bore ? BORE_MOTOR_RELAY_PIN : SUMP_MOTOR_RELAY_PIN;
    bool &motorRunning = bore ? boreMotorRunning : sumpMotorRunning;
    unsigned long &lastOnTime = bore ? boreLastOnTime : sumpLastOnTime;
    unsigned long &lastOffTime = bore ? boreLastOffTime : sumpLastOffTime;
    Settings &settings = bore ? boreSettings : sumpSettings;

    digitalWrite(relayPin, HIGH);
    motorRunning = true;
    lastOnTime = millis();

    while (millis() - lastOnTime < 20000)
    {
        int currentSec = (millis() - lastOnTime) / 1000;
        lcd.setCursor(0, 1);
        lcd.print("Wait for ");
        lcd.print(20 - currentSec);
        lcd.print(" sec");
    }

    const int samples = 5;
    const unsigned long sampleDelay = 500;
    float sumV = 0, sumI = 0, sumPF = 0;

    Serial.println("Starting auto-calibration...");
    lcd.setCursor(0, 0);
    lcd.print(bore ? "Starting Bore" : "Starting Sump");
    lcd.setCursor(0, 1);
    lcd.print("     Calibration");

    for (int i = 0; i < samples; i++)
    {
        float v = pzem.voltage();
        float i_ = pzem.current();
        float pf = pzem.pf();

        if (isnan(v) || isnan(i_) || isnan(pf))
        {
            Serial.println("Error: Invalid PZEM reading (NaN)");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Error:");
            lcd.setCursor(0, 1);
            lcd.print("PZEM Reading ERR");
            digitalWrite(relayPin, LOW);
            blinkLED(2, 1);
            blinkLED(2, 0);
            delay(500);
            return;
        }

        sumV += v;
        sumI += i_;
        sumPF += pf;
        delay(sampleDelay);
    }

    digitalWrite(relayPin, LOW);
    blinkLED(4, 1);
    blinkLED(4, 0);

    lastOffTime = millis();
    motorRunning = false;

    voltage = sumV / samples;
    current = sumI / samples;
    pf = sumPF / samples;
    power = pzem.power();
    energy = pzem.energy();

    settings.minPF = max(0.1, pf - pf * 0.2);
    settings.overCurrent = current + current * 0.2;
    settings.underCurrent = max(0.1, current - current * 0.2);
    settings.overVoltage = voltage + voltage * 0.2;
    settings.underVoltage = max(50.0, voltage - voltage * 0.2);
    settings.offTime = 1;
    settings.onTime = 1;

    saveSettings();

    printf("Min PF: %.2f\n", settings.minPF);
    printf("Over Current: %.2f A\n", settings.overCurrent);
    printf("Under Current: %.2f A\n", settings.underCurrent);
    printf("Over Voltage: %.1f V\n", settings.overVoltage);
    printf("Under Voltage: %.1f V\n", settings.underVoltage);
    while (digitalRead(SW_AUTO))
    {
        lcd.setCursor(0, 0);
        lcd.print("Setting Saved   ");
        lcd.setCursor(0, 1);
        lcd.print("Change Sw 2 AUTO");
        delay(2000);
    }
}

void handleHeldRepeat()
{
    if (upHeld || downHeld)
    {
        unsigned long now = millis();
        unsigned long heldDuration = now - buttonHoldStartTime;
        unsigned long interval;

        if (heldDuration < 1000)
            interval = 300;
        else if (heldDuration < 2000)
            interval = 150;
        else
            interval = 75;

        if (now - lastRepeatTime >= interval)
        {
            updateMenuValue(upHeld); // true for UP, false for DOWN
            lastRepeatTime = now;
        }
    }
}

void setup()
{ // Force STA mode
    WiFi.mode(WIFI_STA);

    // Set the STA MAC to your fixed hardware MAC
    esp_wifi_set_mac(WIFI_IF_STA, realStaMac);

    delay(200); // Let MAC setting settle
    Serial.begin(115200);
    Serial2.begin(9600);

    lcd.init();
    // lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.setContrast(124);
    lcd.print("Water Ctrl Start");

    pinMode(BORE_MOTOR_RELAY_PIN, OUTPUT);
    pinMode(SUMP_MOTOR_RELAY_PIN, OUTPUT);
    pinMode(SUMP_MOTOR_STATUS_LED, OUTPUT);
    pinMode(BORE_MOTOR_STATUS_LED, OUTPUT);
    pinMode(SW_AUTO, INPUT_PULLUP);
    pinMode(SW_MANUAL, INPUT_PULLUP);
    pinMode(FLOAT_BORE_OHT_PIN, INPUT_PULLUP);
    pinMode(FLOAT_SUMP_OHT_PIN, INPUT_PULLUP);
    pinMode(FLOAT_SUMP_UGT_PIN, INPUT_PULLUP);

    digitalWrite(BORE_MOTOR_RELAY_PIN, LOW);
    digitalWrite(SUMP_MOTOR_RELAY_PIN, LOW);
    digitalWrite(SUMP_MOTOR_STATUS_LED, LOW); // LED OFF (safe at boot)
    digitalWrite(BORE_MOTOR_STATUS_LED, LOW);
    // pinMode(FLOAT_SUMP_UGT_PIN, INPUT_PULLUP);
    // pinMode(KEY_SET, INPUT_PULLUP);  // SET
    // pinMode(KEY_UP, INPUT_PULLUP);   // UP
    // pinMode(KEY_DOWN, INPUT_PULLUP); // DOWN
    // Button binding
    btnSet.attachClick(onSetClick);
    // // btnSet.attachLongPressStart(onSetLongPress);
    // btnUp.attachClick(onUpClick);
    // btnDown.attachClick(onDownClick);
    btnUp.attachClick([]()
                      {
                          updateMenuValue(true); // single +1
                      });

    btnUp.attachLongPressStart([]()
                               {
    buttonHoldStartTime = millis();
    lastRepeatTime = millis();
    upHeld = true; });

    btnUp.attachLongPressStop([]()
                              { upHeld = false; });

    btnDown.attachClick([]()
                        {
                            updateMenuValue(false); // single -1
                        });

    btnDown.attachLongPressStart([]()
                                 {
    buttonHoldStartTime = millis();
    lastRepeatTime = millis();
    downHeld = true; });

    btnDown.attachLongPressStop([]()
                                { downHeld = false; });

    // attachInterrupt(digitalPinToInterrupt(KEY_SET), isrSet, FALLING);
    // attachInterrupt(digitalPinToInterrupt(KEY_UP), isrUp, CHANGE);
    // attachInterrupt(digitalPinToInterrupt(KEY_DOWN), isrDown, CHANGE);
    WiFiManager wm;
    // wm.resetSettings(); // wipe settings

    if (!wm.autoConnect())
    {
        Serial.println("Failed to connect or hit timeout");
        ESP.restart();
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }
    // Kill AP mode if it was enabled temporarily
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA); // Ensure we stay only in STA
    server.on("/", handleRoot);
    server.on("/on", handleOn);
    server.on("/off", handleOff);
    server.on("/settings", HTTP_POST, handleSettings);
    server.on("/restart", handleRestart);
    server.begin();

    // btnSET.begin();
    // btnUP.begin();
    // btnDN.begin();

    // EEPROM logic can go here to load both boreSettings and sumpSettings
    loadSettings();
    printSettings("BORE SETTINGS", boreSettings);
    printSettings("SUMP SETTINGS", sumpSettings);
    Serial.println("System Booted on ESP32");
}

void loop()
{
    btnSet.tick();
    btnUp.tick();
    btnDown.tick();
    handleHeldRepeat();
    server.handleClient();
//     if (systemMode != 2)
//     {
//         if (boreError >= 3)
//         {
//             blinkLED(2, 1);
//         }
//         else if (boreMotorRunning)
//         {
//             blinkLED(3, 1);
//         }
//         else if (!boreMotorRunning && boreError == 1 && sumpMotorRunning)
//         {
//             Serial.println("BoreMotor not running  && boreError == 1");
//             blinkLED(1, 1);
//         }
//         else
//         {
//             blinkLED(4, 1);
//         }
   
//         if (sumpError >= 3)
//         {
//             blinkLED(2, 0);
//         }
//         else if (sumpMotorRunning)
//         {
//             blinkLED(3, 0);
//         }
//         else if (!sumpMotorRunning && sumpError == 1 && boreMotorRunning)
//         {
//             blinkLED(1, 0);
//         }
//         else
//         {
//             blinkLED(4, 0);
//         }
//  }

    /*
                blinkLED(error >= 3 ? 2 : 4, isBore);
            motor = false;


    */
    if (!inMenu && millis() - lastPzemRead >= pzemReadInterval)
    {
        readPzemValues();
        lastPzemRead = millis();
    }

    if (!inMenu && millis() - lastScreenSwitch >= 5000)
    {
        Serial.printf("boreError: %d (%s) sumpError: %d (%s), boreMotorRunning: %d, sumpMotorRunning: %d \n", boreError, boreErrorMessage, sumpError, sumpErrorMessage, boreMotorRunning, sumpMotorRunning);

        // Serial.printf("FLOAT_BORE_OHT_PIN: %d", digitalRead(FLOAT_BORE_OHT_PIN));
        // Serial.printf(", FLOAT_SUMP_OHT_PIN: %d", digitalRead(FLOAT_SUMP_OHT_PIN));
        // Serial.printf(", FLOAT_SUMP_UGT_PIN: %d\n", digitalRead(FLOAT_SUMP_UGT_PIN));

        showStatusScreen();
        if (boreError >= 3 || sumpError >= 3)
        {
            screenIndex = 2;
            // reset error conditions if OV or UV
            if (boreError == 3 || sumpError == 3)
            {
                boreError = checkSystemStatus(true);
                sumpError = checkSystemStatus(false);
            }
            else
            { // wait for 1Hr and check again if Dry, OC or UC
                if (millis() - lastBoreErrorTime > 60 * 60 * 1000UL)
                {
                    boreError = checkSystemStatus(true);
                    sumpError = checkSystemStatus(false);
                }
            }
        }
        else
        {
            screenIndex = (screenIndex + 1) % 10;
        }
        lastScreenSwitch = millis();
    }

    if (!digitalRead(SW_AUTO) && digitalRead(SW_MANUAL)) // auto mode
    {
        systemMode = 0;
        if (millis() / 1000 > boreSettings.PowerOnDelay)
        {
            controlMotor(true, true);
            controlMotor(false, true);
            // controlSumpAuto();
        }
    }
    else if (!digitalRead(SW_MANUAL) && digitalRead(SW_AUTO)) // manual mode
    {
        systemMode = 1;
    }
    else if (digitalRead(SW_MANUAL) && digitalRead(SW_AUTO))
    {
        systemMode = 2;
        if (!calibCancelled)
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Calibrating.....");
            lcd.setCursor(0, 1);
            lcd.print("Waiting for intialize.");
            while (digitalRead(KEY_SET) == HIGH)
            {
                // Serial.println("WAiting....");
                if (digitalRead(KEY_UP) == LOW || digitalRead(KEY_DOWN) == LOW)
                {
                    // Serial.println("calcelled....");
                    calibCancelled = true;
                    return;
                }
            }
            calibrateMotor(true);
            calibrateMotor(false);
        }
        else
        {
            // lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("cancelled.......");
            lcd.setCursor(0, 1);
            lcd.print("Change Sw 2 AUTO");
        }
    }
    else if (!digitalRead(SW_MANUAL) && !digitalRead(SW_AUTO)) // motor ON switch
    {
        /* code */
    }
}
