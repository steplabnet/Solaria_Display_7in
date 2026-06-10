/*
 * Project: LVGL 8.4 Port & UI Controller
 * Description: Manages Display, Touch (GT911), RS485 communication, and UI Logic.
 *
 * Hardware:
 *  - Waveshare RGB LCD (ESP32-S3)
 *  - GT911 Touch Controller
 *  - RS485 Transceiver
 *
 * Changes:
 *  - Ported LVGL 8.4
 *  - Fixed keyboard binding for ui_PasswText
 *  - REMOVED WiFi/ESP-NOW features
 */

#include <Arduino.h>
#include "main.h"
#include "parametri.h"
#include "globals.h"
#include "ArduinoJson.h"

// --- Hardware Drivers ---
#include "rgb_lcd_port.h"
#include "gt911.h"
#include <HardwareSerial.h>
#include "user_sd.h"

// --- LVGL & UI ---
#include "lvgl_port.h"
#include <demos/lv_demos.h>
#include "ui.h"
#include "lvgl_sd_fs.h"

// --- Logic Headers ---
#include "home_buttons.h"
#include "incoming_data.h"
#include "TaskSerialPoll.h"

// --- OTA & Network ---
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <SD_MMC.h>

// --- Background Image Descriptors (loaded into PSRAM at startup) ---
static lv_img_dsc_t home_img_dsc;
static lv_img_dsc_t info_img_dsc;
static lv_img_dsc_t setting_img_dsc;
static lv_img_dsc_t wait_img_dsc;

static bool loadBinToImgDsc(const char *sdPath, lv_img_dsc_t *dsc)
{
    File f = SD_MMC.open(sdPath, FILE_READ);
    if (!f) {
        Serial.printf("loadBinToImgDsc: cannot open %s\n", sdPath);
        return false;
    }
    size_t size = f.size();
    uint8_t *buf = (uint8_t *)ps_malloc(size);
    if (!buf) {
        Serial.printf("loadBinToImgDsc: ps_malloc failed (%u bytes)\n", size);
        f.close();
        return false;
    }
    f.read(buf, size);
    f.close();

    memcpy(&dsc->header, buf, sizeof(lv_img_header_t));
    dsc->data_size = size - sizeof(lv_img_header_t);
    dsc->data      = buf + sizeof(lv_img_header_t);
    Serial.printf("loadBinToImgDsc: %s loaded %ux%u (%u bytes)\n",
                  sdPath, dsc->header.w, dsc->header.h, size);
    return true;
}

// --- Config Globals ---
String config_ssid = "";   // Set at runtime by RS485 "update" command
String config_pass = "";   // Set at runtime by RS485 "update" command
String config_otaURL = ""; // Set at runtime by RS485 "update" command

/* =================================================================================
 *                                  CONSTANTS
 * =================================================================================*/
constexpr long BAUD_RATE_DEBUG = 115200;
constexpr long BAUD_RATE_RS485 = 38400;
constexpr long TIMEOUT_BACKLIGHT = SCREEN_TIMEOUT * 1000;
constexpr long INTERVAL_DEBUG = 1000;
constexpr long INTERVAL_LONG_LOG = 5000;

/* =================================================================================
 *                               GLOBAL VARIABLES
 * =================================================================================*/

// Hardware Objects
HardwareSerial RS485Serial(1);

// Data Structures
TYPE_DISPLAY_DATA display;
TYPE_BTN_PRESSED tButton; // Currently selected button index

// State Flags
bool IS_STATE_MACHINE_ACTIVE = false;
bool BACKLIGHT_ON = true;
bool BackLightMessage = true;
bool CONFIG_RECEIVED = false;
extern bool bootAnswerPending;
bool SETPOINT_CHANGED = false;
bool SETPOINT_UPDATE = false;
bool IS_ALWAYS_ON = false; // Prevents blank screen
bool HANDLE_BUTTON = true; // Trigger to process button press

// Initialization Flags
bool FIRST_SCREEN[10];
bool FIRST_BTN[NUM_BUTTONS];
int valueChanged[NUM_BUTTONS];

// Timers & Counters
uint64_t total_sd_space;
uint64_t max_sd_cap;
int counter = 0;
int arcSetpoint = 0;
long screenTimeoutTimestamp;
long lastDebugTime = 0;
long lastScreenLogTime = 0;
int16_t DeviceID;

// String Buffers
String homeBtnLabel[NUM_BUTTONS];
String phomeBtnLabel[NUM_BUTTONS];

float newSetPoint[MAX_LABELS] = {0};
long azzera_counter;
unsigned long arcLastTouchTime = 0;
unsigned long switchLastTouchTime = 0;

