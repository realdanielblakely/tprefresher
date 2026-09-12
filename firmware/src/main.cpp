#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include "fonts/PlexUI.h"
#include "fonts/PlexCaption.h"
#include "fonts/PlexDisplay.h"
#include "secrets.h"
#ifndef API_TOKEN
#define API_TOKEN ""
#endif
namespace {
constexpr int SCREEN_W = 320, SCREEN_H = 480;
constexpr int TOUCH_CS_PIN = 33, TOUCH_IRQ_PIN = 36;
constexpr int TOUCH_SCK_PIN = 14, TOUCH_MISO_PIN = 12, TOUCH_MOSI_PIN = 13;
constexpr unsigned long POLL_INTERVAL_MS = 5000, WIFI_TIMEOUT_MS = 15000;
// Backlight. The panel is far too bright left at full on a wall all day, so it
// idles dim and wakes to full on a touch or a real status change. Tune these.
constexpr int BACKLIGHT_PIN = 27, BACKLIGHT_CHANNEL = 0;
constexpr int DAY_IDLE = 40, DAY_FULL = 255;
constexpr int NIGHT_IDLE = 8, NIGHT_FULL = 110;
constexpr int NIGHT_START_HOUR = 22, NIGHT_END_HOUR = 7;
constexpr unsigned long BACKLIGHT_WAKE_MS = 20000;
// America/New_York with US daylight saving rules. Change this one string to move
// the board to another timezone.
constexpr const char* TIMEZONE = "EST5EDT,M3.2.0,M11.1.0";
unsigned long lastActivity = 0;
int backlightLevel = DAY_FULL;
bool clockReady = false;
// Typical XPT2046 range for this panel. Calibrate if taps are offset.
constexpr int TOUCH_X_MIN = 280, TOUCH_X_MAX = 3860;
constexpr int TOUCH_Y_MIN = 340, TOUCH_Y_MAX = 3860;
// Dark palette. A mostly white UI on an LED backlit panel is bright regardless of
// the backlight setting, because brightness is pixels as much as backlight.
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
constexpr uint16_t COL_BG = RGB565(10, 12, 16);
constexpr uint16_t COL_CARD = RGB565(26, 30, 36);
constexpr uint16_t COL_HEADER = RGB565(14, 42, 36);
constexpr uint16_t COL_TAB_IDLE = RGB565(32, 37, 44);
constexpr uint16_t COL_TEXT = RGB565(228, 232, 238);
constexpr uint16_t COL_MUTED = RGB565(130, 140, 150);
constexpr uint16_t COL_OK = RGB565(34, 197, 94);
constexpr uint16_t COL_LOW = RGB565(250, 204, 21);
constexpr uint16_t COL_URGENT = RGB565(249, 115, 22);
constexpr uint16_t COL_OUT = RGB565(239, 68, 68);
constexpr uint16_t COL_RUNNING = RGB565(56, 160, 220);

TFT_eSPI tft;
// Smooth fonts in three roles: Display for the countdown, UI for names and status
// words, Caption for hints. Only one can be loaded at a time, so useFont tracks
// the current one and switches only when it actually changes.
const uint8_t* currentFont = nullptr;
void useFont(const uint8_t* font) {
  if (font == currentFont) return;
  if (currentFont) tft.unloadFont();
  tft.loadFont(font);
  currentFont = font;
}
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
WiFiUDP logUdp;
// Mirror serial output to a UDP broadcast so the board stays debuggable off USB.
void logf(const char* format, ...) {
  char line[192]; va_list args; va_start(args, format);
  vsnprintf(line, sizeof(line), format, args); va_end(args);
  Serial.print(line);
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress broadcast = WiFi.localIP(); broadcast[3] = 255;
    if (logUdp.beginPacket(broadcast, LOG_UDP_PORT)) { logUdp.write(reinterpret_cast<const uint8_t*>(line), strlen(line)); logUdp.endPacket(); }
  }
}
unsigned long lastPoll = 0;
bool online = false;
struct Bathroom { String id; String name; String state; String reported; String flaggedAt; };
Bathroom bathrooms[8]; size_t bathroomCount = 0;
// Last values actually painted, so refreshes only touch what changed.
String paintedState[8]; String paintedName[8];
size_t paintedCount = 0; bool paintedOnline = false; bool screenReady = false;

