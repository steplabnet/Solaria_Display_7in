/**
 * @file TaskSerialPoll.cpp (REVISED - CRC16 as JSON Element)
 * @brief RS485 Communication with CRC16 as JSON Element
 *
 * =============================================================================
 * PROTOCOL CHANGE (v2.0)
 * =============================================================================
 * Previous Format:  {JSON}|CRC16\n
 * Current Format:   {JSON,"crc16":"XXXX"}\n
 *
 * The CRC16 is now integrated into the JSON structure as the "crc16" field,
 * making it compatible with serial_ping.py (Python master controller).
 *
 * =============================================================================
 * FUNCTIONALITY OVERVIEW
 * =============================================================================
 * This module handles RS485 serial communication between this device (Slave)
 * and a Master controller (serial_ping.py). It operates as a FreeRTOS task.
 *
 * 1. COMMUNICATION PROTOCOL
 *    - Physical: RS485 (Half-duplex), 38400 baud, 8N1.
 *    - Data Format: JSON with "crc16" element containing 4-digit Hex CRC.
 *    - Integrity: CRC16-CCITT validation (Poly 0x1021, Init 0xFFFF).
 *    - Addressing: Ignores packets where "moduleId" does not match DeviceID.
 *    - Termination: Newline character (\n) ends each message.
 *
 * 2. DATA RECEPTION (RX)
 *    Parses incoming JSON commands to perform:
 *    - System Commands: "restart", "update", "setpoint" (action)
 *    - Configuration: Button labels, screen labels
 *    - Telemetry: Temperatures, operation modes, limits
 *    - Setpoints: Target temperatures (with local edit protection)
 *
 * 3. DATA TRANSMISSION (TX)
 *    Sends JSON response ("ping") after processing valid messages.
 *    - Reports: Device ID, command type, status
 *    - Setpoints: Rounded to 0.5 step increments
 *    - CRC: Automatically calculated and added
 *
 * 4. CRC VERIFICATION
 *    - RX: Extracts "crc16" field, validates against recalculated value
 *    - TX: Uses CRCTransmitter::addCrcAndSend() for automatic CRC addition
 *
 * =============================================================================
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <CRCTransmitter.h>
#include "TaskSerialPoll.h"
#include <globals.h>
#include "Parametri.h"
#include "settings.h"
#include "main.h"
#include "home_buttons.h"
#include "ui.h"
#include "lvgl_port.h"
#include <HardwareSerial.h>
#include <math.h>

// --- HARDWARE CONFIGURATION ---
// RS485 pins for the Waveshare ESP32-S3-Touch-LCD-7 (MCU side, per the official
// 05_RS485 example): RX = GPIO15, TX = GPIO16. The board silkscreen "TXD15/RXD16"
// is the transceiver side and is reversed relative to the MCU.
// NOTE: GPIO43/44 are the USB UART0 debug pins — never use them for RS485.
#ifndef RS485Serial_RX_PIN
#define RS485Serial_RX_PIN 15
#endif

#ifndef RS485Serial_TX_PIN
#define RS485Serial_TX_PIN 16
#endif

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2
#endif

// --- CONSTANTS ---
static constexpr bool CALL_ON_CRC_FAIL = false;
static constexpr int RS485_BAUD_RATE = 38400;
static constexpr int RX_BUFFER_SIZE = 2048;

// --- GLOBALS ---
int labelCount = 0;
extern bool CONFIG_RECEIVED;
extern bool SETPOINT_CHANGED;
extern bool PERFORM_OTA_UPDATE;
extern String config_otaURL;
extern String config_ssid;
extern String config_pass;
extern bool TRIGGER_COMUNICATION;
extern float newSetPoint[MAX_LABELS];
extern int16_t DeviceID;
extern HardwareSerial RS485Serial;
extern long azzera_counter;
extern String pageLabels[MAX_PAGES][MAX_LABELS];
extern String pageLabelsOff[MAX_PAGES][MAX_LABELS];
extern String pageTemperatures[MAX_PAGES][NUM_SENSORS];
extern float pageSetPoints[MAX_PAGES][NUM_SENSORS];
extern String pageRegulatorLabels[MAX_PAGES][NUM_SENSORS];
extern int pageModes[MAX_PAGES][NUM_SENSORS];
extern int pageMins[MAX_PAGES][NUM_SENSORS];
extern int pageMaxs[MAX_PAGES][NUM_SENSORS];
extern float pageNewSetPoints[MAX_PAGES][MAX_LABELS];
extern int pageValueChanged[MAX_PAGES][MAX_LABELS];
extern bool pageAvailable[MAX_PAGES];
extern int activePage;
extern void applyPageConfig(int page);
extern unsigned long arcLastTouchTime;
extern int valueChanged[MAX_LABELS];
extern int pageBtnEn[MAX_PAGES][NUM_SENSORS];

unsigned long lastSendStatusTime = 0;
bool RS485_transmitting = false;
bool bootAnswerPending = true;
static bool varsReceivedFromCentral = false;
static bool pendingBootReply = false; // set when incoming page is not yet configured
static int sendPageIdx = 0; // rotates through available pages on each ping response

// -------------------------------------------------------------------------
// PROTOTYPES
// -------------------------------------------------------------------------
static void handleSystemCommands(const JsonDocument &doc);
static void processIncomingConfig(const JsonDocument &doc);
static void processIncomingTelemetry(const JsonDocument &doc);
static void processIncomingSetpoints(const JsonDocument &doc);
static void sendDeviceStatus();
static void transmitJsonPackage(JsonDocument &doc);
static bool verifyCrcAndParse(String &rxBuffer, JsonDocument &doc);
static int getDocPageIndex(const JsonDocument &doc);
static bool hasMatchingModuleId(const String &rxBuffer);

// CRC & Parsing Helpers
static uint16_t crc16_ccitt(const uint8_t *data, size_t len,
                            uint16_t poly = 0x1021, uint16_t init = 0xFFFF);
static bool parseHex16(const char *s, uint16_t &out);

// -------------------------------------------------------------------------
// LOGIC HELPERS
// -------------------------------------------------------------------------

int16_t updateMode(int16_t actualMode, int16_t newMode)
{
    switch (newMode)
    {
    case 200:
        return (actualMode != 12) ? 12 : actualMode;
    case 201:
        return (actualMode != 11) ? 11 : actualMode;
    case 202:
        return (actualMode != 10) ? 10 : actualMode;
    case 204:
        return (actualMode < 20 || actualMode > 23) ? 20 : actualMode;
    case 203:
        return (actualMode < 30 || actualMode > 32) ? 30 : actualMode;
    case 205:
        return actualMode;
    default:
        return actualMode;
    }
}

static inline void push1dec(JsonArray &arr, int16_t tenths)
{
    bool neg = tenths < 0;
    uint16_t a = neg ? -tenths : tenths;
    String num = neg ? "-" : "";
    num += String(a / 10) + "." + String(a % 10);
    arr.add(serialized(num));
}

static int getDocPageIndex(const JsonDocument &doc)
{
    int pageIdx = 0;
    if (doc.containsKey("page"))
    {
        int p = doc["page"].as<int>();
        if (p >= 1 && p <= MAX_PAGES)
            pageIdx = p - 1;
    }
    return pageIdx;
}

static bool hasMatchingModuleId(const String &rxBuffer)
{
    JsonDocument filter;
    filter["moduleId"] = true;

    JsonDocument idDoc;
    DeserializationError err = deserializeJson(idDoc, rxBuffer, DeserializationOption::Filter(filter));
    if (err)
    {
        if (DEBUG_ON)
            Serial.printf("[JSON ERR] moduleId pre-check failed: %s\n", err.c_str());
        return false;
    }

    if (!idDoc.containsKey("moduleId"))
    {
        if (DEBUG_ON)
            Serial.println("[ERR] Missing moduleId");
        return false;
    }

    int receivedId = idDoc["moduleId"];
    if (receivedId != DeviceID)
    {
        if (DEBUG_ON)
            Serial.printf("[IGNORE] Message for ID %d (ours: %d)\n",
                          receivedId, DeviceID);
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
// CRC VERIFICATION AND PARSING
// -------------------------------------------------------------------------

/**
 * @brief Verify CRC and parse incoming message
 *
 * Process:
 * 1. Parse entire line as JSON (includes crc16 field)
 * 2. Verify moduleId matches local DeviceID
 * 3. Verify CRC16 if present
 * 4. Return parsed document if valid
 *
 * @param rxBuffer Raw received buffer (including newline)
 * @param doc Output JsonDocument for valid message
 * @return true if message is valid and should be processed
 */
