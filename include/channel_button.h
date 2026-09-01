#pragma once

// Non-blocking reader for the channel-cycling pushbutton on
// PIN_CHANNEL_BUTTON. Calls onPress() once per press (not on release).
typedef void (*ChannelButtonPressCallback)();

void channelButtonBegin();
void channelButtonLoop(ChannelButtonPressCallback onPress);
