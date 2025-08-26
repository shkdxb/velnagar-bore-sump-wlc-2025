// main.cpp — Dual motor controller (BORE + SUMP)
// Minimal changes from your uploaded file — implements pending logic, manual toggles, single-motor-at-a-time and bore priority.

#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <esp_wifi.h> // for esp_wifi_set_mac
#include <PZEM004Tv30.h>
#include <OneButton.h>
#include <WiFiManager.h>
#include <WebServer.h>
// #include <TFT_eSPI.h>

uint8_t realStaMac[6] = {0xCC, 0xDB, 0xA7, 0x2F, 0xEF, 0x4C};

// mac address AP:02:0f:b5:2f:ef:4c STA:cc:db:a7:2f:ef:4c
WebServer server(80);
// --------------------- Pin Definitions (ESP32) -------------------------
#define FLOAT_BORE_OHT_PIN 18 // 0=LOW, 1=Full
#define FLOAT_SUMP_UGT_PIN 12 // 0=empty, 1=full
#define FLOAT_SUMP_OHT_PIN 13 // 0=LOW, 1=Full

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

// === Button Setup ===
OneButton btnSet(KEY_SET, true);
OneButton btnUp(KEY_UP, true);
OneButton btnDown(KEY_DOWN, true);

// LCD and PZEM
// LiquidCrystal_AIP31068_I2C lcd(0x3E, 16, 2);
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
int systemMode = 0; // 0=AUTO, 1=MANUAL, 2=CALIB
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
// unsigned long statusScreenTimer = 0;
// bool statusScreenHold = false;
unsigned long lastStatusChange = 0;
const unsigned long statusInterval = 10000; // 10 seconds per page

int showingBore = 0; // start with Bore

const unsigned long pzemReadInterval = 1000;
const uint8_t totalMenuItems = 23;
uint8_t screenIndex = 0;
bool ledState = false;
unsigned long lastRepeatTime = 0;
unsigned long buttonHoldStartTime = 0;
bool upHeld = false;
bool downHeld = false;
bool powerFailed = 1; // set on boot (so motors can start once PowerOnDelay passed)

// Pending flags: when user requests the other motor while one is running
bool borePending = false;
bool sumpPending = false;

// helper forward declarations
void loadSettings();
void saveSettings();
void printSettings(const char *label, const Settings &s);
void onSetClick();
void updateMenuValue(bool increse);
void readPzemValues();
int checkSystemStatus(bool isBore);
void startMotor(bool isBore);
void stopMotor(bool isBore);
void tryStartPending();
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

void drawMenuLabels();
void updateValue(int i, bool highlight);
void drawAllValues();
void drawStatusLine(int row, const char *label, const String &value);
void drawStatusScreen(bool isBore);

#include <menu_display_eTFT_eSPI.cpp>
#include <GIF_JPEG_TFTeSPI_SD.cpp>

// TaskHandle_t tickerTaskHandle;

// void tickerTask(void *pvParameters)
// {
//     for (;;)
//     {
//         updateTicker();                      // scrolls sprite
//         vTaskDelay(40 / portTICK_PERIOD_MS); // ~25 FPS
//     }
// }

// ---------------- EEPROM ----------------
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

// ---------------- SET button ----------------
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
            drawMenuLabels();
            drawAllValues();
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
        }
        drawMenuLabels();
        drawAllValues();
    }
}

