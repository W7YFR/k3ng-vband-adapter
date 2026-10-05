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
void diagnosticsNoteWsDisconnect();