// Page configuration storage
String pageLabels[MAX_PAGES][MAX_LABELS];
String pageTemperatures[MAX_PAGES][NUM_SENSORS];
float  pageSetPoints[MAX_PAGES][NUM_SENSORS];
String pageRegulatorLabels[MAX_PAGES][NUM_SENSORS];
int    pageModes[MAX_PAGES][NUM_SENSORS];
int    pageMins[MAX_PAGES][NUM_SENSORS];
int    pageMaxs[MAX_PAGES][NUM_SENSORS];
float  pageNewSetPoints[MAX_PAGES][MAX_LABELS];
int    pageValueChanged[MAX_PAGES][MAX_LABELS];
int    pageBtnEn[MAX_PAGES][NUM_SENSORS]; // btnEn state per page, default 0
bool   pageAvailable[MAX_PAGES];
int    activePage = 0;
bool   SELECT_PREV_PAGE = false;
bool   SELECT_NEXT_PAGE = false;

/*  flags*/

int BACKLIGHT_ALWAYS_ON = 0;
bool BTN_ALWAYS_ON = false;
bool BTN_PULSANTE_CONTROLLO_REMOTO = false;
bool NOT_CONTROLLABLE = false;
bool PERFORM_OTA_UPDATE = false;
bool TRIGGER_RESTART = false;
bool TRIGGER_COMUNICATION = false;

/* =================================================================================
 *                               FORWARD DECLARATIONS
 * =================================================================================*/
// UI Events (C Linkage)
extern "C" void ta_value_changed(lv_event_t *e);
extern "C" void check_code(lv_event_t *e);

// --- ADD THIS LINE ---
static void hide_indicator_timer_cb(lv_timer_t *timer); // Forward declaration for LVGL timer callback
static void updatePageNavigationButtonsState();
static void saveActivePageState();
static int getAvailablePageCount();
static int getActivePageDisplayNumber();
static void updatePageIndicatorLabel();

/* =================================================================================
 *                                HELPER FUNCTIONS
 * =================================================================================*/

// --- Global Variable for the Blinker ---
// Define the radius and size
#define INDICATOR_RADIUS 10
#define INDICATOR_SIZE (INDICATOR_RADIUS * 2)

lv_obj_t *indicator_obj = NULL;
/**
 * @brief Creates a 20x20 green circle, displays it, and schedules its deletion after 30 seconds.
 * @param parent_screen The screen object (e.g., ui_Screen1) to place the indicator on.
 */
