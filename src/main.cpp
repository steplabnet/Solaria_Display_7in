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

// Firmware version — used by the PlatformIO post-build script to name the
// artifact copied into dist/ (display7_<VERSION>.bin). Bump on each release.

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
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

#define VERSION "260616h"

// Log tag for ESP_LOGx macros (compiled in once CORE_DEBUG_LEVEL > 0).
static const char *TAG = "main";

// --- Background Image Descriptors (loaded into PSRAM at startup) ---
static lv_img_dsc_t home_img_dsc;
static lv_img_dsc_t info_img_dsc;
static lv_img_dsc_t setting_img_dsc;
static lv_img_dsc_t wait_img_dsc;

static bool loadBinToImgDsc(const char *sdPath, lv_img_dsc_t *dsc)
{
  File f = SD_MMC.open(sdPath, FILE_READ);
  if (!f)
  {
    Serial.printf("loadBinToImgDsc: cannot open %s\n", sdPath);
    return false;
  }
  size_t size = f.size();
  uint8_t *buf = (uint8_t *)ps_malloc(size);
  if (!buf)
  {
    Serial.printf("loadBinToImgDsc: ps_malloc failed (%u bytes)\n", size);
    f.close();
    return false;
  }
  f.read(buf, size);
  f.close();

  memcpy(&dsc->header, buf, sizeof(lv_img_header_t));
  dsc->data_size = size - sizeof(lv_img_header_t);
  dsc->data = buf + sizeof(lv_img_header_t);
  Serial.printf("loadBinToImgDsc: %s loaded %ux%u (%u bytes)\n",
                sdPath, dsc->header.w, dsc->header.h, size);
  return true;
}

// --- OTA crash breadcrumb (survives brownout/panic/watchdog resets) ---------
// Stored in RTC memory so that, even when a fault drops the USB-CDC serial and
// the live log is lost, the NEXT boot can report exactly how far OTA got.
RTC_NOINIT_ATTR uint32_t otaBcMagic;
RTC_NOINIT_ATTR uint32_t otaBcStage;
#define OTA_BC_MAGIC 0xB0071234u
// Stage values: 0=idle 1=before WiFi.mode 2=after WiFi.mode 3=in httpUpdate

// --- Config Globals ---
String config_ssid = "";   // Set at runtime by RS485 "update" command
String config_pass = "";   // Set at runtime by RS485 "update" command
String config_otaURL = ""; // Set at runtime by RS485 "update" command

// --- "uploadFile" command config (download a remote file onto the SD card) ---
String config_fileURL = "";  // Full source URL, set by RS485 "uploadFile"
String config_filePath = ""; // SD destination path (e.g. "/logo.bin")

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
String pageLabelsOff[MAX_PAGES][MAX_LABELS]; // OFF-state labels for mode-2 buttons
String pageTemperatures[MAX_PAGES][NUM_SENSORS];
float pageSetPoints[MAX_PAGES][NUM_SENSORS];
String pageRegulatorLabels[MAX_PAGES][NUM_SENSORS];
int pageModes[MAX_PAGES][NUM_SENSORS];
int pageMins[MAX_PAGES][NUM_SENSORS];
int pageMaxs[MAX_PAGES][NUM_SENSORS];
float pageNewSetPoints[MAX_PAGES][MAX_LABELS];
int pageValueChanged[MAX_PAGES][MAX_LABELS];
int pageBtnEn[MAX_PAGES][NUM_SENSORS]; // btnEn state per page, default 0
bool pageAvailable[MAX_PAGES];
int activePage = 0;
bool SELECT_PREV_PAGE = false;
bool SELECT_NEXT_PAGE = false;

/*  flags*/

