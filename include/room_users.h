#pragma once

#include <Arduino.h>

// Who else is in the current VBand channel, from the server's user lists
// (ULB / ULE... / ULC, in answer to "LU,<channel>"). The first complete
// list after joining becomes a summary for the join screen; after that,
// each new list is compared with the last and anyone who arrived or left
// is announced on the keyer's display. We never count ourselves.

// Joined a channel: forget the old one's users until the server lists
// this one's.
void roomUsersReset(const String &channel, const String &myId);

// One list from the server: begin, a user at a time, complete. Lists for
// any other channel are ignored.
void roomUsersListBegin(const String &channel);
void roomUsersListAdd(const String &channel, const String &userId, const String &userName);
void roomUsersListComplete(const String &channel);

// True once the current channel's first list has arrived.
bool roomUsersKnown();

// For the join screen, e.g. "2 here: RXXX K1ABC" or "Nobody else here".
String roomUsersSummary();