void show_green_indicator_for_30s(lv_obj_t *parent_screen)
{
  // Check if an indicator already exists to prevent duplicates
  if (indicator_obj != NULL)
  {
    Serial.println("Warning: Indicator already active.");
    return;
  }

  if (!lvgl_port_lock(-1))
    return;

  // --- 1. Create and Style the Object ---
  indicator_obj = lv_obj_create(parent_screen);

  // Set position (top-left corner, adjust margin as needed)
  lv_obj_set_pos(indicator_obj, 10, 5);
  lv_obj_set_size(indicator_obj, INDICATOR_SIZE, INDICATOR_SIZE);

  // Make it a perfect circle
  lv_obj_set_style_radius(indicator_obj, INDICATOR_RADIUS, LV_PART_MAIN);

  // Remove default styling (borders, padding)
  lv_obj_clear_flag(indicator_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(indicator_obj, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(indicator_obj, 0, LV_PART_MAIN);

  // Set the color to GREEN (0x00FF00)
  lv_obj_set_style_bg_color(indicator_obj, lv_color_hex(0x00AA00), LV_PART_MAIN);

  Serial.println(">> Green indicator created.");

  // --- 2. Schedule the Hiding Timer ---
  // Create a one-shot timer that calls hide_indicator_timer_cb after 30,000 milliseconds (30 seconds)
  lv_timer_create(hide_indicator_timer_cb, 30000, NULL);

  lvgl_port_unlock();
}

/**
 * @brief LVGL Timer callback function executed after 30 seconds.
 * It hides and cleans up the indicator object.
 * @param timer Pointer to the LVGL timer object.
 */
static void hide_indicator_timer_cb(lv_timer_t *timer)
{
  if (!lvgl_port_lock(-1))
    return;

  if (indicator_obj != NULL)
  {
    // 1. Delete the object from memory
    lv_obj_del(indicator_obj);
    indicator_obj = NULL;

    Serial.println(">> Green indicator hidden and deleted.");
  }

  // 2. Delete the timer itself so it doesn't run again
  lv_timer_del(timer);

  lvgl_port_unlock();
}

// Define the filename
const char *CONFIG_FILE = "/config.json";
static const char *BTNEN_FILE = "/btnEn.json";

static void saveBtnEnToSD()
{
    if (SD_MMC.exists(BTNEN_FILE))
        SD_MMC.remove(BTNEN_FILE);

    File f = SD_MMC.open(BTNEN_FILE, FILE_WRITE);
    if (!f) { Serial.println("btnEn: cannot open for write"); return; }

    JsonDocument doc;
    JsonArray pages = doc["btnEn"].to<JsonArray>();
    for (int p = 0; p < MAX_PAGES; p++)
    {
        JsonArray row = pages.add<JsonArray>();
        for (int i = 0; i < NUM_SENSORS; i++)
            row.add(pageBtnEn[p][i]);
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("btnEn saved to SD");
}

static void loadBtnEnFromSD()
{
    // Default: all zero (already zero-initialised as a global)
    if (!SD_MMC.exists(BTNEN_FILE)) return;

    File f = SD_MMC.open(BTNEN_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.println("btnEn: parse error, using defaults"); return; }

    JsonArrayConst pages = doc["btnEn"].as<JsonArrayConst>();
    int p = 0;
    for (JsonArrayConst row : pages)
    {
        if (p >= MAX_PAGES) break;
        int i = 0;
        for (int v : row)
        {
            if (i >= NUM_SENSORS) break;
            pageBtnEn[p][i] = (v != 0) ? 1 : 0;
            i++;
        }
        p++;
    }
    Serial.println("btnEn loaded from SD");
}

// Function to save the backlight value
bool saveBacklightValue(int newBacklightValue)
{
  // 1. Check if SD card is ready (optional, depends on your setup)
  // if (!SD.begin()) return false;

  // 2. Open the file for reading to get the current content
  File file = SD_MMC.open(CONFIG_FILE, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open config file for reading");
    return false;
  }

  // 3. Allocate a buffer for the JSON document
  // Adjust size if your config grows. 1024 is plenty for your current structure.
  JsonDocument doc; // For ArduinoJson v7. Use DynamicJsonDocument doc(1024); for v6

  // 4. Parse the file into the JSON document
  DeserializationError error = deserializeJson(doc, file);
  file.close(); // Close immediately after reading

  if (error)
  {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    return false;
  }

  // 5. Update the specific field
  doc["backLight"] = newBacklightValue;

  // 6. Delete the old file so we can write a fresh one
  // (Standard SD library appends by default, so we must remove first to overwrite)
  if (SD_MMC.exists(CONFIG_FILE))
  {
    SD_MMC.remove(CONFIG_FILE);
  }

  // 7. Create/Open the file for writing
  File fileWrite = SD_MMC.open(CONFIG_FILE, FILE_WRITE);
  if (!fileWrite)
  {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  // 8. Serialize the updated JSON data to the file
  if (serializeJson(doc, fileWrite) == 0)
  {
    Serial.println("Failed to write to file");
    fileWrite.close();
    return false;
  }

  // 9. Close the file and finish
  fileWrite.close();
  Serial.println("Backlight value saved successfully");
  return true;
}

/**
 * @brief Copies the labels of a stored page config into display.btnLabel
 *        and forces a full UI refresh by resetting the "old labels" cache.
 */
void applyPageConfig(int page)
{
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    if (i < MAX_LABELS)
    {
      display.btnLabel[i] = pageLabels[page][i];
      phomeBtnLabel[i]    = "";
      newSetPoint[i]      = pageNewSetPoints[page][i];
      valueChanged[i]     = pageValueChanged[page][i];
    }

    display.temperature[i]    = pageTemperatures[page][i];
    display.setPoint[i]       = pageSetPoints[page][i];
    display.regulatorLabel[i] = pageRegulatorLabels[page][i];
    display.mode[i]           = pageModes[page][i];
    display.min[i]            = pageMins[page][i];
    display.max[i]            = pageMaxs[page][i];
    display.btnEn[i]          = pageBtnEn[page][i];
  }
}

static void saveActivePageState()
{
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    if (i < MAX_LABELS)
    {
      pageLabels[activePage][i]        = display.btnLabel[i];
      pageNewSetPoints[activePage][i]  = newSetPoint[i];
      pageValueChanged[activePage][i]  = valueChanged[i];
    }

    pageTemperatures[activePage][i]    = display.temperature[i];
    pageSetPoints[activePage][i]       = display.setPoint[i];
    pageRegulatorLabels[activePage][i] = display.regulatorLabel[i];
    pageModes[activePage][i]           = display.mode[i];
    pageMins[activePage][i]            = display.min[i];
    pageMaxs[activePage][i]            = display.max[i];
    pageBtnEn[activePage][i]           = display.btnEn[i];
  }
}

static int getAvailablePageCount()
{
  int count = 0;
  for (int i = 0; i < MAX_PAGES; i++)
  {
    if (pageAvailable[i])
      count++;
  }
  return count;
}

static int getActivePageDisplayNumber()
{
  int displayNumber = 0;
  for (int i = 0; i <= activePage && i < MAX_PAGES; i++)
  {
    if (pageAvailable[i])
      displayNumber++;
  }
  return displayNumber;
}

static void updatePageIndicatorLabel()
{
  int totalPages = getAvailablePageCount();
  int currentPage = getActivePageDisplayNumber();

  if (totalPages == 0)
  {
    lv_label_set_text(ui_labelPageNum, "0/0");
    return;
  }

  if (currentPage == 0)
    currentPage = 1;

  lv_label_set_text_fmt(ui_labelPageNum, "%d/%d", currentPage, totalPages);
}

static void updatePageNavigationButtonsState()
{
  if (ui_btnPrevPage == NULL || ui_btnNextPage == NULL)
    return;

  int prevPage = activePage - 1;
  int nextPage = (activePage + 1) % MAX_PAGES;
  bool canGoToPrevPage = prevPage >= 0 && pageAvailable[prevPage];
  bool canGoToNextPage = pageAvailable[nextPage];

  if (canGoToPrevPage)
  {
    lv_obj_clear_flag(ui_btnPrevPage, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_add_flag(ui_btnPrevPage, LV_OBJ_FLAG_HIDDEN);
  }

  if (canGoToNextPage)
  {
    lv_obj_clear_flag(ui_btnNextPage, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_add_flag(ui_btnNextPage, LV_OBJ_FLAG_HIDDEN);
  }
}

void initializeDataStructures()
{
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    valueChanged[i] = 0;
    FIRST_BTN[i] = true;
    homeBtnLabel[i] = "topn"; // Default label
  }

  for (int i = 0; i < 10; i++)
  {
    FIRST_SCREEN[i] = true;
  }

  for (int i = 0; i < MAX_PAGES; i++)
  {
    pageAvailable[i] = false;
    for (int j = 0; j < NUM_SENSORS; j++)
    {
      if (j < MAX_LABELS)
      {
        pageLabels[i][j] = "";
        pageNewSetPoints[i][j] = 0;
        pageValueChanged[i][j] = 0;
      }

      pageTemperatures[i][j] = "";
      pageSetPoints[i][j] = 0;
      pageRegulatorLabels[i][j] = "";
      pageModes[i][j] = 0;
      pageMins[i][j] = 0;
      pageMaxs[i][j] = 0;
    }
  }

  // Initialize Display Data
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    display.temperature[i] = "";
    display.setPoint[i] = i;         // Dummy data
    display.regulatorLabel[i] = "schermo:" + String(i);
  }
}

bool isButtonChanged(TYPE_BTN_PRESSED button)
{
  static TYPE_BTN_PRESSED prevButton = STATO_BTN_NONE;
  static int prevPage = -1;
  if (button != prevButton || activePage != prevPage)
  {
    prevButton = button;
    prevPage = activePage;
    Serial.println(">> Button Changed");
    return true;
  }
  return false;
}

bool isScreenChanged(lv_obj_t *screen)
{
  static lv_obj_t *prevScreen = nullptr;
  if (screen != prevScreen)
  {
    prevScreen = screen;
    return true;
  }
  return false;
}

void loadConfigFromSD()
{
  File jsonfile = SD_MMC.open("/config.json");
  if (!jsonfile)
  {
    Serial.println("Failed to open config.json");
    return;
  }

  JsonDocument doc;
  // Increase memory limit if JSON grows, but basic doc is fine for now
  DeserializationError error = deserializeJson(doc, jsonfile);

  if (!error)
  {
    String name = doc["displayName"];
    int id = doc["displayId"];
    BACKLIGHT_ALWAYS_ON = doc["backLight"];
    DeviceID = id;
    display.display_id = id;
    display.display_name = name;

    Serial.println("Config Loaded.");
    Serial.print("Display ID: ");
    Serial.println(DeviceID);
    Serial.print("Display Name: ");
    Serial.println(display.display_name);
  }
  else
  {
    Serial.print("JSON Error: ");
    Serial.println(error.c_str());
  }
  jsonfile.close();
}

void performOTAUpdate()
{
  if (config_ssid == "" || config_otaURL == "")
  {
    Serial.println(">> OTA Skipped: Missing WiFi or URL config");
    return;
  }

  String fwURL = config_otaURL;

  Serial.println(">> OTA: Initializing...");

  // 1. Update UI to show status (Optional but recommended)
  if (lvgl_port_lock(-1))
  {
    // Create a temporary label on the active screen
    lv_obj_t *mbox1 = lv_msgbox_create(NULL, "System Update", "Connecting to WiFi...", NULL, true);
    lv_obj_center(mbox1);
    lv_task_handler(); // Force UI update
    lvgl_port_unlock();
  }

  // 2. Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(config_ssid.c_str(), config_pass.c_str());

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    timeout++;
    if (timeout > 20)
    {
      Serial.println("\n>> OTA Error: WiFi Connection Failed");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return;
    }
  }

  Serial.println("\n>> WiFi Connected. Starting Update...");

  // 3. Update UI to show downloading
  if (lvgl_port_lock(-1))
  {
    lv_obj_t *mbox2 = lv_msgbox_create(NULL, "System Update", "Downloading Firmware...\nDo not turn off.", NULL, true);
    lv_obj_center(mbox2);
    lv_task_handler();
    lvgl_port_unlock();
  }

  // 4. Execute HTTP Update
  WiFiClient client;
  httpUpdate.setLedPin(-1);

  Serial.print(">> OTA URL: ");
  Serial.println(fwURL);

  // This function will Reboot the ESP32 automatically on success
  t_httpUpdate_return ret = httpUpdate.update(client, fwURL);

  // 5. Handle Errors (If we are still here, it failed)
  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }

  // Disconnect and disable WiFi after a failed update
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
/**
 * Bind the LVGL Keyboard to ui_PasswText and attach events
 * (Currently manual binding logic goes here)
 */
static void force_bind_keyboard_and_events(void)
{
  // 1) Make sure both objects are present on the current screen
  // 2) Attach handlers directly
}

/* =================================================================================
 *                                  CORE LOGIC
 * =================================================================================*/

void handleButtonPress()
{
  if (!HANDLE_BUTTON)
    return;

  HANDLE_BUTTON = false;

  // Determine action based on the mode of the pressed button
  switch (display.mode[tButton])
  {
  case 0: // Setting Mode
    _ui_screen_change(&ui_ScreenSetting, LV_SCR_LOAD_ANIM_OVER_LEFT, 200, 0, &ui_ScreenSetting_screen_init);
    lv_arc_set_range(ui_Arc1, display.min[tButton] * 10, display.max[tButton] * 10);
    {
      float sp = newSetPoint[tButton] > 0 ? newSetPoint[tButton] : display.setPoint[tButton];
      lv_arc_set_value(ui_Arc1, (int)(sp * 10));
      lv_label_set_text_fmt(ui_debugLabel, "%d", tButton);
      String setPointStr = String(sp) + "°C";
      lv_label_set_text(ui_setPointNum, setPointStr.c_str());
    }
    break;

  case 1: // View Mode — no page change
    break;

  case 10: // Disabled — do nothing
    break;

  case 2: // Boolean toggle on home screen — no page change
  {
    int newEn = (display.btnEn[tButton] == 0) ? 1 : 0;
    display.btnEn[tButton]          = newEn;
    pageBtnEn[activePage][tButton]  = newEn;
    display.setPoint[tButton]       = newEn ? display.max[tButton] : display.min[tButton];
    newSetPoint[tButton]            = display.setPoint[tButton];
    valueChanged[tButton] = 1;
    SETPOINT_CHANGED = true;
    UpdateBtnLeds(display);
    saveBtnEnToSD();
    break;
  }

  case 3:
    _ui_screen_change(&ui_ScreenBoolControl, LV_SCR_LOAD_ANIM_OVER_LEFT, 200, 0, &ui_ScreenBoolControl_screen_init);
    NOT_CONTROLLABLE = true;
    if (display.setPoint[tButton] >= display.max[tButton])
    {
      lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
      lv_label_set_text(ui_labelONOFF, "ON");
    }
    else
    {
      lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
      lv_label_set_text(ui_labelONOFF, "OFF");
    }
    break;

  default:
    break;
  }
}

void manageBacklight(lv_obj_t *active_screen)
{
  // If screen changed, reset timer
  if (isScreenChanged(active_screen))
  {
    screenTimeoutTimestamp = millis();
    // Reset "First Screen" flags logic if needed
    for (int i = 0; i < 10; i++)
      FIRST_SCREEN[i] = true;
  }

  // Check Timeout
  if (!IS_ALWAYS_ON && (millis() - screenTimeoutTimestamp > TIMEOUT_BACKLIGHT))
  {
    if (BACKLIGHT_ON)
    {
      Serial.println(">> Screen Timeout: Turning Backlight OFF");
      wavesahre_rgb_lcd_bl_off();
      BACKLIGHT_ON = false;

      // Switch to blank screen
      _ui_screen_change(&ui_BlackScreen, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, &ui_BlackScreen_screen_init);

      screenTimeoutTimestamp = millis(); // Reset to prevent rapid firing
    }
  }
}

/**
 * @brief Updates the UI widgets on the Settings Screen for the selected zone.
 *
 * =============================================================================
 * FUNCTIONALITY
 * =============================================================================
 * This function refreshes the graphical elements of the settings page based on
 * the global 'display' structure data for the specific index provided.
 *
 * 1. DATA RETRIEVAL
 *    - Accesses global arrays: display.setPoint[], display.temperature[],
 *      display.min[], display.max[], and display.mode[].
 *
 * 2. WIDGET UPDATES
 *    - Title/Header: Updates the screen title with the zone name (regulatorLabel).
 *    - Slider/Arc:
 *      - Sets the widget value to the current 'setPoint'.
 *      - Dynamically adjusts the slider range using 'min' and 'max' values.
 *    - Labels: Updates the text showing current room temperature.
 *    - Mode Indicators: Updates icons or text to reflect the current HVAC mode
 *      (e.g., Heating, Cooling, Off).
 *
 * 3. LOGIC
 *    - Should be called when navigating to the screen or when new RS485 data
 *      arrives while the screen is active.
 *    - Ensures UI values stay synchronized with backend state.
 *
 * @param index  The index (0-11) of the zone/regulator currently selected.
 */
void updateSettingsScreen()
{
  // Initialize screen on first load
  if (FIRST_SCREEN[2])
  {
    Serial.println(">> Active: Settings Screen");
    screenTimeoutTimestamp = millis();
    FIRST_SCREEN[2] = false;
  }

  // Update logic if button or page changed
  if (isButtonChanged(tButton))
  {
    IS_STATE_MACHINE_ACTIVE = true;

    // Reset button 'first' flags
    for (int i = 0; i < NUM_BUTTONS; i++)
      FIRST_BTN[i] = true;

    // Clear arc touch guard so label/arc refresh immediately for the new button
    arcLastTouchTime = 0;

    // Update labels
    lv_label_set_text(ui_regLabel, display.regulatorLabel[tButton].c_str());
    lv_label_set_text(ui_labelTempInPage, display.btnLabel[tButton].c_str());

    // Update arc range and value for the new button/page
    lv_arc_set_range(ui_Arc1, display.min[tButton] * 10, display.max[tButton] * 10);
    float sp = newSetPoint[tButton] > 0 ? newSetPoint[tButton] : display.setPoint[tButton];
    lv_arc_set_value(ui_Arc1, (int)(sp * 10));
    String setPointStr = String(sp) + "°C";
    lv_label_set_text(ui_setPointNum, setPointStr.c_str());
  }

  // Continuous updates
  if (display.temperature[tButton].length() > 0)
  {
    String tempStr = display.temperature[tButton] + " C";
    lv_label_set_text(ui_tempInPage, tempStr.c_str());
  }

  // Refresh arc and setpoint label from incoming data, unless user recently touched the slider
  if (millis() - arcLastTouchTime > 30000)
  {
    float sp = newSetPoint[tButton] > 0 ? newSetPoint[tButton] : display.setPoint[tButton];
    lv_arc_set_value(ui_Arc1, (int)(sp * 10));
    String setPointStr = String(sp) + "°C";
    lv_label_set_text(ui_setPointNum, setPointStr.c_str());
  }
  /*
    // Parse Setpoint from Label
    const char *labelText = lv_label_get_text(ui_setPointNum);
    display.setPoint[tButton] = atof(labelText); // Simple conversion */

  // Debug output
  if (DEBUG_ON && (millis() - lastDebugTime > INTERVAL_DEBUG))
  {
    lastDebugTime = millis();
    Serial.print("Extracted Setpoint: ");
    Serial.println(display.setPoint[tButton]);
  }

  // Reset State Machine flag if active
  if (IS_STATE_MACHINE_ACTIVE)
  {
    IS_STATE_MACHINE_ACTIVE = false;
    // Add specific state logic here if needed (e.g., reset specific flags based on tButton)
  }
}

void updateActiveScreenLogic(lv_obj_t *active_screen)
{

  if (active_screen == ui_ScreenConnection)
  {
    if (FIRST_SCREEN[3])
    {
      FIRST_SCREEN[3] = false;
      if (wait_img_dsc.data != nullptr)
      {
        lv_img_set_src(ui_waitBg, &wait_img_dsc);
        lv_obj_clear_flag(ui_waitBg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui_waitBg);
        lv_obj_move_background(ui_waitBg);
      }
      lv_label_set_text(ui_connectionLabel, String(DeviceID).c_str());
    }
    if (CONFIG_RECEIVED)
    {
      _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, &ui_Screen1_screen_init);
    }
  }
  else if (active_screen == ui_Screen1)
  {
    if (FIRST_SCREEN[0])
    {
      Serial.println(">> Active: Main Screen");
      FIRST_SCREEN[0] = false;
      bootAnswerPending = false;
      screenTimeoutTimestamp = millis();

      if (home_img_dsc.data != nullptr)
      {
        lv_img_set_src(ui_homeBg, &home_img_dsc);
        lv_obj_clear_flag(ui_homeBg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui_homeBg);
        lv_obj_move_background(ui_homeBg);
      }
    }

    String homeIdStr = "id:" + String(DeviceID);
    lv_label_set_text(ui_Label2, homeIdStr.c_str());
    updatePageIndicatorLabel();
    updatePageNavigationButtonsState();
    UpdateBtnLabels(display, phomeBtnLabel);
    UpdateBtnTemperatures(display);
    UpdateBtnLeds(display);

    // Restore Backlight if returning to this screen
    if (!BACKLIGHT_ON)
    {
      BACKLIGHT_ON = true;
      BackLightMessage = true;
    }
    if (BackLightMessage && BACKLIGHT_ON)
    {
      wavesahre_rgb_lcd_bl_on();
      Serial.println(">> Backlight ON");
      BackLightMessage = false;
    }
  }
  else if (active_screen == ui_ScreenBoolControl)
  {
    lv_label_set_text(ui_labelBoolControl, display.btnLabel[tButton].c_str());
    static long timer_Switch1 = 0;
    if (NOT_CONTROLLABLE)
    {
      if (millis() - timer_Switch1 > 5000)
      {
        if (display.setPoint[tButton] >= display.max[tButton])
        {
          lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
          lv_label_set_text(ui_labelONOFF, "ON");
        }
        else
        {
          lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
          lv_label_set_text(ui_labelONOFF, "OFF");
        }
      }
    }
    else
    {
      if (millis() - switchLastTouchTime > 30000)
      {
        if (display.setPoint[tButton] >= display.max[tButton])
        {
          lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
          lv_label_set_text(ui_labelONOFF, "ON");
        }
        else
        {
          lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
          lv_label_set_text(ui_labelONOFF, "OFF");
        }
      }
    }
  }
  else if (active_screen == ui_ScreenView)
  {
    lv_label_set_text(ui_labelViewTemp, display.btnLabel[tButton].c_str());
    if (display.temperature[tButton].length() > 0)
    {
      String temp = display.temperature[tButton] + " C";
      lv_label_set_text(ui_viewTempValue, temp.c_str());
    }
  }
  else if (active_screen == ui_ScreenInfo)
  {
    if (FIRST_SCREEN[1])
    {
      Serial.println(">> Active: Info Screen");
      screenTimeoutTimestamp = millis();
      FIRST_SCREEN[1] = false;

      if (info_img_dsc.data != nullptr)
      {
        lv_img_set_src(ui_infoBg, &info_img_dsc);
        lv_obj_clear_flag(ui_infoBg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui_infoBg);
        lv_obj_move_background(ui_infoBg);
      }
    }
    String idStr = "id:" + String(display.display_id);
    lv_label_set_text(ui_screenId, idStr.c_str());
  }
  else if (active_screen == ui_ScreenSetting)
  {
    if (FIRST_SCREEN[2])
    {
      FIRST_SCREEN[2] = false;
      if (setting_img_dsc.data != nullptr)
      {
        lv_img_set_src(ui_settingBg, &setting_img_dsc);
        lv_obj_clear_flag(ui_settingBg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui_settingBg);
        lv_obj_move_background(ui_settingBg);
      }
    }
    updateSettingsScreen();
  }
  else if (active_screen == ui_BlackScreen)
  { // Renamed from ui_BlackScreen for consistency
    // Logic handled in transition or timeout, usually empty loop here
  }
}

