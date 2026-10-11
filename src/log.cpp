#include <Arduino.h>
#include "log.h"
#include "mega_link.h"
#include "config.h"

LogOutput Log;

namespace {

// Each kept line, a slot each: line n is in slot n % LOG_HISTORY_LINES.
char lines[LOG_HISTORY_LINES][LOG_LINE_MAX + 1];
uint32_t linesKept = 0;   // lines ever completed; the next goes in slot linesKept % LOG_HISTORY_LINES

// The line being printed, until its '\n'.
char current[LOG_LINE_MAX + 1];
size_t currentLength = 0;

// Forwarding (main loop only, apart from the lock-protected handover).
bool forwarding = false;
uint32_t nextLine = 0;    // the line to send next
size_t nextOffset = 0;    // how much of it is already sent
uint32_t dropped = 0;     // lines overwritten before they could be sent
bool sentAny = false;     // since forwarding was turned on: until then, the history just starts later
unsigned long lastFrameMs = 0;

// Lines can come from other tasks (the power button, WiFi events).
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// A frame's text: link frames start with '$', so that can't appear, and
// control characters become spaces.
char safeChar(char c) {
  if (c == '$') return 'S';
  if ((uint8_t)c < ' ') return ' ';
  return c;
}

void keep(uint8_t c) {
  if (c == '\r') return;
  if (c == '\n') {
    current[currentLength] = 0;
    memcpy(lines[linesKept % LOG_HISTORY_LINES], current, currentLength + 1);
    linesKept++;
    currentLength = 0;
    return;
  }
  if (currentLength < LOG_LINE_MAX) current[currentLength++] = safeChar((char)c);
}

// Text that fits in one frame after "LGP," and the frame's "$" and "*XX\n".
constexpr size_t CHUNK = MEGA_LINK_MAX_FRAME - 10;

}  // namespace

size_t LogOutput::write(uint8_t c) {
  return write(&c, 1);
}

size_t LogOutput::write(const uint8_t *buffer, size_t size) {
  Serial.write(buffer, size);
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < size; i++) keep(buffer[i]);
  portEXIT_CRITICAL(&lock);
  return size;
}

void logForwardSet(bool on) {
  if (on == forwarding) return;
  portENTER_CRITICAL(&lock);
  forwarding = on;
  nextLine = linesKept > LOG_HISTORY_LINES ? linesKept - LOG_HISTORY_LINES : 0;
  nextOffset = 0;
  dropped = 0;
  sentAny = false;
  portEXIT_CRITICAL(&lock);
}

void logForwardLoop() {
  if (!forwarding || !megaLinkUp() || !megaLinkIdle()) return;
  unsigned long now = millis();
  if (now - lastFrameMs < LOG_FORWARD_SPACING_MS) return;

  char text[LOG_LINE_MAX + 1];
  bool haveLine = false;
  portENTER_CRITICAL(&lock);
  if (linesKept - nextLine > LOG_HISTORY_LINES) { // overwritten while waiting
    if (sentAny) dropped += linesKept - nextLine - LOG_HISTORY_LINES;
    nextLine = linesKept - LOG_HISTORY_LINES;
    nextOffset = 0;
  }
  if (nextLine != linesKept) {
    memcpy(text, lines[nextLine % LOG_HISTORY_LINES], sizeof(text));
    haveLine = true;
  }
  portEXIT_CRITICAL(&lock);

  if (dropped && nextOffset == 0) {
    if (megaLinkSend("LG", "(" + String(dropped) + (dropped == 1 ? " line" : " lines") + " dropped)")) dropped = 0;
    lastFrameMs = now;
    return;
  }
  if (!haveLine) return;

  size_t length = strlen(text);
  size_t part = min(length - nextOffset, CHUNK);
  bool last = nextOffset + part >= length;
  String chunk = String(text + nextOffset).substring(0, part);
  lastFrameMs = now;
  if (!megaLinkSend(last ? "LG" : "LGP", chunk)) return; // try again next time
  sentAny = true;
  if (last) {
    nextLine++;
    nextOffset = 0;
  } else {
    nextOffset += part;
  }
}