static bool verifyCrcAndParse(String &rxBuffer, JsonDocument &doc)
{
    // 1. Parse as JSON
    DeserializationError err = deserializeJson(doc, rxBuffer);
    if (err)
    {
        if (DEBUG_ON)
            Serial.printf("[JSON ERR] %s\n", err.c_str());
        return false;
    }

    // 2. Check moduleId
    if (!doc.containsKey("moduleId"))
    {
        if (DEBUG_ON)
            Serial.println("[ERR] Missing moduleId");
        return false;
    }

    int receivedId = doc["moduleId"];
    if (receivedId != DeviceID)
    {
        if (DEBUG_ON)
            Serial.printf("[IGNORE] Message for ID %d (ours: %d)\n",
                          receivedId, DeviceID);
        return false;
    }

    // 3. Verify CRC
    if (doc.containsKey("crc16"))
    {
        const char *receivedCrcStr = doc["crc16"];

        // Create a copy for CRC calculation (without crc16 field)

        JsonDocument docForCrc = doc;
        docForCrc.remove("crc16");

        // Serialize and calculate CRC
        String jsonForCrc;
        serializeJson(docForCrc, jsonForCrc);
        uint16_t calculated = crc16_ccitt((const uint8_t *)jsonForCrc.c_str(),
                                          jsonForCrc.length());

        // Parse received CRC
        uint16_t received = 0;
        parseHex16(receivedCrcStr, received);

        // Compare
        if (calculated != received)
        {
            Serial.printf("[DROP] CRC Mismatch: Calc=%04X, Recv=%04X\n",
                          calculated, received);
            if (!CALL_ON_CRC_FAIL)
                return false;
        }
        else if (DEBUG_ON)
        {
            Serial.printf("[CRC OK] %04X\n", received);
        }
    }
    else
    {
        Serial.println("[WARN] Message missing crc16 field");
    }

    return true;
}

