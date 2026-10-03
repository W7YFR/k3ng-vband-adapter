#include <Arduino.h>
#include <math.h>
#include "sidetone.h"
#include "pins.h"
#include "config.h"

namespace {

constexpr int SINE_TABLE_SIZE = 256; // one full cycle; index = top 8 bits of phase
uint8_t sineTable[SINE_TABLE_SIZE];

hw_timer_t *audioTimer = nullptr;
volatile uint32_t phaseAccumulator = 0;
uint32_t phaseIncrement = 0;
volatile bool toneOn = false;

void IRAM_ATTR onAudioTimer() {
  if (toneOn) {
    dacWrite(PIN_AUDIO_OUT, sineTable[phaseAccumulator >> 24]);
    phaseAccumulator += phaseIncrement;
  } else {
    dacWrite(PIN_AUDIO_OUT, 128);
  }
}

// Ring buffer of space/mark pairs awaiting playback, oldest first.
struct SpaceMark {
  unsigned long space;
  unsigned long mark;
};
SpaceMark queue[AUDIO_QUEUE_CAPACITY];
int queueHead = 0;
int queueCount = 0;

enum class PlaybackState { Idle, Spacing, Marking };
PlaybackState playbackState = PlaybackState::Idle;
unsigned long phaseStartMs = 0;
SpaceMark current;

}  // namespace

void sidetoneBegin() {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    float angle = 2.0f * PI * i / SINE_TABLE_SIZE;
    int8_t sample = (int8_t)(127.0f * sinf(angle));
    sineTable[i] = (uint8_t)(128 + sample);
  }
  phaseIncrement = (uint32_t)(((uint64_t)AUDIO_TONE_HZ << 32) / AUDIO_SAMPLE_RATE_HZ);

  dacWrite(PIN_AUDIO_OUT, 128);

  audioTimer = timerBegin(0, 80, true); // 80MHz APB / 80 = 1MHz tick (1us)
  timerAttachInterrupt(audioTimer, &onAudioTimer, true);
  timerAlarmWrite(audioTimer, 1000000 / AUDIO_SAMPLE_RATE_HZ, true);
  timerAlarmEnable(audioTimer);
}

void sidetoneQueueSpaceMark(unsigned long space, unsigned long mark) {
  if (queueCount >= AUDIO_QUEUE_CAPACITY) {
    // Playback has fallen behind arrival -- drop the oldest queued pair
    // rather than growing unbounded or blocking the caller.
    queueHead = (queueHead + 1) % AUDIO_QUEUE_CAPACITY;
    queueCount--;
  }
  int tail = (queueHead + queueCount) % AUDIO_QUEUE_CAPACITY;
  queue[tail] = {space, mark};
  queueCount++;
}

void sidetoneLoop() {
  unsigned long now = millis();

  if (playbackState == PlaybackState::Idle) {
    if (queueCount == 0) return;
    current = queue[queueHead];
    queueHead = (queueHead + 1) % AUDIO_QUEUE_CAPACITY;
    queueCount--;
    playbackState = PlaybackState::Spacing;
    phaseStartMs = now;
    toneOn = false;
    return;
  }

  if (playbackState == PlaybackState::Spacing) {
    if (now - phaseStartMs < current.space) return;
    playbackState = PlaybackState::Marking;
    phaseStartMs = now;
    phaseAccumulator = 0;
    toneOn = true;
    return;
  }

  // Marking
  if (now - phaseStartMs < current.mark) return;
  toneOn = false;
  playbackState = PlaybackState::Idle;
}
