#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include "secrets.h"
#ifndef API_TOKEN
#define API_TOKEN ""
#endif
namespace {
constexpr int SCREEN_W = 320, SCREEN_H = 480;
constexpr int TOUCH_CS_PIN = 33, TOUCH_IRQ_PIN = 36;
constexpr int TOUCH_SCK_PIN = 14, TOUCH_MISO_PIN = 12, TOUCH_MOSI_PIN = 13;
constexpr unsigned long POLL_INTERVAL_MS = 5000, WIFI_TIMEOUT_MS = 15000;
// Typical XPT2046 range for this panel. Calibrate if taps are offset.
constexpr int TOUCH_X_MIN = 280, TOUCH_X_MAX = 3860;
constexpr int TOUCH_Y_MIN = 340, TOUCH_Y_MAX = 3860;
TFT_eSPI tft;
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
uint16_t colorFor(const String& state) {
  if (state == "out") return TFT_RED;
  if (state == "urgent") return tft.color565(255, 96, 0);
  if (state == "low") return TFT_ORANGE;
  return TFT_DARKGREEN;
}
const char* labelFor(const String& state) {
  if (state == "out") return "OUT OF PAPER";
  if (state == "urgent") return "DANGEROUSLY LOW";
  if (state == "low") return "LOW ON PAPER";
  return "OK";
}
// Sits below the title, not beside it. The title is wide enough at font 4 that a
// badge on the same line clips its last letters when the badge clears its box.
void drawStatusBadge() {
  tft.fillRect(SCREEN_W - 100, 44, 92, 22, TFT_DARKGREEN);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(online ? TFT_GREENYELLOW : TFT_ORANGE, TFT_DARKGREEN);
  tft.drawString(online ? "LIVE" : "OFFLINE", SCREEN_W - 12, 46, 2); tft.setTextDatum(TL_DATUM);
}
void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, 72, TFT_DARKGREEN);
  tft.setTextDatum(TC_DATUM); tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.drawString("TP REFRESHER", SCREEN_W / 2, 13, 4); tft.setTextDatum(TL_DATUM);
  drawStatusBadge();
}
void drawCard(size_t index) {
  const Bathroom& room = bathrooms[index]; const int y = 84 + static_cast<int>(index) * 124;
  const uint16_t accent = colorFor(room.state);
  tft.fillRoundRect(10, y, 300, 108, 12, TFT_WHITE); tft.fillRoundRect(10, y, 12, 108, 12, accent);
  tft.setTextColor(TFT_DARKGREY, TFT_WHITE); tft.drawString(room.name, 30, y + 14, 4);
  tft.setTextColor(accent, TFT_WHITE);
  tft.drawString(labelFor(room.state), 31, y + 55, 2);
  // Say so when time bumped this up, so nobody thinks a person reported it.
  if (room.reported.length() && room.reported != room.state) {
    tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
    tft.drawString(String("was ") + labelFor(room.reported), 31, y + 83, 2);
  }
  tft.setTextDatum(TR_DATUM); tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
  tft.drawString(room.state == "ok" ? "Tap to flag" : "Tap to confirm", 298, y + 83, 2); tft.setTextDatum(TL_DATUM);
}
void drawCardNote(size_t index, const char* note) {
  const int y = 84 + static_cast<int>(index) * 124;
  tft.fillRect(140, y + 76, 158, 22, TFT_WHITE);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
  tft.drawString(note, 298, y + 83, 2); tft.setTextDatum(TL_DATUM);
}
void rememberPainted() {
  for (size_t i = 0; i < bathroomCount; ++i) { paintedState[i] = bathrooms[i].state + "/" + bathrooms[i].reported; paintedName[i] = bathrooms[i].name; }
  paintedCount = bathroomCount; paintedOnline = online; screenReady = true;
}
void drawScreen() {
  tft.fillScreen(TFT_LIGHTGREY); drawHeader();
  for (size_t i = 0; i < bathroomCount; ++i) drawCard(i);
  tft.setTextColor(TFT_DARKGREY, TFT_LIGHTGREY); tft.setTextDatum(BC_DATUM);
  tft.drawString("Tap a bathroom to update it", SCREEN_W / 2, SCREEN_H - 12, 2); tft.setTextDatum(TL_DATUM);
  rememberPainted();
}
// Repaint only what changed. A full redraw every poll makes the screen visibly flash.
void renderUpdates() {
  if (!screenReady || bathroomCount != paintedCount) { drawScreen(); return; }
  if (online != paintedOnline) { drawStatusBadge(); paintedOnline = online; }
  for (size_t i = 0; i < bathroomCount; ++i) {
    const String signature = bathrooms[i].state + "/" + bathrooms[i].reported;
    if (signature == paintedState[i] && bathrooms[i].name == paintedName[i]) continue;
    drawCard(i); paintedState[i] = signature; paintedName[i] = bathrooms[i].name;
  }
}
void showMessage(const char* title, const char* detail) {
  tft.fillScreen(TFT_LIGHTGREY); tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_DARKGREEN, TFT_LIGHTGREY);
  tft.drawString(title, SCREEN_W / 2, 200, 4); tft.setTextColor(TFT_DARKGREY, TFT_LIGHTGREY);
  tft.drawString(detail, SCREEN_W / 2, 245, 2); tft.setTextDatum(TL_DATUM);
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
  DynamicJsonDocument document(4096); const DeserializationError error = deserializeJson(document, http.getStream()); http.end();
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
  online = true; return true;
}
void handleTouch() {
  if (!touch.touched()) return;
  TS_Point raw = touch.getPoint();
  // Both axes are inverted relative to the panel: Y because the panel runs opposite
  // the display, X because the whole screen is rotated 180 degrees.
  const int x = constrain(map(raw.x, TOUCH_X_MAX, TOUCH_X_MIN, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  const int y = constrain(map(raw.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
  logf("touch raw=(%d,%d) mapped=(%d,%d)\n", raw.x, raw.y, x, y);
  for (size_t i = 0; i < bathroomCount; ++i) {
    const int top = 84 + static_cast<int>(i) * 124;
    if (x >= 10 && x <= 310 && y >= top && y <= top + 108) {
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
void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { screenReady = false; showMessage("Updating", "Do not unplug"); logf("OTA start\n"); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int lastPercent = -1; const int percent = total ? static_cast<int>((done * 100ULL) / total) : 0;
    if (percent == lastPercent) return;
    lastPercent = percent;
    tft.fillRect(60, 280, 200, 16, TFT_WHITE); tft.fillRect(60, 280, 2 * percent, 16, TFT_DARKGREEN);
  });
  ArduinoOTA.onEnd([]() { showMessage("Updated", "Restarting"); logf("OTA done\n"); });
  ArduinoOTA.onError([](ota_error_t error) { logf("OTA error %u\n", error); showMessage("Update failed", "Board still running"); });
  ArduinoOTA.begin();
  logf("OTA ready at %s.local, build %s %s\n", OTA_HOSTNAME, __DATE__, __TIME__);
}
void setup() {
  Serial.begin(115200); pinMode(27, OUTPUT); digitalWrite(27, HIGH); tft.init(); tft.setRotation(2);  // 180 degrees, so the USB cable exits the top
  SPI.begin(TOUCH_SCK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN); touch.begin(); touch.setRotation(0);
  showMessage("TP Refresher", "Connecting to Wi-Fi...");
  WiFi.setHostname(OTA_HOSTNAME); online = connectWiFi();
  fetchStatus(); drawScreen();  // loop() starts OTA once Wi-Fi is up
}
void loop() {
  static bool otaStarted = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) { startOta(); otaStarted = true; }
    ArduinoOTA.handle();
  } else {
    otaStarted = false;
  }
  handleTouch();
  if (millis() - lastPoll >= POLL_INTERVAL_MS) { lastPoll = millis(); fetchStatus(); renderUpdates(); }
  delay(20);
}