// -------------------------------------------------------------------------
// JSON HANDLERS
// -------------------------------------------------------------------------

static void handleJson(const JsonDocument &doc)
{
    if (DEBUG_ON)
    {
        Serial.println(">> Message Processed for Local Device");
        serializeJson(doc, Serial);
        Serial.println();
    }

    // If the incoming page is not yet configured, flag a boot reply
    int incomingPage = getDocPageIndex(doc);
    pendingBootReply = doc.containsKey("page") && !pageAvailable[incomingPage];

    // Process all command types
    handleSystemCommands(doc);
    processIncomingConfig(doc);
    processIncomingTelemetry(doc);
    processIncomingSetpoints(doc);

    // Send response back to master
    sendDeviceStatus();
}

static void handleSystemCommands(const JsonDocument &doc)
{
    if (doc["command"] == "restart" || doc["command"] == "reboot")
    {
        Serial.println("[CMD] Restart requested");
        ESP.restart();
    }

    if (doc["command"] == "update")
    {
        String url = doc["url"] | "";
        String filename = doc["filename"] | "";
        String ssid = doc["ssid"] | "";
        String pass = doc["password"] | "";

        if (url == "" || filename == "" || ssid == "" || pass == "")
        {
            Serial.println("[CMD] OTA ignored: missing 'url', 'filename', 'ssid' or 'password'");
        }
        else
        {
            config_ssid = ssid;
            config_pass = pass;

            if (url.endsWith("/"))
                config_otaURL = url + filename;
            else
                config_otaURL = url + "/" + filename;

            // Defensive: tolerate a duplicated scheme in the incoming url
            // (e.g. "http://http://host"), which otherwise makes the HTTP
            // client treat "http" as the hostname and the DNS lookup fails.
            while (config_otaURL.startsWith("http://http://"))
                config_otaURL.remove(0, 7); // drop one leading "http://"
            while (config_otaURL.startsWith("https://https://"))
                config_otaURL.remove(0, 8); // drop one leading "https://"

            Serial.print("[CMD] OTA update requested: ");
            Serial.println(config_otaURL);
            PERFORM_OTA_UPDATE = 1;
        }
    }

    if (doc["action"] == "setpoint")
    {
        // Guard 1: 10 seconds since last status transmission
        if (millis() - lastSendStatusTime >= 10000)
        {
            // Guard 2: 15 seconds since last reset
            if (millis() - azzera_counter > 15000)
            {
                Serial.println("[CMD] Setpoint reset executed");
                SETPOINT_CHANGED = false;
                for (int i = 0; i < 12; i++)
                {
                    valueChanged[i] = 0;
                    delay(5000); // Note: Long blocking delay
                }
                azzera_counter = millis();
            }
        }
    }
}