// ---------------- UP/DOWN (menu or manual) ----------------
void updateMenuValue(bool increse)
{
    if (!inMenu)
    {
        // If not in menu: in MANUAL mode UP/DOWN control motors (toggle)
        if (systemMode == 1)
        {
            if (increse)
            { // UP - toggle BORE in manual
                boreError = checkSystemStatus(true);
                if (!boreMotorRunning)
                {
                    if (sumpMotorRunning)
                    {
                        // Sump running: set bore pending (do NOT stop sump in manual)
                        borePending = true;
                        Serial.println("Bore pending (manual) — will start after sump completes");
                    }
                    else
                    {
                        if (boreError == 1)
                        {
                            startMotor(true);
                        }
                        else
                        {
                            // cannot start due to error: flash error LED
                            Serial.println("Cannot start Bore — condition not met");
                        }
                    }
                }
                else
                {
                    // Bore is running — toggle off
                    stopMotor(true);
                }
            }
            else
            { // DOWN - toggle SUMP in manual
                sumpError = checkSystemStatus(false);
                if (!sumpMotorRunning)
                {
                    if (boreMotorRunning)
                    {
                        // Bore running: set sump pending (do NOT stop bore in manual)
                        sumpPending = true;
                        Serial.println("Sump pending (manual) — will start after bore completes");
                    }
                    else
                    {
                        if (sumpError == 1)
                        {
                            startMotor(false);
                        }
                        else
                        {
                            Serial.println("Cannot start Sump — condition not met");
                        }
                    }
                }
                else
                {
                    // Sump is running — toggle off
                    stopMotor(false);
                }
            }
        }
        else
        {
            // not in menu and not manual: ignore or provide feedback
            Serial.println("UP/DOWN ignored — not in menu and not in MANUAL");
        }

        return;
    }

    // in menu: edit the menu value
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
    drawMenuLabels();
    drawAllValues();
}

// ---------------- PZEM ----------------
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
bool stabilizationStart = 0;
unsigned long boreStabilizationStart = 10;
unsigned long sumpStabilizationStart = 10;
int stabilizationDelay = 10;
// ---------------- CHECK SYSTEM ----------------
// Return codes: 0=OK, 1=OHT Low (should run), 2=UGT low, 3=OV/UV, 4=OC, 5=UC, 6=Dry run
int checkSystemStatus(bool isBore)
{
    const char *errorMsg = nullptr;
    Settings &s = isBore ? boreSettings : sumpSettings;
    bool motor = isBore ? boreMotorRunning : sumpMotorRunning;
    int ohtPin = isBore ? FLOAT_BORE_OHT_PIN : FLOAT_SUMP_OHT_PIN;
    unsigned long &lastErrTime = isBore ? lastBoreErrorTime : lastSumpErrorTime;
    // unsigned long &lastOnTime = isBore ? boreLastOnTime : sumpLastOnTime;
    char *targetMsg = isBore ? boreErrorMessage : sumpErrorMessage;
    unsigned long &stabilizationStart = isBore ? boreStabilizationStart : sumpStabilizationStart;

    int errorCode = 0;
    if (boreError >= 4 || sumpError >= 4)
    {
        if (millis() - lastErrTime < 60 * /*60 */ 2 * 1000UL) // for 2 minute dont cheack errors again
        {
            return isBore ? boreError : sumpError;
        }
    }

    // --- Motor stabilization delay ---
    //  If motor just turned on, set stabilization timer
    if (motor && stabilizationStart == 0)
    {
        stabilizationStart = millis();
    }
    if (!motor)
    {
        stabilizationStart = 0; // reset when motor off
    }
    // Wait until after stabilization delay before current/PF checks
    bool stabilized = !motor || (millis() - stabilizationStart >= stabilizationDelay * 1000UL);

    if ((voltage < s.underVoltage || voltage > s.overVoltage) && s.detectVoltage)
    {
        errorMsg = (voltage < s.underVoltage) ? "LOW Voltage" : "HIGH Voltage";
        errorCode = 3;
    }
    else if (motor && current > s.overCurrent && s.detectCurrent && stabilized)
    {
        errorMsg = "Over current";
        errorCode = 4;
    }
    else if (motor && current < s.underCurrent && s.detectCurrent && stabilized)
    {
        errorMsg = "Under current";
        errorCode = 5;
    }
    else if (motor && current < s.underCurrent && pf < s.minPF && s.dryRun && stabilized)
    {
        errorMsg = "Dry run";
        errorCode = 6;
    }
    else if (!isBore && !digitalRead(FLOAT_SUMP_UGT_PIN))
    {
        errorMsg = "UGT empty";
        errorCode = 2;
    }
    else if (!digitalRead(ohtPin))
    {
        errorMsg = "OHT LOW";
        errorCode = 1;
    }

    lastErrTime = millis();
    if (errorMsg)
    {
        strncpy(targetMsg, errorMsg, sizeof(boreErrorMessage) - 1);
        targetMsg[sizeof(boreErrorMessage) - 1] = '\0';
        return errorCode;
    }

    // No error
    strncpy(targetMsg, "OK", sizeof(boreErrorMessage) - 1);
    targetMsg[sizeof(boreErrorMessage) - 1] = '\0';
    return 0;
}

