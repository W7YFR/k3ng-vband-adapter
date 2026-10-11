#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>
#include "diagnostics.h"
#include "config.h"
#include "log.h"

namespace {

const char *STAGE_NAMES[DIAG_STAGES] = {"vband", "mega", "keyer", "tone", "rxtext", "other"};

unsigned long lastReportMs = 0;
unsigned long stageStartUs = 0;
unsigned long stageMaxUs[DIAG_STAGES] = {};
unsigned long loopStartUs = 0;
unsigned long loopMaxUs = 0;

unsigned long lastServerMessageMs = 0;
unsigned long serverGapMaxMs = 0;
unsigned long lastSmkMs = 0;
unsigned long smkGapMaxMs = 0;
unsigned long smkCount = 0;
int queueDepth = 0;
int queueDepthMax = 0;
unsigned long queueDrops = 0;
unsigned long toneMs = 0;
unsigned long worstSmkGapMs = 0;
unsigned long wsDisconnects = 0;
unsigned long wifiDisconnects = 0;

// Kept events (DIAG_EVENTS, in NVS): why the adapter last went away.
enum EventKind : uint8_t { EVENT_RESET, EVENT_WIFI, EVENT_VBAND };
struct Event {
  uint8_t kind;
  uint8_t count;    // the same thing again in the same boot adds to this
  int16_t reason;   // esp_reset_reason_t, the WiFi reason code, or for VBand how long the server had been silent (0.1 s)
  int8_t rssi;
  uint8_t unused;
  uint16_t boot;
  uint32_t uptimeS; // since that boot, the latest time
};
Event events[DIAG_EVENTS];
uint8_t eventCount = 0;
uint16_t bootNumber = 0;
bool eventsDirty = false;
unsigned long lastSaveMs = 0;
bool wifiUp = false;
// Events come from the WiFi event task as well as loop().
portMUX_TYPE eventLock = portMUX_INITIALIZER_UNLOCKED;
constexpr const char *NVS_NAMESPACE = "diag";

void addEvent(EventKind kind, int reason, int rssi) {
  uint32_t uptime = millis() / 1000;
  portENTER_CRITICAL(&eventLock);
  Event *last = eventCount ? &events[eventCount - 1] : nullptr;
  if (last && last->kind == kind && (last->reason == reason || kind == EVENT_VBAND) && last->boot == bootNumber) {
    if (last->count < 255) last->count++;
    last->uptimeS = uptime;
    last->rssi = rssi;
    last->reason = reason;
  } else {
    if (eventCount == DIAG_EVENTS) {
      memmove(events, events + 1, sizeof(Event) * (DIAG_EVENTS - 1));
      eventCount--;
    }
    events[eventCount++] = {(uint8_t)kind, 1, (int16_t)reason, (int8_t)rssi, 0, bootNumber, uptime};
  }
  eventsDirty = true;
  portEXIT_CRITICAL(&eventLock);
}

void saveEvents() {
  Event copy[DIAG_EVENTS];
  portENTER_CRITICAL(&eventLock);
  memcpy(copy, events, sizeof(copy));
  uint8_t count = eventCount;
  eventsDirty = false;
  portEXIT_CRITICAL(&eventLock);
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBytes("events", copy, sizeof(copy));
  prefs.putUChar("count", count);
  prefs.end();
  lastSaveMs = millis();
}

// Reads the kept events and counts this boot.
void loadEvents() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  bootNumber = prefs.getUShort("boots", 0) + 1;
  prefs.putUShort("boots", bootNumber);
  if (prefs.getBytesLength("events") == sizeof(events)) { // else none yet, or another layout
    prefs.getBytes("events", events, sizeof(events));
    eventCount = min((int)prefs.getUChar("count", 0), DIAG_EVENTS);
  }
  prefs.end();
}

const char *resetName(int reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "SDIO";
  }
  return "unknown";
}

