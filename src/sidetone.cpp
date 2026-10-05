#include <Arduino.h>
#include <math.h>
#include <soc/rtc_io_reg.h>
#include "sidetone.h"
#include "pins.h"
#include "config.h"
#include "diagnostics.h"

namespace {

constexpr int SINE_TABLE_SIZE = 256; // one full cycle; index = top 8 bits of phase
uint8_t sineTable[SINE_TABLE_SIZE];

static_assert(PIN_AUDIO_OUT == 25, "writeDac() drives DAC1, which is GPIO25");

hw_timer_t *audioTimer = nullptr;
volatile bool toneOn = false;
volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseIncrement = 0;

#ifdef AUDIO_TONE_PER_SENDER
constexpr uint16_t SENDER_TONES_HZ[] = AUDIO_SENDER_TONES_HZ;
static_assert(sizeof(SENDER_TONES_HZ) / sizeof(SENDER_TONES_HZ[0]) == RX_TEXT_MAX_SENDERS,
              "AUDIO_SENDER_TONES_HZ needs one pitch per RX_TEXT_MAX_SENDERS slot");
#endif

uint32_t toneHz(uint8_t sender) {
#ifdef AUDIO_TONE_PER_SENDER
  if (sender < RX_TEXT_MAX_SENDERS) return SENDER_TONES_HZ[sender];
#endif
  return AUDIO_TONE_HZ;
}

// One register write, from IRAM. dacWrite() re-runs the DAC pad setup
// (from flash, under a lock) on every call, which at this sample rate
// took long enough that the timer interrupt could fire again before the
// last one finished and starve loop() of CPU time entirely -- the board
// "hung" wherever loop() or setup() happened to be.
void IRAM_ATTR writeDac(uint8_t value) {
  SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, value, RTC_IO_PDAC1_DAC_S);
}

// Only runs while a tone is sounding (see startTone()/stopTone()).
void IRAM_ATTR onAudioTimer() {
  if (!toneOn) {
    writeDac(128);
    return;
  }
  writeDac(sineTable[phaseAccumulator >> 24]);
  phaseAccumulator += phaseIncrement;
}

void startTone(uint8_t sender) {
  phaseIncrement = (uint32_t)(((uint64_t)toneHz(sender) << 32) / AUDIO_SAMPLE_RATE_HZ);
  phaseAccumulator = 0;
  toneOn = true;
  timerAlarmEnable(audioTimer);
}

// The timer driver turns the alarm back on after each interrupt (it's
// auto-reload), so a sample interrupt landing just as the alarm is
// disabled can leave it running -- before toneOn, that kept the tone
// sounding until the next element finished, seconds later across a
// pause. Now such a stray interrupt only holds the DAC at mid-scale, and
// sidetoneLoop() switches the timer off again.
void stopTone() {
  toneOn = false;
  timerAlarmDisable(audioTimer);
  writeDac(128); // back to the idle mid-scale level
}

// Ring buffer of space/mark pairs awaiting playback, oldest first.
struct SpaceMark {
  unsigned long space;
  unsigned long mark;
  uint8_t sender;
};
SpaceMark queue[AUDIO_QUEUE_CAPACITY];
int queueHead = 0;
int queueCount = 0;

enum class PlaybackState { Idle, Spacing, Marking };
PlaybackState playbackState = PlaybackState::Idle;
unsigned long phaseStartMs = 0;
SpaceMark current;

// A pair's space counts from when the last mark was due to end, and
// silence that has already gone by is not played again: when the queue
// ran dry because the next element was late arriving, it starts as soon
// as it arrives rather than replaying its whole space first (which kept
// playback a full space -- up to MAX_TIME_MS -- behind after every pause).
// Back-to-back elements likewise aren't pushed later by loop() being a
// few ms late on each one. Nothing plays faster than it was sent.
unsigned long lastEndMs = 0; // when the last mark was due to end

// A mark that loop() starts more than this late (a real stall) is timed
// from now rather than cut short.
constexpr unsigned long MAX_CATCH_UP_MS = 30;

SidetonePlayedCallback playedCallback = nullptr;

}  // namespace

void sidetoneBegin() {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    float angle = 2.0f * PI * i / SINE_TABLE_SIZE;
    int8_t sample = (int8_t)(127.0f * sinf(angle));
    sineTable[i] = (uint8_t)(128 + sample);
  }
  dacWrite(PIN_AUDIO_OUT, 128); // enables the DAC pad once; writeDac() just updates its level

  audioTimer = timerBegin(0, 80, true); // 80MHz APB / 80 = 1MHz tick (1us)
  timerAttachInterrupt(audioTimer, &onAudioTimer, true);
  timerAlarmWrite(audioTimer, 1000000 / AUDIO_SAMPLE_RATE_HZ, true);
  // Alarm stays disabled until a tone starts.
  lastEndMs = millis();
}

void sidetoneSetPlayedCallback(SidetonePlayedCallback callback) {
  playedCallback = callback;
}

void sidetoneQueueSpaceMark(unsigned long space, unsigned long mark, uint8_t sender) {
  if (queueCount >= AUDIO_QUEUE_CAPACITY) {
    // Playback has fallen behind arrival -- drop the oldest queued pair
    // rather than growing unbounded or blocking the caller.
    queueHead = (queueHead + 1) % AUDIO_QUEUE_CAPACITY;
    queueCount--;
    diagnosticsNoteQueueDrop();
  }
  int tail = (queueHead + queueCount) % AUDIO_QUEUE_CAPACITY;
  queue[tail] = {space, mark, sender};
  queueCount++;
  diagnosticsNoteQueueDepth(queueCount);
}

void sidetoneLoop() {
  unsigned long now = millis();

  if (playbackState == PlaybackState::Idle) {
    if (toneOn == false && timerAlarmEnabled(audioTimer)) timerAlarmDisable(audioTimer);
    if (queueCount == 0) return;
    current = queue[queueHead];
    queueHead = (queueHead + 1) % AUDIO_QUEUE_CAPACITY;
    queueCount--;
    diagnosticsNoteQueueDepth(queueCount);
    playbackState = PlaybackState::Spacing;
    // Space starts at the last mark's end, or late enough that it ends now.
    phaseStartMs = (now - lastEndMs >= current.space) ? now - current.space : lastEndMs;
    return;
  }

  if (playbackState == PlaybackState::Spacing) {
    if (now - phaseStartMs < current.space) return;
    playbackState = PlaybackState::Marking;
    unsigned long dueMs = phaseStartMs + current.space;
    phaseStartMs = (now - dueMs > MAX_CATCH_UP_MS) ? now : dueMs;
    startTone(current.sender);
    return;
  }

  // Marking
  if (now - phaseStartMs < current.mark) return;
  stopTone();
  diagnosticsNoteToneMs(current.mark);
  playbackState = PlaybackState::Idle;
  lastEndMs = phaseStartMs + current.mark;
  if (playedCallback) playedCallback(current.sender, current.space, current.mark);
}
