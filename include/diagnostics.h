#pragma once

#include <Arduino.h>

// Temporary field diagnostics (DIAGNOSTICS_LOG in config.h): every
// DIAGNOSTICS_INTERVAL_MS a "DIAG ..." line on the USB serial port with
// WiFi signal, heap, the slowest pass through each part of loop(), how
// received keying arrived (count, longest gap between messages) and how
// far playback fell behind; plus a line for each WiFi or VBand drop with
// its reason. The note*() calls are cheap and harmless with it off.

void diagnosticsBegin();

// A few rows for the keyer's display ("/DIAG"): firmware commit, signal, uptime,
// disconnects, and the worst gap in someone's sending / playback drops
// since boot.
String diagnosticsSummary();

// The latest kept events for the keyer's display ("/LAST"), newest first,
// as many as fit: "Brownout -2b" (two boots ago), "WiFi r200 12m" (12
// minutes ago, reason code 200), "Reset: crash" (what started this boot).
String diagnosticsLastEvents();

// Logs the kept events, oldest first: resets (with esp_reset_reason()),
// WiFi drops (with the reason code) and unexpected VBand drops, each with
// its boot number, time since that boot and signal strength.
void diagnosticsLogEvents();
void diagnosticsLoop();

// Times one part of loop(): diagnosticsStageStart() before it,
// diagnosticsStageEnd(index) after.
enum DiagStage { DIAG_VBAND, DIAG_MEGA, DIAG_KEYER, DIAG_SIDETONE, DIAG_RXTEXT, DIAG_OTHER, DIAG_STAGES };
void diagnosticsStageStart();
void diagnosticsStageEnd(DiagStage stage);

void diagnosticsNoteServerMessage(); // anything received from VBand
void diagnosticsNoteSmk();           // someone else's keying
void diagnosticsNoteQueueDepth(int depth);
void diagnosticsNoteQueueDrop();
void diagnosticsNoteToneMs(unsigned long ms);
// Every WebSocket disconnect, including each failed reconnect attempt;
// unexpected: it was connected and nobody asked it to go (kept as an event).
void diagnosticsNoteWsDisconnect(bool unexpected);
