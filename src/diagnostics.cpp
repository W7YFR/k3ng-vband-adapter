#include <Arduino.h>
#include <WiFi.h>
#include "diagnostics.h"
#include "config.h"

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

void onWifiDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
  wifiDisconnects++;
  Serial.printf("DIAG wifi disconnected reason=%d at %lus\n",
                info.wifi_sta_disconnected.reason, millis() / 1000);
}

}  // namespace

void diagnosticsBegin() {
#ifdef DIAGNOSTICS_LOG
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif
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

void diagnosticsNoteWsDisconnect() {
  wsDisconnects++;
#ifdef DIAGNOSTICS_LOG
  Serial.printf("DIAG ws disconnected at %lus, %lums since last server message, rssi=%d\n",
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

void diagnosticsLoop() {
  unsigned long nowUs = micros();
  unsigned long loopUs = nowUs - loopStartUs;
  if (loopUs > loopMaxUs) loopMaxUs = loopUs;
  loopStartUs = nowUs;

#ifdef DIAGNOSTICS_LOG
  if (millis() - lastReportMs < DIAGNOSTICS_INTERVAL_MS) return;
  lastReportMs = millis();

  String stages;
  for (int i = 0; i < DIAG_STAGES; i++) {
    stages += String(" ") + STAGE_NAMES[i] + "=" + String(stageMaxUs[i] / 1000);
  }
  Serial.printf("DIAG t=%lus rssi=%d heap=%u minheap=%u loopmax=%lums [max ms:%s] "
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