// Second screen: laundry timers. The timers themselves live on the server, so a
// reboot or a firmware push never loses a running cycle.
struct Machine { String id; String name; String state; long remaining; };
Machine machines[4]; size_t machineCount = 0;
unsigned long lastFetchMs = 0;
int screen = 0;  // 0 status, 1 laundry
String paintedMachine[4]; long paintedRemaining[4] = {-1, -1, -1, -1};
bool laundryReady = false;
// Tabs sit under the header, not at the bottom edge. Resistive panels are
// unreliable at the extremes, and the very bottom may be unreachable entirely.
constexpr int TAB_Y = 78, TAB_H = 38;
constexpr int CARD_Y = 128, CARD_H = 100, CARD_GAP = 10;
// The colour is the information, so give it real area rather than a sliver.
constexpr int ACCENT_W = 76, CARD_X = 10, CARD_W = 300, HEADER_H = 72;
// Every screen keeps content inside this column. Header, cards and hints all align
// to it, which is most of the difference between "app" and "sketch".
constexpr int EDGE_L = CARD_X + 12, EDGE_R = CARD_X + CARD_W - 12;
constexpr int TEXT_X = CARD_X + ACCENT_W + 14;
constexpr int CONTENT_MID = CARD_X + ACCENT_W + (CARD_W - ACCENT_W) / 2;
constexpr int LCARD_Y = 128, LCARD_H = 148, LCARD_GAP = 16;


