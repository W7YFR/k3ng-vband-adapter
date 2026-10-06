#pragma once

// Owns the WebSocket connection to the VBand server: connecting (or not,
// per the VB.START setting), joining the current CHANNEL_CYCLE position or
// waiting in the lobby (connected, in no channel), keeping count of who's
// in each room, dispatching incoming server messages, and sending this
// device's own keying timing.
void vbandBegin();
void vbandLoop();

// Sends a space/mark pair for this device's own keying. No-ops if not
// currently joined to a channel.
void vbandSendSpaceMark(unsigned long space, unsigned long mark);

// Advances to the next channel in CHANNEL_CYCLE and asks the server to
// join it. From the lobby, or disconnected, it (connects and) rejoins the
// channel we were last on instead.
void vbandCycleChannel();

// Joins a channel by its vbandChannelCode() ('1'-'5', 'C'); false if
// there's no such channel (vbandChannelCodeValid() checks without joining).
bool vbandJoinChannelCode(char code);
bool vbandChannelCodeValid(char code);

// Shows the current channel and who's in it on the keyer, the room counts
// in the lobby, or why VBand isn't connected.
void vbandShowRoom();

// Connect and wait in the lobby (/CON), drop the connection (/DIS), or
// leave the channel for the lobby (/LOBBY).
void vbandConnect();
void vbandDisconnect();
void vbandGoToLobby();

// Ask for fresh room counts, then show them (/ROOMS) or join whichever
// channel (1-5 or the custom room) has the most people in it (/BUSY).
void vbandShowCounts();
void vbandJoinBusiest();

// The VBand name changed: tell the server, which renames us in place.
void vbandNameChanged();

// The custom room's name changed: rejoin it if that's where we are.
void vbandCustomRoomChanged();

// True once the server has confirmed the join (CJN) for the current
// CHANNEL_CYCLE position; false from the moment a join is requested
// (including on vbandCycleChannel()) until that confirmation arrives.
bool vbandIsJoined();

// True from the first join confirmation after connecting until the
// WebSocket disconnects. Unlike vbandIsJoined(), it stays true while
// switching channels, so a channel change doesn't look like VBand going
// away and back (the keyer would otherwise flip between key lines).
bool vbandIsReady();

// Connected to the server (in a channel or the lobby).
bool vbandIsConnected();

// VBand is wanted: connected, or connecting. False when off on purpose
// (VB.START off, /DIS).
bool vbandIsOn();

// On the way somewhere: connecting, or connected and joining a channel.
// False in the lobby, once joined, and when disconnected on purpose.
bool vbandIsJoining();

// Single-character identifier for the current CHANNEL_CYCLE position,
// for the status LED's Morse announcement: '1'-'5' for the numbered
// channels, 'C' for the custom room (vbandSettingsRoom()).
char vbandChannelCode();

// Name of the current CHANNEL_CYCLE position, as the server knows it
// (e.g. "Channel 5 (ND)" or the custom room).
String vbandChannelName();

// Registers a callback invoked once per inbound SMK from someone else in
// the channel (never for this device's own keying), with who sent it and
// that transmission's original space/mark timing -- e.g. to play it back
// as audio. Call once from setup(); pass nullptr to clear it.
typedef void (*VbandRxSpaceMarkCallback)(const String &userId, const String &userName,
                                         unsigned long space, unsigned long mark);
void vbandSetRxCallback(VbandRxSpaceMarkCallback callback);
