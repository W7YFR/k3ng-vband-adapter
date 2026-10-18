#include <Arduino.h>
#include <time.h>
#include <vector>
#include "transcript.h"
#include "morse_decoder.h"
#include "mega_link.h"
#include "adapter_settings.h"
#include "vband_settings.h"
#include "user_tag.h"
#include "keyer.h"
#include "vband_client.h"
#include "config.h"

namespace {

// A message, or a notice (a line of its own), from when it started until
// it's recorded.
struct Message {
  String sender;          // user id; "" for a notice
  String tag;
  String text;
  String stamp;           // "[14:02Z]", from when it started
  unsigned long lastMs;   // last text, for TRANSCRIPT_MESSAGE_GAP_MS
  bool open;
};

// In the order they started. The front ones go into the recording as
// they finish, so a message that started earlier is never recorded after
// one that started later.
std::vector<Message> messages;

const char *const OWN_SENDER = "*me"; // never a VBand user id
MorseDecoder ownDecoder;

// The live stream: text waiting to go to the keyer, '\n' ending a line.
String outbox;
bool outboxLost = false;   // text didn't fit: say so before the next line
String lineSender;         // whose message the keyer's current line is
bool lineOpen = false;     // the keyer's current line is unfinished
bool streaming = false;    // as last decided in transcriptLoop()
unsigned long lastFrameMs = 0;
unsigned long lastStateMs = 0;
bool stateSent = false;    // the "UI" value the keyer last got

// The recording: whole lines, each ending '\n', oldest first, wrapping.
char record[TRANSCRIPT_RECORD_BYTES];
size_t recordStart = 0;
size_t recordUsed = 0;
uint32_t recordLines = 0;

// Text that fits in one frame after "TRP," and the frame's "$" and "*XX\n".
constexpr size_t CHUNK = MEGA_LINK_MAX_FRAME - 10;

String stamp() {
  time_t now = time(nullptr);
  if (now < 1600000000) return "[--:--Z]"; // NTP hasn't answered yet
  struct tm utc;
  gmtime_r(&now, &utc);
  char text[12];
  snprintf(text, sizeof(text), "[%02d:%02dZ]", utc.tm_hour, utc.tm_min);
  return text;
}

String lineFor(const Message &m) {
  if (!m.sender.length()) return m.stamp + " * " + m.text;
  String text = m.text;
  text.trim();
  return m.stamp + " " + m.tag + ": " + text;
}

void dropOldestLine() {
  while (recordUsed) {
    char c = record[recordStart];
    recordStart = (recordStart + 1) % TRANSCRIPT_RECORD_BYTES;
    recordUsed--;
    if (c == '\n') break;
  }
  if (recordLines) recordLines--;
}

void recordLine(const String &line) {
  size_t length = min(line.length(), (size_t)TRANSCRIPT_RECORD_BYTES / 4); // no line takes it all
  while (recordUsed + length + 1 > TRANSCRIPT_RECORD_BYTES) dropOldestLine();
  for (size_t i = 0; i <= length; i++) {
    record[(recordStart + recordUsed) % TRANSCRIPT_RECORD_BYTES] = i < length ? line[i] : '\n';
    recordUsed++;
  }
  recordLines++;
}

// Records the finished messages at the front.
void recordFinished() {
  size_t done = 0;
  while (done < messages.size() && !messages[done].open) recordLine(lineFor(messages[done++]));
  messages.erase(messages.begin(), messages.begin() + done);
}

// Queues text for the keyer, if the live stream is on.
void emit(const String &text) {
  if (!streaming) return;
  if (outboxLost && !lineOpen && outbox.length() + 40 < TRANSCRIPT_OUTBOX_BYTES) {
    outboxLost = false;
    outbox += stamp() + " * (some of the transcript was lost)\n";
  }
  if (outbox.length() + text.length() > TRANSCRIPT_OUTBOX_BYTES) {
    outboxLost = true;
    return;
  }
  outbox += text;
}

void endLine() {
  if (!lineOpen) return;
  emit("\n");
  lineOpen = false;
}

void finish(Message &m) {
  m.open = false;
  if (lineOpen && lineSender == m.sender) endLine();
}

Message *openMessage(const String &sender) {
  for (Message &m : messages) {
    if (m.open && m.sender == sender) return &m;
  }
  return nullptr;
}

void addText(const String &sender, const String &tag, String text) {
  Message *m = openMessage(sender);
  if (m && (millis() - m->lastMs > TRANSCRIPT_MESSAGE_GAP_MS || m->text.length() > TRANSCRIPT_MESSAGE_MAX)) {
    finish(*m);
    m = nullptr;
  }
  if (!m) {
    text.trim(); // nothing before the first character (a word space)
    messages.push_back({sender, tag, "", stamp(), millis(), true});
    m = &messages.back();
  }
  m->tag = tag; // names can change mid-session
  m->text += text;
  m->lastMs = millis();

  if (lineOpen && lineSender == sender) {
    emit(text);
  } else {
    // A new line: the whole message so far, so it reads whole even when
    // it was interrupted by someone else.
    endLine();
    String line = lineFor(*m);
    if (m->text.length() && m->text.endsWith(" ")) line += " "; // the next word goes on after this one
    emit(line);
    lineOpen = true;
    lineSender = sender;
  }
  recordFinished();
}

// The keyer's link frames can't carry '$' (it starts a frame).
String frameSafe(String text) {
  text.replace('$', 'S');
  return text;
}

void sendLive() {
  if (!outbox.length() || !megaLinkIdle() || millis() - lastFrameMs < TRANSCRIPT_FRAME_SPACING_MS) return;
  int newline = outbox.indexOf('\n');
  size_t part = newline >= 0 ? min((size_t)newline, CHUNK) : min(outbox.length(), CHUNK);
  bool ends = newline >= 0 && part == (size_t)newline;
  lastFrameMs = millis();
  if (!megaLinkSend(ends ? "TR" : "TRP", frameSafe(outbox.substring(0, part)))) return; // try again next time
  outbox.remove(0, part + (ends ? 1 : 0));
}

void sendState(bool on) {
  if (!megaLinkUp()) return;
  if (on == stateSent && millis() - lastStateMs < MEGA_LINK_HI_UP_MS) return;
  if (megaLinkSend("UI", on ? "1" : "0")) {
    stateSent = on;
    lastStateMs = millis();
  }
}

}  // namespace