long remainingNow(size_t index) {
  if (machines[index].state != "running") return machines[index].remaining;
  const long elapsed = (millis() - lastFetchMs) / 1000;
  const long left = machines[index].remaining - elapsed;
  return left > 0 ? left : 0;
}
// Until the clock syncs, treat it as daytime rather than guessing dark.
bool isNight() {
  if (!clockReady) return false;
  struct tm now;
  if (!getLocalTime(&now, 50)) return false;
  return NIGHT_START_HOUR > NIGHT_END_HOUR
    ? (now.tm_hour >= NIGHT_START_HOUR || now.tm_hour < NIGHT_END_HOUR)
    : (now.tm_hour >= NIGHT_START_HOUR && now.tm_hour < NIGHT_END_HOUR);
}
int idleLevel() { return isNight() ? NIGHT_IDLE : DAY_IDLE; }
int fullLevel() { return isNight() ? NIGHT_FULL : DAY_FULL; }
// Fade rather than step, so waking does not read as a flash in a dark room.
void setBacklight(int target) {
  target = constrain(target, 0, 255);
  if (target == backlightLevel) return;
  const int step = target > backlightLevel ? 6 : -6;
  while (abs(target - backlightLevel) > abs(step)) { backlightLevel += step; ledcWrite(BACKLIGHT_CHANNEL, backlightLevel); delay(8); }
  backlightLevel = target; ledcWrite(BACKLIGHT_CHANNEL, backlightLevel);
}
void wakeScreen() { lastActivity = millis(); setBacklight(fullLevel()); }
uint16_t colorFor(const String& state) {
  if (state == "out") return COL_OUT;
  if (state == "urgent") return COL_URGENT;
  if (state == "low") return COL_LOW;
  return COL_OK;
}
const char* labelFor(const String& state) {
  if (state == "out") return "OUT";
  if (state == "urgent") return "URGENT";
  if (state == "low") return "LOW";
  return "OK";
}
// Sits below the title, not beside it. The title is wide enough at font 4 that a
// badge on the same line clips its last letters when the badge clears its box.
// The badge shares the title line. "REFRESH" ends near x=209 and the badge box
// starts at 236, so the two cannot collide the way the old long title did.
constexpr int BADGE_X = 236, BADGE_W = EDGE_R - BADGE_X + 12;
void drawStatusBadge() {
  tft.fillRect(BADGE_X, 24, BADGE_W, 24, COL_HEADER);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(online ? COL_OK : COL_URGENT, COL_HEADER);
  useFont(PlexCaption);
  tft.drawString(online ? "LIVE" : "OFFLINE", EDGE_R, 26); tft.setTextDatum(TL_DATUM);
}
void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(COL_TEXT, COL_HEADER);
  // Title starts on the same left inset as every card, not floating centre.
  useFont(PlexUI);
  tft.drawString("DAEMON", EDGE_L, 20);
  drawStatusBadge();
}
void drawCard(size_t index) {
  const Bathroom& room = bathrooms[index]; const int y = CARD_Y + static_cast<int>(index) * (CARD_H + CARD_GAP);
  const uint16_t accent = colorFor(room.state);
  tft.fillRect(CARD_X, y, CARD_W, CARD_H, COL_CARD);
  tft.fillRect(CARD_X, y, ACCENT_W, CARD_H, accent);
  useFont(PlexUI);
  tft.setTextColor(COL_TEXT, COL_CARD); tft.drawString(room.name, TEXT_X, y + 20);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(accent, COL_CARD);
  tft.drawString(labelFor(room.state), EDGE_R, y + 20);
  tft.setTextDatum(TL_DATUM);
  useFont(PlexCaption);
  // The bottom line holds one thing. When time escalated a room, saying so beats
  // repeating a hint you can infer, and the two do not fit side by side.
  tft.setTextColor(COL_MUTED, COL_CARD);
  const bool escalated = room.reported.length() && room.reported != room.state;
  if (escalated) {
    tft.drawString("raised by time", TEXT_X, y + 64);
  } else {
    tft.setTextDatum(TR_DATUM);
    tft.drawString(room.state == "ok" ? "Tap to flag" : "Tap to confirm", EDGE_R, y + 64);
    tft.setTextDatum(TL_DATUM);
  }
}
void drawCardNote(size_t index, const char* note) {
  const int y = CARD_Y + static_cast<int>(index) * (CARD_H + CARD_GAP);
  tft.fillRect(140, y + 62, 158, 24, COL_CARD);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(COL_MUTED, COL_CARD);
  useFont(PlexCaption);
  tft.drawString(note, EDGE_R, y + 64); tft.setTextDatum(TL_DATUM);
}
void rememberPainted() {
  for (size_t i = 0; i < bathroomCount; ++i) { paintedState[i] = bathrooms[i].state + "/" + bathrooms[i].reported; paintedName[i] = bathrooms[i].name; }
  paintedCount = bathroomCount; paintedOnline = online; screenReady = true;
}
void drawTabs() {
  const uint16_t activeBg = COL_CARD, idleBg = COL_BG;
  tft.fillRect(0, TAB_Y, SCREEN_W / 2, TAB_H, screen == 0 ? activeBg : idleBg);
  tft.fillRect(SCREEN_W / 2, TAB_Y, SCREEN_W / 2, TAB_H, screen == 1 ? activeBg : idleBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(screen == 0 ? COL_TEXT : COL_MUTED, screen == 0 ? activeBg : idleBg);
  useFont(PlexUI);
  tft.drawString("PAPER", SCREEN_W / 4, TAB_Y + TAB_H / 2);
  tft.setTextColor(screen == 1 ? COL_TEXT : COL_MUTED, screen == 1 ? activeBg : idleBg);
  tft.drawString("LAUNDRY", SCREEN_W * 3 / 4, TAB_Y + TAB_H / 2);
  tft.setTextDatum(TL_DATUM);
}
uint16_t machineColor(const String& state) {
  if (state == "done") return COL_OUT;
  if (state == "running") return COL_RUNNING;
  return COL_MUTED;
}
void formatRemaining(long seconds, char* out, size_t size) {
  if (seconds < 0) seconds = 0;
  snprintf(out, size, "%02ld:%02ld", seconds / 60, seconds % 60);
}
// Only the digits change every second, so this repaints just that strip.
void drawMachineTime(size_t index) {
  const int y = LCARD_Y + static_cast<int>(index) * (LCARD_H + LCARD_GAP);
  const Machine& machine = machines[index];
  tft.fillRect(CARD_X + ACCENT_W, y + 34, CARD_X + CARD_W - ACCENT_W - 12, 74, COL_CARD);
  tft.setTextDatum(MC_DATUM);
  if (machine.state == "idle") {
    tft.setTextColor(COL_TEXT, COL_CARD);
    useFont(PlexUI); tft.drawString("READY", CONTENT_MID, y + 70);
  } else {
    char buffer[8]; formatRemaining(remainingNow(index), buffer, sizeof(buffer));
    tft.setTextColor(machineColor(machine.state), COL_CARD);
    if (machine.state == "done") { useFont(PlexUI); tft.drawString("DONE", CONTENT_MID, y + 70); }
    else { useFont(PlexDisplay); tft.drawString(buffer, CONTENT_MID, y + 70); }
  }
  tft.setTextDatum(TL_DATUM);
}
void drawMachine(size_t index) {
  const int y = LCARD_Y + static_cast<int>(index) * (LCARD_H + LCARD_GAP);
  const Machine& machine = machines[index];
  const uint16_t accent = machineColor(machine.state);
  tft.fillRect(CARD_X, y, CARD_W, LCARD_H, COL_CARD);
  tft.fillRect(CARD_X, y, ACCENT_W, LCARD_H, accent);
  tft.setTextColor(COL_TEXT, COL_CARD);
  useFont(PlexUI); tft.drawString(machine.name, TEXT_X, y + 8);
  drawMachineTime(index);
  tft.setTextDatum(BC_DATUM); tft.setTextColor(accent, COL_CARD);
  const char* hint = machine.state == "idle" ? "Tap to start"
                   : machine.state == "done" ? "Tap to clear" : "Tap to cancel";
  useFont(PlexCaption); tft.drawString(hint, CONTENT_MID, y + LCARD_H - 8);
  tft.setTextDatum(TL_DATUM);
}
void drawMachineNote(size_t index, const char* note) {
  const int y = LCARD_Y + static_cast<int>(index) * (LCARD_H + LCARD_GAP);
  tft.fillRect(CARD_X + ACCENT_W, y + LCARD_H - 24, CARD_W - ACCENT_W - 12, 20, COL_CARD);
  tft.setTextDatum(BC_DATUM); tft.setTextColor(COL_MUTED, COL_CARD);
  useFont(PlexCaption); tft.drawString(note, CONTENT_MID, y + LCARD_H - 8); tft.setTextDatum(TL_DATUM);
}
void drawLaundryScreen() {
  tft.fillScreen(COL_BG); drawHeader();
  for (size_t i = 0; i < machineCount; ++i) {
    drawMachine(i);
    paintedMachine[i] = machines[i].state; paintedRemaining[i] = remainingNow(i);
  }
  if (machineCount == 0) {
    tft.setTextDatum(MC_DATUM); tft.setTextColor(COL_MUTED, COL_BG);
    useFont(PlexUI); tft.drawString("No laundry data", SCREEN_W / 2, 240); tft.setTextDatum(TL_DATUM);
  }
  drawTabs(); laundryReady = true;
}
void drawScreen() {
  tft.fillScreen(COL_BG); drawHeader();
  for (size_t i = 0; i < bathroomCount; ++i) drawCard(i);
  drawTabs();
  rememberPainted();
}
void renderLaundry() {
  if (!laundryReady) { drawLaundryScreen(); return; }
  for (size_t i = 0; i < machineCount; ++i) {
    const long left = remainingNow(i);
    if (machines[i].state != paintedMachine[i]) { drawMachine(i); paintedMachine[i] = machines[i].state; paintedRemaining[i] = left; continue; }
    if (left != paintedRemaining[i]) { drawMachineTime(i); paintedRemaining[i] = left; }
  }
}
// Repaint only what changed. A full redraw every poll makes the screen visibly flash.
// A finished cycle should catch the eye even from the other screen.
void noticeLaundryChanges() {
  for (size_t i = 0; i < machineCount; ++i) {
    if (machines[i].state == "done" && paintedMachine[i] != "done") wakeScreen();
  }
}
void renderUpdates() {
  if (screen == 1) { renderLaundry(); return; }
  if (!screenReady || bathroomCount != paintedCount) { drawScreen(); return; }
  if (online != paintedOnline) { drawStatusBadge(); paintedOnline = online; }
  for (size_t i = 0; i < bathroomCount; ++i) {
    const String signature = bathrooms[i].state + "/" + bathrooms[i].reported;
    if (signature == paintedState[i] && bathrooms[i].name == paintedName[i]) continue;
    drawCard(i); paintedState[i] = signature; paintedName[i] = bathrooms[i].name; wakeScreen();
  }
}
void showMessage(const char* title, const char* detail) {
  tft.fillScreen(COL_BG); tft.setTextDatum(MC_DATUM); tft.setTextColor(COL_TEXT, COL_BG);
  useFont(PlexUI); tft.drawString(title, SCREEN_W / 2, 200);
  useFont(PlexCaption); tft.setTextColor(COL_MUTED, COL_BG);
  tft.drawString(detail, SCREEN_W / 2, 245); tft.setTextDatum(TL_DATUM);
}
bool connectWiFi() {
  // Power save drops inbound packets while the radio naps, which makes the board
  // unpingable and invisible to OTA even though its own requests still work.
  WiFi.mode(WIFI_STA); WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS); const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_TIMEOUT_MS) delay(250);
  if (WiFi.status() == WL_CONNECTED) logf("WiFi connected, ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else logf("WiFi FAILED, status=%d ssid=%s\n", WiFi.status(), WIFI_SSID);
  return WiFi.status() == WL_CONNECTED;
}
bool requestApi(const String& path, const char* method) {
  if (WiFi.status() != WL_CONNECTED && !connectWiFi()) { online = false; return false; }
  HTTPClient http; if (!http.begin(String(API_BASE_URL) + path)) { online = false; return false; }
  http.setTimeout(4500); if (strlen(API_TOKEN) > 0) http.addHeader("X-API-Token", API_TOKEN);
  const int code = strcmp(method, "POST") == 0 ? http.POST("") : http.GET(); http.end();
  online = code >= 200 && code < 300; return online;
}
bool fetchStatus() {
  if (WiFi.status() != WL_CONNECTED && !connectWiFi()) { online = false; return false; }
  HTTPClient http; if (!http.begin(String(API_BASE_URL) + "/api/status")) { online = false; return false; }
  http.setTimeout(4500); if (strlen(API_TOKEN) > 0) http.addHeader("X-API-Token", API_TOKEN);
  const int code = http.GET();
  logf("GET %s/api/status -> %d\n", API_BASE_URL, code);
  if (code != HTTP_CODE_OK) { http.end(); online = false; return false; }
  DynamicJsonDocument document(8192); const DeserializationError error = deserializeJson(document, http.getStream()); http.end();
  if (error) { online = false; return false; }
  bathroomCount = 0;
  for (JsonObject item : document["bathrooms"].as<JsonArray>()) {
    if (bathroomCount >= 8) break;
    bathrooms[bathroomCount].id = item["id"].as<const char*>();
    bathrooms[bathroomCount].name = item["name"].as<const char*>();
    bathrooms[bathroomCount].state = item["state"].as<const char*>();
    bathrooms[bathroomCount].reported = item["reported"].isNull() ? "" : item["reported"].as<const char*>();
    bathrooms[bathroomCount].flaggedAt = item["flaggedAt"].as<const char*>(); ++bathroomCount;
  }
  machineCount = 0;
  for (JsonObject item : document["laundry"].as<JsonArray>()) {
    if (machineCount >= 4) break;
    machines[machineCount].id = item["id"].as<const char*>();
    machines[machineCount].name = item["name"].as<const char*>();
    machines[machineCount].state = item["state"].as<const char*>();
    machines[machineCount].remaining = item["remainingSeconds"] | 0;
    ++machineCount;
  }
  lastFetchMs = millis();
  online = true; return true;
}
void handleTouch() {
  if (!touch.touched()) return;
  TS_Point raw = touch.getPoint();
  // A touch on a dimmed screen both wakes it and does what you tapped. Swallowing
  // that first touch avoided stray flags but made the board feel broken.
  wakeScreen();
  // X is NOT inverted. The 180 degree rotation cancels an inversion the panel
  // already had, which stayed hidden while every control spanned the full width.
  // Verified against the tab bar, the first left/right split on this display.
  const int x = constrain(map(raw.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  const int y = constrain(map(raw.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
  logf("touch raw=(%d,%d) mapped=(%d,%d)\n", raw.x, raw.y, x, y);
  if (y >= TAB_Y && y < TAB_Y + TAB_H) {
    const int wanted = x < SCREEN_W / 2 ? 0 : 1;
    if (wanted != screen) {
      screen = wanted;
      if (screen == 0) { screenReady = false; drawScreen(); } else { laundryReady = false; drawLaundryScreen(); }
    }
    while (touch.touched()) delay(10);
    return;
  }
  if (screen == 1) {
    for (size_t i = 0; i < machineCount; ++i) {
      const int top = LCARD_Y + static_cast<int>(i) * (LCARD_H + LCARD_GAP);
      if (x < 10 || x > 310 || y < top || y > top + LCARD_H) continue;
      const char* action = machines[i].state == "idle" ? "start" : "clear";
      drawMachineNote(i, "Saving...");
      requestApi(String("/api/laundry/") + machines[i].id + "/" + action, "POST");
      fetchStatus(); paintedMachine[i] = ""; renderLaundry();
      while (touch.touched()) delay(10);
      return;
    }
    return;
  }
  for (size_t i = 0; i < bathroomCount; ++i) {
    const int top = CARD_Y + static_cast<int>(i) * (CARD_H + CARD_GAP);
    if (x >= 10 && x <= 310 && y >= top && y <= top + CARD_H) {
      const char* action = bathrooms[i].state == "ok" ? "flag" : "confirm";
      drawCardNote(i, "Saving...");
      if (!requestApi(String("/api/bathrooms/") + bathrooms[i].id + "/" + action, "POST")) { drawCardNote(i, "Offline, try again"); delay(1200); }
      fetchStatus();
      paintedState[i] = ""; renderUpdates();
      while (touch.touched()) delay(10);
      return;
    }
  }
}
}
void syncClock() {
  configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
  struct tm now;
  clockReady = getLocalTime(&now, 6000);
  if (clockReady) logf("clock synced: %04d-%02d-%02d %02d:%02d local, night mode %s\n",
                       now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min,
                       isNight() ? "on" : "off");
  else logf("clock sync failed, staying on daytime brightness\n");
}
void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { screenReady = false; showMessage("Updating", "Do not unplug"); logf("OTA start\n"); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int lastPercent = -1; const int percent = total ? static_cast<int>((done * 100ULL) / total) : 0;
    if (percent == lastPercent) return;
    lastPercent = percent;
    tft.fillRect(60, 280, 200, 16, COL_CARD); tft.fillRect(60, 280, 2 * percent, 16, COL_HEADER);
  });
  ArduinoOTA.onEnd([]() { showMessage("Updated", "Restarting"); logf("OTA done\n"); });
  ArduinoOTA.onError([](ota_error_t error) { logf("OTA error %u\n", error); showMessage("Update failed", "Board still running"); });
  ArduinoOTA.begin();
  logf("OTA ready at %s.local, build %s %s\n", OTA_HOSTNAME, __DATE__, __TIME__);
}
void setup() {
  Serial.begin(115200);
  ledcSetup(BACKLIGHT_CHANNEL, 5000, 8); ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
  ledcWrite(BACKLIGHT_CHANNEL, DAY_FULL); lastActivity = millis();
  tft.init(); tft.setRotation(2);  // 180 degrees, so the USB cable exits the top
  SPI.begin(TOUCH_SCK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN); touch.begin(); touch.setRotation(0);
  showMessage("TP Refresher", "Connecting to Wi-Fi...");
  WiFi.setHostname(OTA_HOSTNAME); online = connectWiFi();
  fetchStatus(); drawScreen();  // loop() starts OTA once Wi-Fi is up
}
void loop() {
  static bool otaStarted = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) { startOta(); syncClock(); otaStarted = true; }
    ArduinoOTA.handle();
  } else {
    otaStarted = false;
  }
  handleTouch();
  if (millis() - lastPoll >= POLL_INTERVAL_MS) { lastPoll = millis(); fetchStatus(); noticeLaundryChanges(); renderUpdates(); }
  static unsigned long lastTick = 0;
  if (screen == 1 && millis() - lastTick >= 500) { lastTick = millis(); renderLaundry(); }
  if (millis() - lastActivity >= BACKLIGHT_WAKE_MS && backlightLevel != idleLevel()) setBacklight(idleLevel());
  delay(20);
}