static void processIncomingConfig(const JsonDocument &doc)
{
    // OFF-state labels for ON/OFF (mode 2) buttons. Parsed before "btns" so a
    // combined config doc resolves the active caption against fresh OFF labels.
    const char *labelsOffKey = doc.containsKey("btnsOff") ? "btnsOff" : (doc.containsKey("labelsOff") ? "labelsOff" : nullptr);
    if (labelsOffKey != nullptr)
    {
        int pageIdx = getDocPageIndex(doc);
        JsonArrayConst labels = doc[labelsOffKey];
        labelCount = 0;
        for (String value : labels)
        {
            if (labelCount >= MAX_LABELS)
                break;
            pageLabelsOff[pageIdx][labelCount] = value;
            labelCount++;
        }
        if (pageIdx == activePage)
            applyPageConfig(pageIdx);
    }

    const char *labelsKey = doc.containsKey("btns") ? "btns" : (doc.containsKey("label") ? "label" : nullptr);
    if (labelsKey != nullptr)
    {
        int pageIdx = getDocPageIndex(doc);

        // Exit the waiting screen only when the first page (index 0) is ready
        if (pageIdx == 0)
        {
            CONFIG_RECEIVED = true;
            // bootAnswerPending cleared when home screen is first shown
        }

        // Store labels in the per-page cache
        JsonArrayConst labels = doc[labelsKey];
        labelCount = 0;
        for (String value : labels)
        {
            if (labelCount >= MAX_LABELS)
                break;
            pageLabels[pageIdx][labelCount] = value;
            labelCount++;
        }
        pageAvailable[pageIdx] = true;

        // Only push to display/UI if this is the currently active page
        if (pageIdx == activePage)
        {
            applyPageConfig(pageIdx);

            if (lvgl_port_lock(-1))
            {
                lv_label_set_text(ui_labelBtn1, activeBtnLabel(display, 0).c_str());
                lv_label_set_text(ui_labelBtn2, activeBtnLabel(display, 1).c_str());
                lv_label_set_text(ui_labelBtn3, activeBtnLabel(display, 2).c_str());
                lv_label_set_text(ui_labelBtn4, activeBtnLabel(display, 3).c_str());
                lv_label_set_text(ui_labelBtn5, activeBtnLabel(display, 4).c_str());
                lv_label_set_text(ui_labelBtn6, activeBtnLabel(display, 5).c_str());
                lv_label_set_text(ui_labelBtn7, activeBtnLabel(display, 6).c_str());
                lv_label_set_text(ui_labelBtn8, activeBtnLabel(display, 7).c_str());
                lv_label_set_text(ui_labelBtn9, activeBtnLabel(display, 8).c_str());
                lv_label_set_text(ui_labelBtn10, activeBtnLabel(display, 9).c_str());
                lv_label_set_text(ui_labelBtn11, activeBtnLabel(display, 10).c_str());
                lv_label_set_text(ui_labeBtn12, activeBtnLabel(display, 11).c_str());
                lvgl_port_unlock();
            }
        }
    }

    const char *regLabelsKey = doc.containsKey("controlLabel") ? "controlLabel" : nullptr;
    if (regLabelsKey != nullptr)
    {
        int pageIdx = getDocPageIndex(doc);
        JsonArrayConst labels = doc[regLabelsKey];
        labelCount = 0;
        for (String value : labels)
        {
            if (labelCount < NUM_SENSORS)
            {
                pageRegulatorLabels[pageIdx][labelCount] = value;
                if (pageIdx == activePage)
                    display.regulatorLabel[labelCount] = value;
                labelCount++;
            }
        }

        if (pageIdx == activePage)
            applyPageConfig(pageIdx);
    }
}

