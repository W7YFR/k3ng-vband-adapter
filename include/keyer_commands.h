#pragma once

// Commands keyed on the keyer: in its command mode, "/" then a word. The
// keyer first asks whether we know it ("CK,<word>", answered "CR,1" or
// "CR,0"), so a typo stays in command mode to be retried; then it leaves
// command mode and sends "CMD,<word>". Each answers with a status screen.
//
//   /WHO        current channel and who's in it
//   /CH         next channel; /CH1../CH5, /CHC (custom room) to pick one
//   /OTA        listen for an OTA update for OTA_WINDOW_MS
//   /AP         restart into the WiFi / VBand settings portal
//   /OFF        power off
//   /DIAG       signal, disconnects, worst gap since boot
//   /H          the list above
//
// It also serves the adapter's settings (adapter_settings.h) to the
// keyer's menu and CLI.
void keyerCommandsBegin();
void keyerCommandsLoop();