// What happened, for the log.
String eventText(const Event &e) {
  String what;
  if (e.kind == EVENT_RESET) {
    what = String("reset: ") + resetName(e.reason);
  } else if (e.kind == EVENT_WIFI) {
    what = String("WiFi drop, reason ") + e.reason + " (" + WiFi.disconnectReasonName((wifi_err_reason_t)e.reason) +
           "), rssi " + e.rssi;
  } else {
    what = String("VBand drop, server silent ") + e.reason / 10 + "." + e.reason % 10 + "s before, rssi " + e.rssi;
  }
  if (e.count > 1) what += String(", x") + e.count;
  return what;
}

// The same in one row of the keyer's display, with when.
String eventRow(const Event &e) {
  String what;
  if (e.kind == EVENT_RESET) {
    what = resetName(e.reason);
    what[0] = toupper(what[0]);
  } else if (e.kind == EVENT_WIFI) {
    what = String("WiFi r") + e.reason;
  } else {
    what = "VBand drop";
  }
  if (e.count > 1) what += String(" x") + e.count;
  if (e.boot != bootNumber) return what + " -" + (bootNumber - e.boot) + "b";
  if (e.kind == EVENT_RESET) return "Reset: " + what;
  return what + " " + (millis() / 1000 - e.uptimeS) / 60 + "m";
}

void onWifiConnected(WiFiEvent_t, WiFiEventInfo_t) {
  wifiUp = true;
}

void onWifiDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
  wifiDisconnects++;
  int reason = info.wifi_sta_disconnected.reason;
  if (wifiUp) addEvent(EVENT_WIFI, reason, info.wifi_sta_disconnected.rssi); // not each failed reconnect
  wifiUp = false;
#ifdef DIAGNOSTICS_LOG
  Log.printf("DIAG wifi disconnected reason=%d at %lus\n", reason, millis() / 1000);
#endif
}

}  // namespace

