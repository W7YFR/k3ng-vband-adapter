#pragma once

// Synthesizes incoming CW as a sine wave on PIN_AUDIO_OUT (one of the
// ESP32's built-in DACs) -- a different pitch for each sender (see
// AUDIO_SENDER_TONES_HZ) -- so it can feed an external audio
// circuit alongside its own sidetone.
// Sample generation runs on a hardware timer interrupt, fully
// decoupled from loop() timing, so playback stays glitch-free regardless
// of what else is running. The timer only runs while a tone is sounding.
//
// Queued space/mark pairs are replayed in arrival order at their
// original timing -- silence for `space` ms, tone for `mark` ms -- via a
// non-blocking state machine polled from sidetoneLoop().
void sidetoneBegin();
void sidetoneLoop();
// sender (a received-text slot) picks the pitch and is passed back to
// the played callback.
void sidetoneQueueSpaceMark(unsigned long space, unsigned long mark, uint8_t sender);

// Called as each queued pair finishes playing (end of its mark), e.g. to
// decode it in step with what's heard. Call once from setup().
typedef void (*SidetonePlayedCallback)(uint8_t sender, unsigned long space, unsigned long mark);
void sidetoneSetPlayedCallback(SidetonePlayedCallback callback);