static void processIncomingTelemetry(const JsonDocument &doc)
{
    int pageIdx = getDocPageIndex(doc);

    if (doc.containsKey("T"))
    {
        JsonArrayConst temperatures = doc["T"];
        labelCount = 0;
        for (JsonVariantConst item : temperatures)
        {
            if (labelCount >= MAX_LABELS)
                break;

            bool shouldUpdate = false;
            String parsedValue;

            if (item.is<float>() || item.is<int>() || item.is<long>() || item.is<double>())
            {
                float numericValue = item.as<float>();

                if (numericValue < -100.0f)
                {
                    parsedValue = "--";
                    shouldUpdate = true;
                }
                // Some senders use 0 as a "no reading" placeholder. Once a real
                // value has been shown, keep it instead of replacing it with 0.
                else if (!(numericValue == 0.0f && pageTemperatures[pageIdx][labelCount].length() > 0))
                {
                    parsedValue = String(numericValue);
                    shouldUpdate = true;
                }
            }
            else if (item.is<const char *>())
            {
                parsedValue = item.as<const char *>();
                parsedValue.trim();

                if (parsedValue.toFloat() < -100.0f)
                {
                    parsedValue = "--";
                    shouldUpdate = true;
                }
                else if (parsedValue == "0" || parsedValue == "0.0" || parsedValue == "0.00")
                {
                    if (pageTemperatures[pageIdx][labelCount].length() == 0)
                        shouldUpdate = true;
                }
                else if (parsedValue.length() > 0 && parsedValue != "null")
                    shouldUpdate = true;
            }

            if (shouldUpdate)
            {
                pageTemperatures[pageIdx][labelCount] = parsedValue;
                if (pageIdx == activePage)
                    display.temperature[labelCount] = parsedValue;
            }

            labelCount++;
        }

        // Update temperature labels on Screen1 immediately
        lv_obj_t *tempLabels[12] = {
            ui_tempLabel1, ui_tempLabel2, ui_tempLabel3, ui_tempLabel4,
            ui_tempLabel5, ui_tempLabel6, ui_tempLabel7, ui_tempLabel8,
            ui_tempLabel9, ui_tempLabel10, ui_tempLabel11, ui_tempLabel12};
        if (lvgl_port_lock(-1))
        {
            UpdateTempLabelBackgrounds(display);
            for (int i = 0; i < 12; i++)
            {
                if (pageIdx == activePage && display.temperature[i].length() > 0 && display.temperature[i] != "--")
                {
                    String t = display.temperature[i] + "\xC2\xB0"
                                                        "C";
                    lv_label_set_text(tempLabels[i], t.c_str());
                    lv_obj_clear_flag(tempLabels[i], LV_OBJ_FLAG_HIDDEN);
                }
                else if (pageIdx == activePage)
                {
                    lv_obj_add_flag(tempLabels[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            lvgl_port_unlock();
        }
    }

    if (doc.containsKey("min"))
    {
        JsonArrayConst vals = doc["min"];
        labelCount = 0;
        for (float value : vals)
        {
            if (labelCount < NUM_SENSORS)
            {
                pageMins[pageIdx][labelCount] = value;
                if (pageIdx == activePage)
                    display.min[labelCount] = value;
                labelCount++;
            }
        }
    }

    if (doc.containsKey("max"))
    {
        JsonArrayConst vals = doc["max"];
        labelCount = 0;
        for (float value : vals)
        {
            if (labelCount < NUM_SENSORS)
            {
                pageMaxs[pageIdx][labelCount] = value;
                if (pageIdx == activePage)
                    display.max[labelCount] = value;
                labelCount++;
            }
        }
    }

    // NEW: mode support
    if (doc.containsKey("mode"))
    {
        JsonArrayConst vals = doc["mode"];
        labelCount = 0;
        for (int value : vals)
        {
            if (labelCount >= NUM_SENSORS)
                break;

            if (value >= 0)
            {
                pageModes[pageIdx][labelCount] = value;
                if (pageIdx == activePage)
                    display.mode[labelCount] = value;
            }

            labelCount++;
        }
    }

    if (pageIdx == activePage)
        applyPageConfig(pageIdx);
}

static void processIncomingSetpoints(const JsonDocument &doc)
{
    if (doc.containsKey("Var"))
    {
        int pageIdx = getDocPageIndex(doc);
        varsReceivedFromCentral = true;
        JsonArrayConst vars = doc["Var"];
        labelCount = 0;
        for (float value : vars)
        {
            if (labelCount >= MAX_LABELS)
                break;

            if (pageValueChanged[pageIdx][labelCount])
            {
                pageSetPoints[pageIdx][labelCount] = pageNewSetPoints[pageIdx][labelCount];
            }
            else
            {
                pageSetPoints[pageIdx][labelCount] = String(value, 1).toFloat();
            }

            if (pageIdx == activePage)
            {
                bool recentlyTouched = (millis() - arcLastTouchTime < 30000UL) && (valueChanged[labelCount] == 1);
                if (!recentlyTouched)
                    display.setPoint[labelCount] = pageSetPoints[pageIdx][labelCount];
            }

            labelCount++;
        }

        if (pageIdx == activePage)
            applyPageConfig(pageIdx);
        SETPOINT_UPDATE = true;
    }
}

// -------------------------------------------------------------------------
// TRANSMISSION LOGIC
// -------------------------------------------------------------------------

/**
 * @brief Constructs and sends the status JSON response.
 *
 * Message Format:
 * {
 *   "moduleId": 32,
 *   "command": "ping",
 *   "type": "moduloDisplay",
 *   "vars": [s0, s1, ..., s11],  // 12 setpoint values
 *   "crc16": "XXXX"              // Auto-calculated
 * }
 *
 * Setpoint Logic:
 * - Rounds local setpoint changes to nearest 0.5 step
 * - Protects against external overrides during editing
 */
static void sendDeviceStatus()
{
    JsonDocument answer;

    // Build Header
    answer["moduleId"] = DeviceID;
    answer["command"] = "ping";
    answer["type"] = "moduloDisplay";

    if (bootAnswerPending || pendingBootReply)
        answer["answer"] = "boot";

    // Add "vars" and "btnEn" only after boot is complete and vars have been received
    if (varsReceivedFromCentral && !bootAnswerPending)
    {
        // Advance sendPageIdx to the next available page (wrapping around)
        for (int attempt = 0; attempt < MAX_PAGES; attempt++)
        {
            sendPageIdx = (sendPageIdx + 1) % MAX_PAGES;
            if (pageAvailable[sendPageIdx])
                break;
        }

        answer["page"] = sendPageIdx + 1;
        JsonArray varsArr   = answer["vars"].to<JsonArray>();
        JsonArray btnEnArr  = answer["btnEn"].to<JsonArray>();

        for (int i = 0; i < 12; i++)
        {
            float sp = pageSetPoints[sendPageIdx][i];

            if (pageValueChanged[sendPageIdx][i])
            {
                // ROUNDING: Round to nearest 0.5 (e.g., 20.3 -> 20.5)
                sp = round(pageNewSetPoints[sendPageIdx][i] * 2.0f) / 2.0f;
                pageNewSetPoints[sendPageIdx][i] = sp;
                if (sendPageIdx == activePage)
                    display.setPoint[i] = sp;
            }

            varsArr.add(sp);

            // btnEn: explicit stored state (only changes on button press or incoming btnEn)
            btnEnArr.add(pageModes[sendPageIdx][i] == 2 ? pageBtnEn[sendPageIdx][i] : 0);
        }
    }

    if (DEBUG_ON)
    {
        Serial.println("<< Sending Status Response");
        serializeJson(answer, Serial);
        Serial.println();
    }

    transmitJsonPackage(answer);

    // Update timestamp
    lastSendStatusTime = millis();
}

/**
 * @brief Transmits a JSON package with automatic CRC calculation
 *
 * Uses CRCTransmitter::addCrcAndSend() which:
 * 1. Removes any existing crc16 field
 * 2. Serializes JSON (without crc16)
 * 3. Calculates CRC16-CCITT
 * 4. Adds crc16 field as hex string
 * 5. Serializes final JSON
 * 6. Transmits with RS485 handshake and newline
 *
 * @param doc JsonDocument to transmit
 */
static void transmitJsonPackage(JsonDocument &doc)
{
    RS485_transmitting = true;

    // CRCTransmitter handles:
    // - CRC calculation
    // - CRC field addition
    // - RS485 handshake
    // - Newline termination
    CRCTransmitter::addCrcAndSend(doc, RS485Serial);

    // Stabilization delay
    delay(2);

    RS485_transmitting = false;
}

// -------------------------------------------------------------------------
// MAIN TASK
// -------------------------------------------------------------------------

void TaskSerialPoll(void *parameter)
{
    // Initialize RS485 serial port
    if (!RS485Serial)
    {
        RS485Serial.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485Serial_RX_PIN, RS485Serial_TX_PIN);
        Serial.println("[INIT] RS485 Serial Initialized");
    }

    String rxBuffer;
    rxBuffer.reserve(RX_BUFFER_SIZE);

    for (;;)
    {
        // Stop handling RS485 while an OTA update is pending/running. The flag
        // stays set for the whole performOTAUpdate() sequence, so we just idle
        // here (and drain any incoming bytes) until the device reboots.
        if (PERFORM_OTA_UPDATE)
        {
            while (RS485Serial.available())
                RS485Serial.read();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Don't receive while transmitting
        if (RS485_transmitting)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Check for incoming data
        if (RS485Serial.available())
        {
            // Read line until newline
            rxBuffer = RS485Serial.readStringUntil('\n');
            rxBuffer.trim();

            if (rxBuffer.length() > 0)
            {
                if (!hasMatchingModuleId(rxBuffer))
                    continue;

                JsonDocument doc;

                // Verify and parse: includes JSON parsing and CRC validation
                if (verifyCrcAndParse(rxBuffer, doc))
                {
                    // Mark communication received
                    TRIGGER_COMUNICATION = true;

                    // Process the message
                    handleJson(doc);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// -------------------------------------------------------------------------
// CRC UTILITIES
// -------------------------------------------------------------------------

/**
 * @brief CRC16-CCITT Calculation
 *
 * Matches implementation in Python (serial_ping.py) and original
 * CRCTransmitter class for compatibility verification.
 *
 * Parameters match standard CCITT:
 *   - Poly: 0x1021
 *   - Init: 0xFFFF
 *   - No input/output reflection
 */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len,
                            uint16_t poly, uint16_t init)
{
    uint16_t crc = init & 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ poly);
            else
                crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

/**
 * @brief Parse 4-digit hex string to uint16_t
 *
 * Accepts formats:
 * - "3A1B"
 * - "0x3A1B"
 * - "3a1b" (case-insensitive)
 *
 * @param s Hex string pointer
 * @param out Output uint16_t value
 * @return true if parse successful
 */
static bool parseHex16(const char *s, uint16_t &out)
{
    if (!s)
        return false;

    while (*s && isspace((unsigned char)*s))
        ++s;

    if (*s == '\0')
        return false;

    unsigned int v = 0;

    // Skip 0x prefix if present
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    int digits = 0;
    while (*s)
    {
        char c = *s++;
        if (isspace(c))
            break;

        v <<= 4;
        if (c >= '0' && c <= '9')
            v += (c - '0');
        else if (c >= 'a' && c <= 'f')
            v += (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v += (c - 'A' + 10);
        else
            return false;

        if (++digits > 4)
            return false;
    }

    if (digits == 0)
        return false;

    out = (uint16_t)(v & 0xFFFFu);
    return true;
}