// ---------------- MOTOR HELPERS ----------------
void startMotor(bool isBore)
{
    int relayPin = isBore ? BORE_MOTOR_RELAY_PIN : SUMP_MOTOR_RELAY_PIN;
    bool &motor = isBore ? boreMotorRunning : sumpMotorRunning;
    unsigned long &lastOnTime = isBore ? boreLastOnTime : sumpLastOnTime;

    if (!motor)
    {
        digitalWrite(relayPin, HIGH);
        motor = true;
        lastOnTime = millis();
        Serial.printf("%s Motor turned ON\n", isBore ? "Bore" : "Sump");
    }
}

void stopMotor(bool isBore)
{
    int relayPin = isBore ? BORE_MOTOR_RELAY_PIN : SUMP_MOTOR_RELAY_PIN;
    bool &motor = isBore ? boreMotorRunning : sumpMotorRunning;
    unsigned long &lastOffTime = isBore ? boreLastOffTime : sumpLastOffTime;

    if (motor)
    {
        digitalWrite(relayPin, LOW);
        motor = false;
        lastOffTime = millis();
        Serial.printf("%s Motor turned OFF\n", isBore ? "Bore" : "Sump");
        // If the other motor was pending, try to start it now
        tryStartPending();
    }
}

// Start pending motor if any (bore preferred).
void tryStartPending()
{
    // Bore has priority
    if (borePending && !boreMotorRunning && !sumpMotorRunning)
    {
        boreError = checkSystemStatus(true);
        if (boreError == 1)
        {
            Serial.println("Starting pending Bore...");
            borePending = false;
            startMotor(true);
            return;
        }
    }

    // Then Sump
    if (sumpPending && !sumpMotorRunning && !boreMotorRunning)
    {
        sumpError = checkSystemStatus(false);
        if (sumpError == 1)
        {
            Serial.println("Starting pending Sump...");
            sumpPending = false;
            startMotor(false);
            return;
        }
    }
}

// ---------------- MOTOR CONTROL (auto/manual) ----------------
void controlMotor(bool isBore, bool isAuto)
{
    Settings &s = isBore ? boreSettings : sumpSettings;
    bool &motor = isBore ? boreMotorRunning : sumpMotorRunning;
    unsigned long &lastOnTime = isBore ? boreLastOnTime : sumpLastOnTime;
    unsigned long &lastOffTime = isBore ? boreLastOffTime : sumpLastOffTime;
    // int relayPin = isBore ? BORE_MOTOR_RELAY_PIN : SUMP_MOTOR_RELAY_PIN;
    int &error = isBore ? boreError : sumpError;

    error = checkSystemStatus(isBore);

    // If motor is OFF: see if we can start
    if (!motor)
    {
        // If this is SUMP and BORE currently running, SUMP must wait (Bore priority)
        if (!isBore && boreMotorRunning)
            return;

        // Determine readiness (power failure overrides timer)
        bool readyToStart = (powerFailed || (s.cyclicTimer ? (millis() - lastOffTime >= (unsigned long)s.offTime * 60000UL) : true)) && (error == 1);

        // AUTO: Bore priority — if bore wants to start while sump running, we stop sump and start bore.
        if (isAuto)
        {
            if (readyToStart)
            {
                Serial.printf("readyToStart: %d \n", readyToStart);
                if (isBore && sumpMotorRunning)
                {
                    // stop sump immediately to allow bore to run (auto priority behaviour)
                    stopMotor(false);
                }
                startMotor(isBore);
                powerFailed = 0; // clear power-failed flag after first auto-start
            }
            // else not ready -> do nothing
        }
        else
        {
            // In manual mode, we do not auto-start motors here.
            // Manual starts happen via button handler (updateMenuValue).
            // However if user had set pending flags (from manual action) and condition becomes OK,
            // tryStartPending will start them (called by stopMotor).
        }
    }
    else
    {
        // Motor is ON: check stop conditions
        if (s.onTime < 1)
            s.onTime = 1;

        bool stopCondition = (error == 0 || error >= 2);
        // Serial.printf("error: %d, stopCondition: %d \n", error, stopCondition);

        if (!isAuto)
        {
            // In manual mode, we stop if user toggles off (handled elsewhere) or conditions require stop
            if (stopCondition)
            {
                stopMotor(isBore);
            }
        }
        else
        {
            // Auto mode: stop on error or after onTime if cyclicTimer enabled
            if (stopCondition || (s.cyclicTimer && (millis() - lastOnTime >= (unsigned long)s.onTime * 60000UL)))
            {
                stopMotor(isBore);
            }
        }
    }
}

