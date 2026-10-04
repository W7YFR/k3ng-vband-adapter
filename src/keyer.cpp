#include <Arduino.h>
#include "keyer.h"
#include "pins.h"
#include "config.h"

namespace {

// Key line changes are timestamped in an interrupt the moment they
// happen and queued for keyerLoop(). Watching the line from loop()
// instead loses whatever happens while loop() is held up -- sending to
// VBand waits on the network, so on a slow connection a whole dit could
// start and end unseen, and every edge's timing slipped by however long
// loop() was late.

struct KeyEdge {
  uint32_t atUs;
  bool down;
};

constexpr uint8_t EDGE_QUEUE_SIZE = 128; // a few seconds of fast keying if loop() stalls
volatile KeyEdge edges[EDGE_QUEUE_SIZE];
volatile uint8_t edgeHead = 0; // next slot the interrupt writes
volatile uint8_t edgeTail = 0; // next slot keyerLoop() reads

constexpr uint32_t DEBOUNCE_US = DEBOUNCE_MS * 1000UL;

bool keyDown = false;          // as of the edges processed so far
uint32_t downStartUs = 0;
uint32_t releaseUs = 0;        // end of the last mark reported
unsigned long releaseMs = 0;   // same moment, for spaces too long for micros()
bool markPending = false;      // key released, waiting out DEBOUNCE_US before reporting
uint32_t pendingUpUs = 0;

bool lineActive() {
  return (digitalRead(PIN_KEY) == HIGH) == KEY_ACTIVE_HIGH;
}

void IRAM_ATTR onKeyEdge() {
  uint8_t next = (edgeHead + 1) % EDGE_QUEUE_SIZE;
  if (next == edgeTail) return; // queue full; keyerLoop() is far behind
  edges[edgeHead].atUs = micros();
  edges[edgeHead].down = (digitalRead(PIN_KEY) == HIGH) == KEY_ACTIVE_HIGH;
  edgeHead = next;
}

unsigned long roundToMs(uint32_t us) {
  return (us + 500) / 1000;
}

void reportMark(KeyerSpaceMarkCallback onSpaceMark) {
  markPending = false;
  // micros() wraps every ~71 minutes, so the gap is checked in millis()
  // first; only a gap under MAX_TIME_MS is measured in micros().
  unsigned long downStartMs = millis() - roundToMs(micros() - downStartUs);
  unsigned long space = (downStartMs - releaseMs >= MAX_TIME_MS)
                            ? MAX_TIME_MS
                            : min(roundToMs(downStartUs - releaseUs), (unsigned long)MAX_TIME_MS);
  unsigned long mark = min(roundToMs(pendingUpUs - downStartUs), (unsigned long)MAX_TIME_MS);
  releaseUs = pendingUpUs;
  releaseMs = millis() - roundToMs(micros() - pendingUpUs);
  onSpaceMark(space, mark);
}

}  // namespace

void keyerBegin() {
  // Keying line has its own external divider biasing it, so plain INPUT --
  // an internal pull-up here would skew the divider math.
  pinMode(PIN_KEY, INPUT);
  releaseMs = millis() - MAX_TIME_MS; // the first mark's space counts as a long gap
  keyDown = lineActive();
  downStartUs = micros();
  attachInterrupt(digitalPinToInterrupt(PIN_KEY), onKeyEdge, CHANGE);
}

void keyerLoop(KeyerSpaceMarkCallback onSpaceMark) {
  while (edgeTail != edgeHead) {
    KeyEdge edge = {edges[edgeTail].atUs, edges[edgeTail].down};
    edgeTail = (edgeTail + 1) % EDGE_QUEUE_SIZE;
    if (edge.down == keyDown) continue; // two edges too close to read apart; nothing changed

    keyDown = edge.down;
    if (edge.down) {
      if (markPending && edge.atUs - pendingUpUs < DEBOUNCE_US) {
        markPending = false; // a blip in the middle of a mark; it carries on
        continue;
      }
      if (markPending) reportMark(onSpaceMark);
      downStartUs = edge.atUs;
    } else if (edge.atUs - downStartUs < DEBOUNCE_US) {
      // A blip on an idle line, not keying.
    } else {
      markPending = true; // report once the line has stayed up long enough
      pendingUpUs = edge.atUs;
    }
  }

  if (markPending && micros() - pendingUpUs >= DEBOUNCE_US) reportMark(onSpaceMark);
}
