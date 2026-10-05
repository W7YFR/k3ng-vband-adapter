#pragma once

// Non-blocking reader for the channel-cycling pushbutton on
// PIN_CHANNEL_BUTTON. Calls onPress() once per press, on release, and
// only for a press shorter than POWER_OFF_HOLD_MS -- holding the button
// to power off must not switch channels on the way down.
typedef void (*ChannelButtonPressCallback)();

void channelButtonBegin();
void channelButtonLoop(ChannelButtonPressCallback onPress);