void transcriptBegin() {
  configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC; keeps itself in step from here on
}

void transcriptLoop() {
  bool live = adapterSetting(AdapterSetting::Ui) != TRANSCRIPT_LIVE_OFF && megaLinkUp();
  if (live != streaming) {
    // Starting (or after the link was down): a fresh line, nothing stale.
    streaming = live;
    outbox = "";
    outboxLost = false;
    lineOpen = false;
  }
  sendState(live && vbandSending()); // "the transcript has our sending": not while it stays here

  // Our keying is timed right here: a character is over once the key has
  // been up longer than the gap inside one (2 dits), so it shows then
  // rather than when the next mark is over.
  ownDecoder.flushIfIdle();
  if (keyerIdleMs() > ownDecoder.ditLength() * TRANSCRIPT_OWN_CHARACTER_END_DITS) ownDecoder.endCharacter();
  String own = ownDecoder.take();
  if (own.length()) addText(OWN_SENDER, userTag(vbandSettingsName()), own);

  bool finished = false;
  for (Message &m : messages) {
    if (m.open && millis() - m.lastMs > TRANSCRIPT_MESSAGE_GAP_MS) {
      finish(m);
      finished = true;
    }
  }
  if (finished) recordFinished();

  if (streaming) sendLive();
}

void transcriptText(const String &userId, const String &tag, const String &text) {
  addText(userId, tag, text);
}

void transcriptOwnSpaceMark(unsigned long space, unsigned long mark) {
  ownDecoder.add(space, mark);
}

void transcriptNotice(const String &text) {
  messages.push_back({"", "", text, stamp(), millis(), false});
  endLine();
  emit(lineFor(messages.back()) + "\n");
  recordFinished();
}

uint32_t transcriptRecordedLines() {
  return recordLines;
}

size_t transcriptRecordedBytes() {
  return recordUsed;
}