/* =================================================================================
 *                                  ARDUINO SETUP
 * =================================================================================*/

void setup()
{
  // 1. Init Serial Communication
  Serial.begin(BAUD_RATE_DEBUG);

  // Keep WiFi off until an OTA update command arrives over RS485
  WiFi.mode(WIFI_OFF);
  pinMode(RS485_RX_PIN, INPUT);
  RS485Serial.begin(BAUD_RATE_RS485, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  // 2. Start Background Tasks
  xTaskCreatePinnedToCore(
      TaskSerialPoll, "TaskSerialPoll",
      4096, nullptr, 1, nullptr, 1);

  // 3. Init Hardware
  sd_mmc_init();
  loadBtnEnFromSD();
  loadBinToImgDsc("/home.bin", &home_img_dsc);
  loadBinToImgDsc("/info.bin", &info_img_dsc);
  loadBinToImgDsc("/setting.bin", &setting_img_dsc);
  loadBinToImgDsc("/wait.bin", &wait_img_dsc);

  // 4. Init Display & Touch
  static esp_lcd_panel_handle_t panel_handle = NULL;
  static esp_lcd_touch_handle_t tp_handle = NULL;
  tp_handle = touch_gt911_init();
  panel_handle = waveshare_esp32_s3_rgb_lcd_init();

  // 5. Init LVGL
  wavesahre_rgb_lcd_bl_on();
  ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));
  lvgl_sd_fs_init();  // Must be after lvgl_port_init() which calls lv_init()
  ESP_LOGI(TAG, "Display LVGL initialized");

  // 6. Build UI (Thread Safe)
  if (lvgl_port_lock(-1))
  {
    ui_init();
    force_bind_keyboard_and_events();
    lvgl_port_unlock();
  }

  // 7. Initialize Logic & Data
  initializeDataStructures();
  loadConfigFromSD();
  UpdateBtnLabels(display, phomeBtnLabel);



  screenTimeoutTimestamp = millis();
  HANDLE_BUTTON = false;

  // 8. Load Initial Screen
  if (lvgl_port_lock(-1))
  {
    _ui_screen_change(&ui_ScreenConnection, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, &ui_ScreenConnection_screen_init);
    if (BACKLIGHT_ALWAYS_ON)
    {
      lv_obj_add_state(ui_alwaysOn, LV_STATE_CHECKED);
    }
    lvgl_port_unlock();
  }
}