int BACKLIGHT_ALWAYS_ON = 0;
bool BTN_ALWAYS_ON = false;
bool BTN_PULSANTE_CONTROLLO_REMOTO = false;
bool NOT_CONTROLLABLE = false;
bool PERFORM_OTA_UPDATE = false;
bool PERFORM_FILE_UPLOAD = false;
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
  if (!f)
  {
    Serial.println("btnEn: cannot open for write");
    return;
  }

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
  if (!SD_MMC.exists(BTNEN_FILE))
    return;

  File f = SD_MMC.open(BTNEN_FILE, FILE_READ);
  if (!f)
    return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err)
  {
    Serial.println("btnEn: parse error, using defaults");
    return;
  }

  JsonArrayConst pages = doc["btnEn"].as<JsonArrayConst>();
  int p = 0;
  for (JsonArrayConst row : pages)
  {
    if (p >= MAX_PAGES)
      break;
    int i = 0;
    for (int v : row)
    {
      if (i >= NUM_SENSORS)
        break;
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
      display.btnLabelOff[i] = pageLabelsOff[page][i];
      phomeBtnLabel[i] = "";
      newSetPoint[i] = pageNewSetPoints[page][i];
      valueChanged[i] = pageValueChanged[page][i];
    }

    display.temperature[i] = pageTemperatures[page][i];
    display.setPoint[i] = pageSetPoints[page][i];
    display.regulatorLabel[i] = pageRegulatorLabels[page][i];
    display.mode[i] = pageModes[page][i];
    display.min[i] = pageMins[page][i];
    display.max[i] = pageMaxs[page][i];
    display.btnEn[i] = pageBtnEn[page][i];
  }
}

