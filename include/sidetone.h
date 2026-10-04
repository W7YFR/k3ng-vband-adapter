#pragma once

// Synthesizes incoming CW as an AUDIO_TONE_HZ sine wave on PIN_AUDIO_OUT
// (one of the ESP32's built-in DACs), so it can feed an external audio
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
void sidetoneQueueSpaceMark(unsigned long space, unsigned long mark);