/* =================================================================================
 *                                  ARDUINO LOOP
 * =================================================================================*/

void loop()
{

  if (PERFORM_OTA_UPDATE)
  {
    performOTAUpdate();
    PERFORM_OTA_UPDATE = 0;
  }
  if (BTN_ALWAYS_ON)
  {
    saveBacklightValue(lv_obj_has_state(ui_alwaysOn, LV_STATE_CHECKED));
    BTN_ALWAYS_ON = false;
  }
  if (TRIGGER_RESTART)
  {
    TRIGGER_RESTART = false;
    ESP.restart();
  }

  // 1. Process User Input
  handleButtonPress();

  // Cycle through available page configurations when "next" is pressed
  if (SELECT_PREV_PAGE)
  {
    SELECT_PREV_PAGE = false;

    int prev = activePage - 1;
    if (prev >= 0 && pageAvailable[prev])
    {
      saveActivePageState();
      activePage = prev;
      applyPageConfig(activePage);
      Serial.printf(">> Page config switched to %d\n", activePage + 1);
      if (lvgl_port_lock(-1))
      {
        updatePageIndicatorLabel();
        updatePageNavigationButtonsState();
        lvgl_port_unlock();
      }
    }
    else
    {
      Serial.printf(">> Page %d not available yet, keeping page %d\n",
                    prev + 1, activePage + 1);
    }
  }

  if (SELECT_NEXT_PAGE)
  {
    SELECT_NEXT_PAGE = false;

    int next = (activePage + 1) % MAX_PAGES;
    if (pageAvailable[next])
    {
      saveActivePageState();
      activePage = next;
      applyPageConfig(activePage);
      Serial.printf(">> Page config switched to %d\n", activePage + 1);
      if (lvgl_port_lock(-1))
      {
        updatePageIndicatorLabel();
        updatePageNavigationButtonsState();
        lvgl_port_unlock();
      }
    }
    else
    {
      Serial.printf(">> Page %d not available yet, keeping page %d\n",
                    next + 1, activePage + 1);
    }
  }

  // 2. Periodic Logging
  if (millis() - lastScreenLogTime > INTERVAL_LONG_LOG)
  {
    Serial.print(">> Heartbeat | Arc Setpoint: ");
    Serial.println(arcSetpoint);

    // Check "Always On" toggle
    IS_ALWAYS_ON = lv_obj_has_state(ui_alwaysOn, LV_STATE_CHECKED);

    lastScreenLogTime = millis();
  }

  // 3. System Delay (Yield)
  delay(100);

  // 4. Update Screen Logic & Power Management
  if (lvgl_port_lock(-1))
  {
    lv_obj_t *active_screen = lv_scr_act();

    if (TRIGGER_COMUNICATION)
    {
      show_green_indicator_for_30s(active_screen);
      TRIGGER_COMUNICATION = false;
    }
    manageBacklight(active_screen);
    updateActiveScreenLogic(active_screen);
    lvgl_port_unlock();
  }
}