void diagnosticsBegin() {
  loadEvents();
  esp_reset_reason_t reason = esp_reset_reason();
  Log.printf("DIAG boot %u, reset: %s\n", bootNumber, resetName(reason));
  // Power-on is the usual start, and restarts are our own (OTA, the portal).
  if (reason != ESP_RST_POWERON && reason != ESP_RST_SW) addEvent(EVENT_RESET, reason, 0);
  if (eventsDirty) saveEvents();
  diagnosticsLogEvents();

  WiFi.onEvent(onWifiConnected, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  lastReportMs = millis();
  loopStartUs = micros();
}

void diagnosticsStageStart() {
  stageStartUs = micros();
}

void diagnosticsStageEnd(DiagStage stage) {
  unsigned long us = micros() - stageStartUs;
  if (us > stageMaxUs[stage]) stageMaxUs[stage] = us;
}

void diagnosticsNoteServerMessage() {
  unsigned long now = millis();
  if (lastServerMessageMs && now - lastServerMessageMs > serverGapMaxMs) {
    serverGapMaxMs = now - lastServerMessageMs;
  }
  lastServerMessageMs = now;
}

void diagnosticsNoteSmk() {
  unsigned long now = millis();
  // Gaps over 5s are the sender pausing, not the network.
  if (lastSmkMs && now - lastSmkMs < 5000 && now - lastSmkMs > smkGapMaxMs) {
    smkGapMaxMs = now - lastSmkMs;
    if (smkGapMaxMs > worstSmkGapMs) worstSmkGapMs = smkGapMaxMs;
  }
  lastSmkMs = now;
  smkCount++;
}

void diagnosticsNoteQueueDepth(int depth) {
  queueDepth = depth;
  if (depth > queueDepthMax) queueDepthMax = depth;
}

void diagnosticsNoteQueueDrop() {
  queueDrops++;
}

void diagnosticsNoteToneMs(unsigned long ms) {
  toneMs += ms;
}

void diagnosticsNoteWsDisconnect(bool unexpected) {
  wsDisconnects++;
  if (unexpected) {
    unsigned long silentMs = lastServerMessageMs ? millis() - lastServerMessageMs : 0;
    addEvent(EVENT_VBAND, (int)min(silentMs / 100, 32767UL), WiFi.RSSI());
  }
#ifdef DIAGNOSTICS_LOG
  Log.printf("DIAG ws disconnected at %lus, %lums since last server message, rssi=%d\n",
                millis() / 1000, lastServerMessageMs ? millis() - lastServerMessageMs : 0,
                WiFi.RSSI());
#endif
}

String diagnosticsSummary() {
  char rows[64];
  snprintf(rows, sizeof(rows), "%s WiFi %d|Up %lum ws%lu wifi%lu|Gap %lu.%lus lost %lu",
           FIRMWARE_VERSION, WiFi.RSSI(), millis() / 60000, wsDisconnects, wifiDisconnects,
           worstSmkGapMs / 1000, worstSmkGapMs % 1000 / 100, queueDrops);
  return rows;
}

String diagnosticsLastEvents() {
  Event copy[DIAG_EVENTS];
  portENTER_CRITICAL(&eventLock);
  memcpy(copy, events, sizeof(copy));
  uint8_t count = eventCount;
  portEXIT_CRITICAL(&eventLock);
  if (!count) return "Last drops|None kept";
  // Three rows: four don't fit in one status frame.
  String rows;
  for (int i = count - 1; i >= 0 && i >= (int)count - 3; i--) {
    if (rows.length()) rows += "|";
    rows += eventRow(copy[i]);
  }
  return rows;
}

void diagnosticsLogEvents() {
  Event copy[DIAG_EVENTS];
  portENTER_CRITICAL(&eventLock);
  memcpy(copy, events, sizeof(copy));
  uint8_t count = eventCount;
  portEXIT_CRITICAL(&eventLock);
  if (!count) Log.println("DIAG events: none kept");
  for (uint8_t i = 0; i < count; i++) {
    Log.printf("DIAG event %u/%u: boot %u at %lus, %s\n", i + 1, count, copy[i].boot,
               (unsigned long)copy[i].uptimeS, eventText(copy[i]).c_str());
  }
}

void diagnosticsLoop() {
  unsigned long nowUs = micros();
  unsigned long loopUs = nowUs - loopStartUs;
  if (loopUs > loopMaxUs) loopMaxUs = loopUs;
  loopStartUs = nowUs;

  if (eventsDirty && millis() - lastSaveMs >= DIAG_SAVE_MS) saveEvents();

#ifdef DIAGNOSTICS_LOG
  if (millis() - lastReportMs < DIAGNOSTICS_INTERVAL_MS) return;
  lastReportMs = millis();

  String stages;
  for (int i = 0; i < DIAG_STAGES; i++) {
    stages += String(" ") + STAGE_NAMES[i] + "=" + String(stageMaxUs[i] / 1000);
  }
  Log.printf("DIAG t=%lus rssi=%d heap=%u minheap=%u loopmax=%lums [max ms:%s] "
                "smk=%lu smkgap=%lums srvgap=%lums queue=%d qmax=%d drops=%lu tone=%lums "
                "wsdrops=%lu wifidrops=%lu\n",
                millis() / 1000, WiFi.RSSI(), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                loopMaxUs / 1000, stages.c_str(), smkCount, smkGapMaxMs, serverGapMaxMs,
                queueDepth, queueDepthMax, queueDrops, toneMs, wsDisconnects, wifiDisconnects);

  // Per-interval values start over; totals (drops, disconnects) keep counting.
  for (unsigned long &us : stageMaxUs) us = 0;
  loopMaxUs = 0;
  smkCount = 0;
  smkGapMaxMs = 0;
  serverGapMaxMs = 0;
  queueDepthMax = queueDepth;
  toneMs = 0;
#endif
}