// ---------------- LED blink helper ----------------
// Type 1: slow blink (500ms), 2: fast triple-blink pattern, 3: ON, 4: OFF
void blinkLED(int type, bool isBore)
{
    static bool ledStateBore = false;
    static bool ledStateSump = false;
    static unsigned long lastBlink = 0;
    static int errorBlinkCountBore = 0;
    static int errorBlinkCountSump = 0;

    int ledPin = isBore ? BORE_MOTOR_STATUS_LED : SUMP_MOTOR_STATUS_LED;
    bool &ledState = isBore ? ledStateBore : ledStateSump;
    int &errorCount = isBore ? errorBlinkCountBore : errorBlinkCountSump;
    unsigned long now = millis();

    if (type == 1)
    {
        // slow toggle every 500ms
        if (now - lastBlink >= 500)
        {
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(ledPin, ledState);
        }
    }
    else if (type == 2)
    {
        // triple blink then pause
        if (errorCount >= 6)
        {
            digitalWrite(ledPin, LOW);
            if (now - lastBlink > 2000)
            {
                lastBlink = now;
                errorCount = 0;
            }
            return;
        }

        if (now - lastBlink >= 250)
        {
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(ledPin, ledState);
            errorCount++;
        }
    }
    else if (type == 3)
    {
        digitalWrite(ledPin, HIGH);
    }
    else if (type == 4)
    {
        digitalWrite(ledPin, LOW);
    }
}

