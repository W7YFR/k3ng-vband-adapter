#include <Arduino.h>
#include <vector>
#include "room_users.h"
#include "user_tag.h"
#include "display_events.h"

namespace {

struct User {
  String id;
  String tag;
};

String channel;
String me;
std::vector<User> users;   // the last complete list
std::vector<User> pending; // the list being received
bool listing = false;
bool known = false;

bool contains(const std::vector<User> &list, const String &id) {
  for (const User &user : list) {
    if (user.id == id) return true;
  }
  return false;
}

}  // namespace

void roomUsersReset(const String &joinedChannel, const String &myId) {
  channel = joinedChannel;
  me = myId;
  users.clear();
  pending.clear();
  listing = false;
  known = false;
}

void roomUsersListBegin(const String &listChannel) {
  if (listChannel != channel) return;
  pending.clear();
  listing = true;
}

void roomUsersListAdd(const String &listChannel, const String &userId, const String &userName) {
  if (!listing || listChannel != channel || userId == me || contains(pending, userId)) return;
  pending.push_back({userId, userTag(userName)});
}

void roomUsersListComplete(const String &listChannel) {
  if (!listing || listChannel != channel) return;
  listing = false;

  if (known) {
    for (const User &user : pending) {
      if (!contains(users, user.id)) displayUserJoined(user.tag);
    }
    for (const User &user : users) {
      if (!contains(pending, user.id)) displayUserLeft(user.tag);
    }
  }
  Serial.printf("Room %s: %u others\n", channel.c_str(), (unsigned)pending.size());
  users = pending;
  known = true;
}

bool roomUsersKnown() {
  return known;
}

String roomUsersSummary() {
  if (users.empty()) return "Nobody else here";
  String summary = String(users.size()) + " here:";
  for (const User &user : users) summary += " " + user.tag;
  return summary;
}