static void saveActivePageState()
{
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    if (i < MAX_LABELS)
    {
      pageLabels[activePage][i] = display.btnLabel[i];
      pageLabelsOff[activePage][i] = display.btnLabelOff[i];
      pageNewSetPoints[activePage][i] = newSetPoint[i];
      pageValueChanged[activePage][i] = valueChanged[i];
    }

    pageTemperatures[activePage][i] = display.temperature[i];
    pageSetPoints[activePage][i] = display.setPoint[i];
    pageRegulatorLabels[activePage][i] = display.regulatorLabel[i];
    pageModes[activePage][i] = display.mode[i];
    pageMins[activePage][i] = display.min[i];
    pageMaxs[activePage][i] = display.max[i];
    pageBtnEn[activePage][i] = display.btnEn[i];
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
        pageLabelsOff[i][j] = "";
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
    display.setPoint[i] = i; // Dummy data
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

// --- OTA failsafe guard -------------------------------------------------------
// A one-shot timer that force-reboots the ESP32 from the independent esp_timer
// task. It is armed BEFORE the WiFi/HTTP calls, so even if loop() gets wedged
// inside the WiFi stack (or an lvgl lock) and never reaches the normal reboot
// path, the chip still restarts. This is what guarantees "reboot on failure".
static esp_timer_handle_t otaGuardTimer = NULL;

static void otaGuardFired(void *arg)
{
  // Runs in the esp_timer task — independent of the (possibly stuck) loop task.
  esp_restart();
}

static void otaArmGuard(uint32_t seconds)
{
  if (otaGuardTimer == NULL)
  {
    const esp_timer_create_args_t args = {
        .callback = &otaGuardFired,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_guard"};
    esp_timer_create(&args, &otaGuardTimer);
  }
  esp_timer_stop(otaGuardTimer); // ignore error if not running
  esp_timer_start_once(otaGuardTimer, (uint64_t)seconds * 1000000ULL);
  Serial.printf(">> OTA: failsafe reboot armed (%u s)\n", seconds);
}

// Append one OTA status line to /ota_status.txt on the SD card so the whole
// update sequence can be reviewed after a reboot without a serial monitor
// attached. Best-effort: a failed SD open is ignored (never blocks the OTA).
static void otaLogToSD(const char *msg)
{
  File lf = SD_MMC.open("/ota_status.txt", FILE_APPEND);
  if (lf)
  {
    lf.printf("[%lu ms] %s\n", (unsigned long)millis(), msg);
    lf.close();
  }
}

// Single status popup reused across the whole OTA sequence so the user always
// sees the current stage (connecting / connected / updating % / error) instead
// of a stack of separate message boxes.
static lv_obj_t *otaMbox = NULL;

// Set once the LVGL task and RGB panel are torn down for the update. After that
// the display is gone, so status updates are serial+SD only (never touch lvgl).
static bool otaDisplayDown = false;

// Create the popup on first call, then just refresh its text on later calls.
// Uses a finite lock timeout so a stuck UI can never block the OTA itself.
static void otaShowStatus(const char *msg)
{
  Serial.printf(">> OTA: %s\n", msg);
  otaLogToSD(msg);

  // Once the panel/LVGL are down (WiFi/OTA phase) there is nothing to draw to.
  if (otaDisplayDown)
    return;

  if (lvgl_port_lock(1000))
  {
    if (otaMbox == NULL)
    {
      // add_close_btn = false: user must not dismiss it mid-update
      otaMbox = lv_msgbox_create(NULL, "System Update", msg, NULL, false);
      lv_obj_center(otaMbox);
    }
    else
    {
      lv_label_set_text(lv_msgbox_get_text(otaMbox), msg);
    }
    // NOTE: do NOT call lv_task_handler() here. The dedicated lvgl_port_task
    // renders the screen; its flush callback blocks on a task notification that
    // is only delivered to that task. Calling the handler from loop() would
    // deadlock the OTA. We just update the widget and let the lvgl task draw it.
    lvgl_port_unlock();
  }
  else
  {
    Serial.println(">> OTA: (UI lock busy, continuing without popup update)");
  }
}

// httpUpdate progress callback: show download percentage (throttled to whole %).
static void otaProgressCb(int cur, int total)
{
  static int lastPct = -1;
  if (total <= 0)
    return;

  int pct = (int)((cur * 100L) / total);
  if (pct == lastPct)
    return;
  lastPct = pct;

  char buf[64];
  snprintf(buf, sizeof(buf), "Updating... %d%%\nDo not turn off.", pct);
  otaShowStatus(buf);
}

// Show an error message, wait 1 minute, then reboot the ESP32 so it returns to a
// clean state on its own instead of staying stuck after a failed/aborted update.
static void rebootAfterOtaFailure(const char *reason)
{
  char buf[80];
  snprintf(buf, sizeof(buf), "%s.\nRestarting in 20 s...", reason);
  otaShowStatus(buf);

  delay(20000); // 20 seconds
  ESP.restart();
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

  // Failsafe: if anything below wedges (WiFi stack, lvgl lock, stalled
  // download) the chip reboots on its own. 90 s covers the 20 s WiFi connect
  // attempt plus margin; re-armed to a larger window once the download starts.
  otaArmGuard(90);

  // 1. Show "connecting" status
  Serial.printf(">> OTA: Connecting to SSID '%s' (pass len %d)\n",
                config_ssid.c_str(), config_pass.length());
  otaShowStatus("Updating firmware...\nScreen will go dark.\nDo NOT power off.");

  // 2. Connect to WiFi.
  // Minimal, reliable bring-up from WIFI_OFF. Each step is logged so a hang is
  // pinpointed in the serial output.
  // WiFi init needs internal (DMA-capable) RAM; log how much is free so we can
  // see if esp_wifi_init is starving (a silent panic on this 320 KB-RAM board).
  Serial.printf(">> OTA: free heap=%u  free internal=%u  largest internal block=%u\n",
                ESP.getFreeHeap(),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  // Mirror the heap numbers to SD: WiFi.mode() can abort (panic) if the largest
  // *contiguous* internal block is too small for esp_wifi_init, even when total
  // free looks fine. This line lands on the card right before the crash point.
  {
    char hbuf[96];
    snprintf(hbuf, sizeof(hbuf),
             "pre-WiFi heap free_int=%u largest_int_block=%u",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    otaLogToSD(hbuf);
  }
  Serial.println(">> OTA: wifi persistent(false)");
  WiFi.persistent(false);
  Serial.flush();

  // --- Tear down the display BEFORE any cache-disabling flash work ----------
  // Root cause of the stage-1 panic (reset_reason=4): esp_wifi_init() reads PHY
  // calibration data from NVS, which disables the CPU cache. While the RGB
  // panel runs, its bounce-buffer ISR touches PSRAM and flash-resident code; if
  // it fires during that window the chip panics. The OTA flash writes later
  // disable the cache the same way. So stop the renderer and the panel now —
  // every OTA outcome ends in a reboot, so we never need the display back.
  Serial.println(">> OTA: stopping LVGL + RGB panel before WiFi/flash work");
  otaLogToSD("stopping LVGL + RGB panel");
  delay(800);                 // let the last status frame render first
  lvgl_port_stop_rendering(); // park the LVGL task (no more panel draws)
  waveshare_rgb_lcd_deinit(); // stop RGB GDMA + bounce/VSYNC ISR
  wavesahre_rgb_lcd_bl_off(); // dark screen instead of garbage once DMA stops
  otaDisplayDown = true;      // further otaShowStatus() = serial + SD only
  otaLogToSD("display down; entering WiFi.mode()");

  Serial.println(">> OTA: wifi mode(STA)");
  Serial.flush();
  otaBcStage = 1; // about to enter WiFi.mode() — the previous crash point
  WiFi.mode(WIFI_STA);
  otaBcStage = 2; // WiFi.mode() returned
  Serial.println(">> OTA: wifi begin()");
  WiFi.begin(config_ssid.c_str(), config_pass.c_str());
  Serial.println(">> OTA: entering status loop");

  const int maxTries = 40; // 40 * 500 ms = 20 s
  int timeout = 0;
  wl_status_t st;
  while ((st = WiFi.status()) != WL_CONNECTED)
  {
    delay(500);
    timeout++;
    // Log the raw status code so we can tell *why* it isn't connecting.
    Serial.printf(">> OTA: WiFi status=%d (try %d/%d)\n", st, timeout, maxTries);

    if (timeout >= maxTries)
    {
      Serial.println(">> OTA Error: WiFi Connection Failed");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      // Show the last status code so the cause is visible on the display too.
      char buf[64];
      snprintf(buf, sizeof(buf), "WiFi connection failed (code %d)", st);
      rebootAfterOtaFailure(buf);
      return; // Not reached: rebootAfterOtaFailure() restarts the ESP32
    }
  }
  Serial.printf(">> OTA: WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());

  // Connected: extend the failsafe to allow the firmware download to finish.
  otaArmGuard(240);

  // 3. Connected
  otaShowStatus("WiFi connected.\nStarting update...");

  // 4. Execute HTTP(S) Update (progress callback updates the popup live)
  httpUpdate.setLedPin(-1);
  httpUpdate.onProgress(otaProgressCb);

  Serial.print(">> OTA URL: ");
  Serial.println(fwURL);

  const bool useTLS = fwURL.startsWith("https://");

  // This function will Reboot the ESP32 automatically on success
  otaBcStage = 3; // inside httpUpdate (download + flash write)
  t_httpUpdate_return ret;
  if (useTLS)
  {
    // HTTPS: setInsecure() skips server-certificate validation. Fine on a
    // trusted LAN; for an untrusted network use setCACert() with the server's
    // root CA instead. TLS needs ~30-50 KB of internal RAM for its buffers.
    Serial.println(">> OTA: using TLS (insecure: cert not validated)");
    otaLogToSD("OTA over HTTPS (setInsecure)");
    WiFiClientSecure sclient;
    sclient.setInsecure();
    ret = httpUpdate.update(sclient, fwURL);
  }
  else
  {
    WiFiClient client;
    ret = httpUpdate.update(client, fwURL);
  }
  otaBcStage = 0; // update returned (failure path); cleared so next boot is clean

  // 5. Handle Errors (If we are still here, the update did not reboot us -> it failed)
  // Disconnect and disable WiFi before the reboot.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
  {
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    char errbuf[80];
    snprintf(errbuf, sizeof(errbuf), "Error: %s", httpUpdate.getLastErrorString().c_str());
    rebootAfterOtaFailure(errbuf);
    break;
  }

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    rebootAfterOtaFailure("No update available");
    break;

  case HTTP_UPDATE_OK:
    // On success the library already rebooted; we should never get here.
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

// --- "uploadFile" RS485 command: download a remote file onto the SD card ------
// Mirrors performOTAUpdate()'s WiFi bring-up, failsafe guard and display
// teardown, but instead of flashing the firmware it streams the HTTP(S) body
// into a file on the SD card. Reuses the same otaArmGuard()/otaShowStatus()/
// otaLogToSD()/rebootAfterOtaFailure() helpers so the on-screen + SD status
// reporting is identical to OTA. Reboots the display after a successful save.
void performFileUpload()
{
  if (config_ssid == "" || config_fileURL == "" || config_filePath == "")
  {
    Serial.println(">> UPLOAD Skipped: Missing WiFi, URL or destination path");
    return;
  }

  const String url = config_fileURL;
  const String destPath = config_filePath;
  // Download into a temp file first; only rename to the final path once the
  // whole body is on the card, so a partial download never replaces a good file.
  const String tmpPath = destPath + ".part";

  Serial.println(">> UPLOAD: Initializing...");

  // Failsafe: reboot if anything below wedges. 90 s covers the 20 s WiFi connect
  // attempt plus margin; re-armed to a larger window once data starts flowing.
  otaArmGuard(90);

  Serial.printf(">> UPLOAD: Connecting to SSID '%s' (pass len %d)\n",
                config_ssid.c_str(), config_pass.length());
  otaShowStatus("Downloading file...\nScreen will go dark.\nDo NOT power off.");

  WiFi.persistent(false);
  Serial.flush();

  // Tear down the display BEFORE any WiFi/flash work — same panic root cause as
  // OTA: esp_wifi_init() disables the CPU cache while the RGB panel ISR touches
  // PSRAM/flash. We reboot at the end regardless, so the panel is not needed.
  Serial.println(">> UPLOAD: stopping LVGL + RGB panel before WiFi work");
  otaLogToSD("UPLOAD: stopping LVGL + RGB panel");
  delay(800); // let the last status frame render first
  lvgl_port_stop_rendering();
  waveshare_rgb_lcd_deinit();
  wavesahre_rgb_lcd_bl_off();
  otaDisplayDown = true; // further otaShowStatus() = serial + SD only
  otaLogToSD("UPLOAD: display down; entering WiFi.mode()");

  WiFi.mode(WIFI_STA);
  WiFi.begin(config_ssid.c_str(), config_pass.c_str());

  const int maxTries = 40; // 40 * 500 ms = 20 s
  int timeout = 0;
  wl_status_t st;
  while ((st = WiFi.status()) != WL_CONNECTED)
  {
    delay(500);
    timeout++;
    Serial.printf(">> UPLOAD: WiFi status=%d (try %d/%d)\n", st, timeout, maxTries);
    if (timeout >= maxTries)
    {
      Serial.println(">> UPLOAD Error: WiFi Connection Failed");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      char buf[64];
      snprintf(buf, sizeof(buf), "WiFi connection failed (code %d)", st);
      rebootAfterOtaFailure(buf);
      return; // Not reached: rebootAfterOtaFailure() restarts the ESP32
    }
  }
  Serial.printf(">> UPLOAD: WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());

  // Connected: extend the failsafe to allow the download to finish.
  otaArmGuard(240);
  otaShowStatus("WiFi connected.\nDownloading...");

  Serial.print(">> UPLOAD URL: ");
  Serial.println(url);
  Serial.print(">> UPLOAD dest: ");
  Serial.println(destPath);

  const bool useTLS = url.startsWith("https://");
  WiFiClientSecure sclient;
  WiFiClient client;
  HTTPClient http;
  bool begun;
  if (useTLS)
  {
    // HTTPS: setInsecure() skips cert validation (fine on a trusted LAN).
    sclient.setInsecure();
    begun = http.begin(sclient, url);
  }
  else
  {
    begun = http.begin(client, url);
  }
  if (!begun)
  {
    rebootAfterOtaFailure("HTTP begin failed");
    return;
  }

  const int code = http.GET();
  Serial.printf(">> UPLOAD: HTTP GET -> %d\n", code);
  if (code != HTTP_CODE_OK)
  {
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    char buf[48];
    snprintf(buf, sizeof(buf), "HTTP error %d", code);
    rebootAfterOtaFailure(buf);
    return;
  }

  const int total = http.getSize(); // -1 when unknown (chunked transfer)

  SD_MMC.remove(tmpPath);
  File out = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!out)
  {
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    rebootAfterOtaFailure("SD open failed");
    return;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  int written = 0;
  int lastPct = -1;
  unsigned long lastData = millis();
  unsigned long lastArm = millis();

  while (http.connected() && (total < 0 || written < total))
  {
    size_t avail = stream->available();
    if (avail)
    {
      int n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
      if (n > 0)
      {
        out.write(buf, n);
        written += n;
        lastData = millis();

        // Keep the failsafe fed while data flows (throttled to avoid serial
        // spam / timer churn). Covers chunked transfers too, which have no %.
        if (millis() - lastArm > 5000)
        {
          otaArmGuard(240);
          lastArm = millis();
        }

        if (total > 0)
        {
          int pct = (int)((written * 100L) / total);
          if (pct != lastPct)
          {
            lastPct = pct;
            char m[64];
            snprintf(m, sizeof(m), "Downloading... %d%%\nDo not turn off.", pct);
            otaShowStatus(m);
          }
        }
      }
    }
    else
    {
      if (millis() - lastData > 30000) // 30 s with no data => give up
      {
        out.close();
        SD_MMC.remove(tmpPath);
        http.end();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        rebootAfterOtaFailure("Download stalled");
        return;
      }
      delay(10);
    }
  }

  out.close();
  http.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (total > 0 && written < total)
  {
    SD_MMC.remove(tmpPath);
    rebootAfterOtaFailure("Incomplete download");
    return;
  }

  // Atomically replace the destination with the freshly downloaded temp file.
  SD_MMC.remove(destPath);
  if (!SD_MMC.rename(tmpPath, destPath))
  {
    SD_MMC.remove(tmpPath);
    rebootAfterOtaFailure("SD rename failed");
    return;
  }

  Serial.printf(">> UPLOAD: saved %d bytes to %s\n", written, destPath.c_str());
  char okbuf[80];
  snprintf(okbuf, sizeof(okbuf), "UPLOAD: success (%d bytes -> %s)",
           written, destPath.c_str());
  otaLogToSD(okbuf);
  otaShowStatus("Download complete.\nRestarting...");
  delay(1500);
  ESP.restart();
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
    display.btnEn[tButton] = newEn;
    pageBtnEn[activePage][tButton] = newEn;
    display.setPoint[tButton] = newEn ? display.max[tButton] : display.min[tButton];
    newSetPoint[tButton] = display.setPoint[tButton];
    valueChanged[tButton] = 1;
    SETPOINT_CHANGED = true;
    UpdateBtnLeds(display);
    // Instantly switch the button caption to its ON/OFF label without waiting
    // for the central to round-trip a new label back.
    UpdateBtnLabels(display, phomeBtnLabel);
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
  delay(200);

  // Why did we (re)boot? After a silent USB-CDC reset this is the only way to
  // tell a brownout / panic / watchdog apart from a normal power-on.
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("\n[BOOT] reset reason = %d  (1=POWERON 3=SW 4=PANIC 5=INT_WDT 6=TASK_WDT 7=WDT 9=BROWNOUT)\n", rr);
  Serial.printf("[BOOT] free heap=%u  free internal=%u\n",
                ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  // If the previous boot died mid-OTA, the breadcrumb survived the reset.
  uint32_t prevOtaStage = (otaBcMagic == OTA_BC_MAGIC) ? otaBcStage : 0;
  if (prevOtaStage != 0)
  {
    Serial.printf("[BOOT] *** previous boot died during OTA at stage %u "
                  "(1=before WiFi.mode 2=after WiFi.mode 3=in httpUpdate) ***\n",
                  prevOtaStage);
  }
  otaBcMagic = OTA_BC_MAGIC;
  otaBcStage = 0;

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

  // Persist the boot diagnostic to SD so it survives the USB-CDC drop on reset.
  // Open /otalog.txt and read it after a crash to see reset reason + OTA stage.
  {
    File lf = SD_MMC.open("/otalog.txt", FILE_APPEND);
    if (lf)
    {
      lf.printf("[BOOT] millis=%lu reset_reason=%d prevOtaStage=%u freeInternal=%u\n",
                (unsigned long)millis(), (int)rr, prevOtaStage,
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
      lf.close();
      Serial.println("[BOOT] diagnostic appended to /otalog.txt");
    }
    else
    {
      Serial.println("[BOOT] could not open /otalog.txt for logging");
    }
  }

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
  lvgl_sd_fs_init(); // Must be after lvgl_port_init() which calls lv_init()
  ESP_LOGI(TAG, "Display LVGL initialized");

  // 6. Build UI (Thread Safe)
  if (lvgl_port_lock(-1))
  {
    ui_init();
    force_bind_keyboard_and_events();
    // Show the real firmware version on the info page (ui_screenInfo defaults
    // to a hardcoded "FW 1.0.0"); keep it in sync with VERSION from this file.
    lv_label_set_text_fmt(ui_fwVersion, "FW %s", VERSION);
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
  if (PERFORM_FILE_UPLOAD)
  {
    performFileUpload();
    PERFORM_FILE_UPLOAD = 0;
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