// ---------------- Web handlers ----------------
void handleRootold()
{
    // (kept for reference — not used)
    server.send(200, "text/html", "<html><body>Old root</body></html>");
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
    html += "<tr><td><strong>Bore Motor Status:</strong></td><td>" + String(boreMotorRunning ? "ON" : "OFF") + "</td></tr>";
    unsigned long elapsed = (millis() - (boreMotorRunning ? boreLastOnTime : boreLastOffTime)) / 1000;
    unsigned long remaining = (boreMotorRunning ? boreSettings.onTime : boreSettings.offTime) * 60 - elapsed;
    html += "<tr><td><strong>Bore Remaining Time(min):</strong></td><td>" + String(remaining) + "</td></tr>";
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
    html += "<tr><td><strong>Sump Motor Status:</strong></td><td>" + String(sumpMotorRunning ? "ON" : "OFF") + "</td></tr>";
    elapsed = (millis() - (sumpMotorRunning ? sumpLastOnTime : sumpLastOffTime)) / 1000;
    remaining = (sumpMotorRunning ? sumpSettings.onTime : sumpSettings.offTime) * 60 - elapsed;
    html += "<tr><td><strong>Sump Remaining Time (min):</strong></td><td>" + String(remaining) + "</td></tr>";
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
        startMotor(true);
    }
    else if (motor == "sump")
    {
        startMotor(false);
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
        stopMotor(true);
    }
    else if (motor == "sump")
    {
        stopMotor(false);
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

// ---------------- CALIBRATION ----------------
bool calibCancelled = 0;
void calibrateMotor(bool bore)
{
    if (boreError >= 2 || sumpError >= 2)
    {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        tft.print("Bore:");
        tft.print(boreErrorMessage);
        tft.setCursor(0, 1);
        tft.print("SUMP:");
        tft.print(sumpErrorMessage);
        return;
    }
    if (boreMotorRunning || sumpMotorRunning)
    {
        stopMotor(true);
        stopMotor(false);
    }

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
        tft.setCursor(0, 1);
        tft.print("Wait for ");
        tft.print(20 - currentSec);
        tft.print(" sec");
    }

    const int samples = 5;
    const unsigned long sampleDelay = 500;
    float sumV = 0, sumI = 0, sumPF = 0;

    Serial.println("Starting auto-calibration...");
    tft.setCursor(0, 0);
    tft.print(bore ? "Starting Bore" : "Starting Sump");
    tft.setCursor(0, 1);
    tft.print("     Calibration");

    for (int i = 0; i < samples; i++)
    {
        float v = pzem.voltage();
        float i_ = pzem.current();
        float pf = pzem.pf();

        if (isnan(v) || isnan(i_) || isnan(pf))
        {
            Serial.println("Error: Invalid PZEM reading (NaN)");
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            tft.print("Error:");
            tft.setCursor(0, 1);
            tft.print("PZEM Reading ERR");
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
        tft.setCursor(0, 0);
        tft.print("Setting Saved   ");
        tft.setCursor(0, 1);
        tft.print("Change Sw 2 AUTO");
        delay(2000);
    }
}

// ---------------- Held-repeat support for long-press acceleration ----------------
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

// ---------------- Setup ----------------
void setup()
{ // Force STA mode
    WiFi.mode(WIFI_STA);

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

    // Set the STA MAC to your fixed hardware MAC (if needed)
    esp_wifi_set_mac(WIFI_IF_STA, realStaMac);

    delay(200); // Let MAC setting settle
    Serial.begin(115200);
    Serial2.begin(9600);

    tft.init();
    tft.setCursor(0, 0);
    // lcd.setContrast(124);
    tft.setRotation(1);
    // Create ticker sprite(full width, 36px height)
    tickerSprite.createSprite(tft.width(), tickerHeight);
    tickerSprite.setTextColor(TFT_GREEN, TFT_BLACK);
    tickerSprite.setTextSize(2);

    tickerX = tft.width(); // start offscreen right
    tft.print("Water Ctrl Start");
    gifJpegInitialize();
    // xTaskCreatePinnedToCore(
    //     tickerTask,        // function to run
    //     "TickerTask",      // name
    //     8192,              // stack size
    //     NULL,              // params
    //     0,                 // priority
    //     &tickerTaskHandle, // task handle
    //     1                  // run on core 1
    // );

    // Button binding
    btnSet.attachClick(onSetClick);

    btnUp.attachClick([]()
                      {
                          updateMenuValue(true); // single +1 or manual toggle
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
                            updateMenuValue(false); // single -1 or manual toggle
                        });

    btnDown.attachLongPressStart([]()
                                 {
                                     buttonHoldStartTime = millis();
                                     lastRepeatTime = millis();
                                     downHeld = true; });

    btnDown.attachLongPressStop([]()
                                { downHeld = false; });

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

    loadSettings();
    printSettings("BORE SETTINGS", boreSettings);
    printSettings("SUMP SETTINGS", sumpSettings);
    Serial.println("System Booted on ESP32");
}
unsigned long secondsSinceBoot = millis() / 1000;
// ---------------- Main Loop ----------------
void loop()
{
    btnSet.tick();
    btnUp.tick();
    btnDown.tick();
    handleHeldRepeat();
    server.handleClient();
    if (millis() - lastStatusChange >= statusInterval)
    {
        if (boreMotorRunning || sumpMotorRunning)
        {
            String fname = mediaFiles[currentFile];
            currentFile = (currentFile + 1) % mediaFiles.size();
            if (fname.endsWith(".gif") || fname.endsWith(".GIF"))
            {
                tft.fillScreen(TFT_BLACK);
                play_gif(fname.c_str());
            }
        }
        else
        {
            drawStatusScreen(showingBore);
            // drawMenuLabels();
            lastStatusChange = millis();
        }
    }
    static unsigned long lastScroll = 0;
    if (millis() - lastScroll >= 40)
    { // ~25 FPS
        updateTicker();
        lastScroll = millis();
    }
    // updateTicker();
    // Update sensor reads
    if (!inMenu && millis() - lastPzemRead >= pzemReadInterval)
    {
        readPzemValues();
        lastPzemRead = millis();
    }

    // Update screen every few seconds
    if (!inMenu && millis() - lastScreenSwitch >= 5000)
    {
        // Serial.printf("boreError: %d (%s) sumpError: %d (%s), boreMotorRunning: %d, sumpMotorRunning: %d \n", boreError, boreErrorMessage, sumpError, sumpErrorMessage, boreMotorRunning, sumpMotorRunning);
        unsigned long secondsSinceBoot = millis() / 1000;
        unsigned long remaining = boreSettings.PowerOnDelay - secondsSinceBoot;

        if ((millis() / 1000) < boreSettings.PowerOnDelay)
        {
            tft.setCursor(0, 0);
            tft.print("Waiting to start ");
            tft.setCursor(0, 1);
            tft.print(remaining);
            tft.print(" sec   "); // extra spaces clear leftovers
            return;
        }
        else
        {
            drawStatusScreen(showingBore);
            if (boreError >= 3 || sumpError >= 3)
            {
                screenIndex = 2;
            }
            else
            {
                screenIndex = (screenIndex + 1) % 9;
            }
            lastScreenSwitch = millis();
        }
    }

    // LED state updates for each motor (non-blocking)
    // Decide Bore LED mode
    if (systemMode != 2)
    {
        int boreMode = 4; // default OFF
        if (boreMotorRunning)
        {
            boreMode = 3;
        }
        else if (boreError >= 2)
        {
            boreMode = 2;
        }
        else if (borePending || (!boreMotorRunning && boreError == 1))
        {
            boreMode = 1;
        }
        // Decide Sump LED mode
        int sumpMode = 4; // default OFF
        if (sumpMotorRunning)
        {
            sumpMode = 3; // on
        }
        else if (sumpError >= 2)
        {
            sumpMode = 2; // fast blink
        }
        else if (sumpPending || (!sumpMotorRunning && sumpError == 1))
        {
            sumpMode = 1; // slow blink
        }

        // Apply
        blinkLED(boreMode, true);
        blinkLED(sumpMode, false);
    }

    // Modes handling: note your switch logic uses INPUT_PULLUP
    if (!digitalRead(SW_AUTO) && digitalRead(SW_MANUAL)) // auto mode
    {
        systemMode = 0;
        // Wait for power-on delay before starting motors
        if ((millis() / 1000) > boreSettings.PowerOnDelay)
        {
            // Bore first (priority), then sump
            controlMotor(true, true);  // auto control for Bore
            controlMotor(false, true); // auto control for Sump
        }
        else
        {
        }
    }
    else if (!digitalRead(SW_MANUAL) && digitalRead(SW_AUTO)) // manual mode
    {
        systemMode = 1;
        // Manual motor control occurs via button handlers (updateMenuValue)
        // However we still refresh errors so UI & LEDs are accurate
        boreError = checkSystemStatus(true);
        sumpError = checkSystemStatus(false);
    }
    else if (digitalRead(SW_MANUAL) && digitalRead(SW_AUTO)) // calibration mode (both high)
    {
        systemMode = 2;
        if (!calibCancelled)
        {
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            tft.print("Calibrating.....");
            tft.setCursor(0, 1);
            tft.print("Waiting for intialize.");
            while (digitalRead(KEY_SET) == HIGH)
            {
                if (digitalRead(KEY_UP) == LOW || digitalRead(KEY_DOWN) == LOW)
                {
                    calibCancelled = true;
                    return;
                }
            }
            calibrateMotor(true);
            calibrateMotor(false);
        }
        else
        {
            tft.setCursor(0, 0);
            tft.print("cancelled.......");
            tft.setCursor(0, 1);
            tft.print("Change Sw 2 AUTO");
        }
    }
    else if (!digitalRead(SW_MANUAL) && !digitalRead(SW_AUTO))
    {
        // Both pressed? treat as a special 'force' state - currently unused
    }
}
