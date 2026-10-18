#include <Arduino.h>
#include "keyer_menu.h"
#include "keyer_commands.h"
#include "adapter_settings.h"
#include "mega_link.h"

namespace {

// The lists: 'V' VBand itself, 'c' its Commands and 'v' its Settings, and
// under each the lists named by MENU_GROUP_* letters: upper case under
// Settings, lower case under Commands.
const char SETTINGS_LISTS[] = "RADLH";
const char COMMANDS_LISTS[] = "radlh";

// Frames go out at most this often, so the keyer's 64-byte receive buffer
// keeps up (one full row is about 30 bytes, 8 ms at 38400 baud).
constexpr unsigned long FRAME_SPACING_MS = 12;
// While the menu is open, rows are checked this often for values that
// changed by themselves (the channel, Connect / Disconnect).
constexpr unsigned long REFRESH_MS = 1000;
// With no key for this long, the keyer may have gone without saying (it
// restarted): stop refreshing until the next key.
constexpr unsigned long IDLE_MS = 120000;
constexpr int MAX_SCREEN_ROWS = 7;

struct Row {
  char type;   // 'k' < Back, 'g' opens a list, 's' a setting, 'a' a command
  char target; // 'g': the list it opens
  int item;    // 's': the setting's index; 'a': the command's
};

int columns = 18;
int screenRows = 3; // under the title
bool isOpen = false;
bool paused = false; // the keyer is showing a command's answer or reading a word: nothing until it asks
unsigned long lastKeyMs = 0;
char list = 'V';
int cursor = 1;
int top = 0;
int backCursor[3] = {1, 1, 1}; // where the cursor goes on going back to a list, by its depth
bool editing = false;
int editValue = 0;
const char *note = nullptr; // in place of the title, until the next key
bool beep = false;

// What the keyer still needs: the title, rows (by screen row), the mark.
bool titleDirty = false;
bool rowDirty[MAX_SCREEN_ROWS] = {};
bool cursorDirty = false;
String rowSent[MAX_SCREEN_ROWS]; // what each row last went out as
unsigned long lastFrameMs = 0;
unsigned long lastRefreshMs = 0;

const char *listName(char l) {
  switch (l) {
    case 'V': return "VBand";
    case 'v': return "VBand Settings";
    case 'c': return "VBand Commands";
  }
  switch (toupper(l)) {
    case MENU_GROUP_ROOMS: return "Rooms";
    case MENU_GROUP_SOUND: return "Sound";
    case MENU_GROUP_DISPLAY: return "Display";
    case MENU_GROUP_LED: return "LED";
    case MENU_GROUP_DEVICE: return "Device";
  }
  return "VBand";
}

char parentOf(char l) {
  if (l == 'V') return 0;
  if (l == 'c' || l == 'v') return 'V';
  return isupper(l) ? 'v' : 'c';
}

int depthOf(char l) {
  int depth = 0;
  while ((l = parentOf(l))) depth++;
  return depth;
}

const char *childrenOf(char l) {
  if (l == 'V') return "cv";
  if (l == 'v') return SETTINGS_LISTS;
  if (l == 'c') return COMMANDS_LISTS;
  return "";
}

// The list a setting goes in among the settings ('v' or upper case), or
// 0 if it's listed with the commands instead (a lower-case group).
char settingList(int i) {
  char group = adapterSettingInfo(i).group;
  if (islower(group)) return 0;
  return group == MENU_GROUP_NONE ? 'v' : group;
}

// The list a command goes in. One that's a setting shown among them takes
// its setting's group ('c' for Commands itself, 'r' for its Rooms).
char actionList(int a) {
  const KeyerAction &action = keyerAction(a);
  if (!action.label) {
    char group = adapterSettingInfo(adapterSettingFind(action.word)).group;
    return islower(group) ? group : 'c';
  }
  return action.group == MENU_GROUP_NONE ? 'c' : (char)tolower(action.group);
}

// Item n of a list's own settings or commands; false past the last.
bool itemAt(char l, int n, Row &row) {
  if (l == 'V') return false;
  if (l == 'v' || isupper(l)) {
    for (int i = 0; i < adapterSettingsCount(); i++) {
      if (settingList(i) == l && n-- == 0) {
        row = {'s', 0, i};
        return true;
      }
    }
    return false;
  }
  for (int a = 0; a < keyerActionCount(); a++) {
    if (!keyerActionListed(a) || actionList(a) != l || n-- != 0) continue;
    if (keyerAction(a).label) {
      row = {'a', 0, a};
    } else {
      row = {'s', 0, adapterSettingFind(keyerAction(a).word)};
    }
    return true;
  }
  return false;
}

bool hasItems(char l) {
  Row row;
  return l == 'c' || l == 'v' || itemAt(l, 0, row);
}

// Row n of a list: "< Back", its own settings or commands, then the lists
// it opens.
bool rowAt(char l, int n, Row &row) {
  if (n == 0) {
    row = {'k', 0, 0};
    return true;
  }
  n--;
  int items = 0;
  Row item;
  while (itemAt(l, items, item)) items++;
  if (n < items) return itemAt(l, n, row);
  n -= items;
  for (const char *child = childrenOf(l); *child; child++) {
    if (!hasItems(*child)) continue;
    if (n-- == 0) {
      row = {'g', *child, 0};
      return true;
    }
  }
  return false;
}

int rowCount(char l) {
  int count = 0;
  Row row;
  while (rowAt(l, count, row)) count++;
  return count;
}

// The first row shown when a list opens: "< Back" scrolled off the top when
// the rest of the list fills the screen.
int openingTop(char l) {
  return rowCount(l) - 1 >= screenRows ? 1 : 0;
}

// Keeps the cursor in the list and on screen.
void settle() {
  int count = rowCount(list);
  if (cursor >= count) cursor = count > 1 ? 1 : 0;
  if (cursor < top) top = cursor;
  if (cursor >= top + screenRows) top = cursor - screenRows + 1;
}

void dirtyAll() {
  titleDirty = true;
  for (int r = 0; r < screenRows; r++) rowDirty[r] = true;
  cursorDirty = true;
}

void dirtyCursorRow() {
  if (cursor >= top && cursor < top + screenRows) rowDirty[cursor - top] = true;
}

String rowLine(int n) {
  Row row;
  if (!rowAt(list, n, row)) return "";
  String line = n == cursor ? (editing ? "*" : ">") : " ";
  String value;
  int gap = 1;
  switch (row.type) {
    case 'k':
      line += "< Back";
      break;
    case 'g':
      line += list == 'V' ? (row.target == 'c' ? "Commands" : "Settings") : listName(row.target);
      value = ">";
      break;
    case 'a':
      line += keyerAction(row.item).label;
      break;
    case 's': {
      // "Name:", so its value reads as one
      line += String(adapterSettingInfo(row.item).label) + ":";
      gap = 0;
      value = adapterSettingShow(row.item, editing && n == cursor ? editValue : adapterSettingValue(row.item));
      break;
    }
  }
  if ((int)(line.length() + gap + value.length()) > columns) {
    value = value.substring(0, max(0, columns - (int)line.length() - gap));
  }
  while ((int)(line.length() + value.length()) < columns) line += ' ';
  line += value;
  return line.substring(0, columns);
}

void setNote(const char *text, bool withBeep) {
  note = text;
  beep = withBeep;
  titleDirty = true;
}

void open(int keyerColumns, int keyerRows) {
  if (keyerColumns > 0) columns = keyerColumns;
  if (keyerRows > 1) screenRows = min(keyerRows - 1, MAX_SCREEN_ROWS);
  isOpen = true;
  paused = false;
  list = 'V';
  cursor = 1;
  top = openingTop(list);
  editing = false;
  note = nullptr;
  for (int &b : backCursor) b = 1;
  dirtyAll();
}

void close() {
  isOpen = false;
  editing = false;
  titleDirty = cursorDirty = false;
  for (bool &d : rowDirty) d = false;
}

void enter(char l) {
  backCursor[depthOf(list)] = cursor;
  list = l;
  cursor = 1;
  top = openingTop(list);
  dirtyAll();
}

void back() {
  char parent = parentOf(list);
  if (!parent) {
    megaLinkSend("MX", "");
    close();
    return;
  }
  list = parent;
  cursor = backCursor[depthOf(list)];
  top = openingTop(list);
  settle();
  dirtyAll();
}

// Saves a number setting; a note if it didn't take (a channel that
// couldn't be joined).
void save(const Row &row, int value) {
  if (adapterSettingSet(adapterSettingInfo(row.item).key, String(value)) < 0) setNote("Not changed", true);
  dirtyCursorRow();
}

void moveCursor(int to) {
  int oldTop = top;
  cursor = to;
  settle();
  if (top != oldTop) {
    for (int r = 0; r < screenRows; r++) rowDirty[r] = true;
  }
}

void key(char k) {
  Row row;
  int count = rowCount(list);
  settle();
  rowAt(list, cursor, row);
  AdapterSettingInfo info = row.type == 's' ? adapterSettingInfo(row.item) : AdapterSettingInfo{};

  switch (k) {
    case 'E': // down, the next option, or a lower value
    case 'T': // up, the option before, or a higher value
      if (editing) {
        if (info.type == 'e') { // options go down the list as rows do, and wrap around
          int options = info.max - info.min + 1;
          editValue = info.min + (editValue - info.min + (k == 'E' ? 1 : options - 1)) % options;
        } else {
          editValue = constrain(editValue + (k == 'T' ? info.step : -info.step), info.min, info.max);
        }
        dirtyCursorRow();
      } else {
        moveCursor(k == 'E' ? (cursor + 1) % count : (cursor ? cursor - 1 : count - 1));
      }
      break;
    case 'R': // open, toggle, run or save
      if (editing) {
        editing = false;
        save(row, editValue);
      } else if (row.type == 'k') {
        back();
      } else if (row.type == 'g') {
        enter(row.target);
      } else if (row.type == 'a') {
        megaLinkSend("MA", "");
        paused = true;
        keyerCommandRun(keyerAction(row.item).word);
      } else if (info.type == 'b') {
        save(row, !adapterSettingValue(row.item));
      } else if (info.type == 's') {
        megaLinkSend("MI", info.label);
        paused = true;
      } else {
        editing = true;
        editValue = adapterSettingValue(row.item);
        dirtyCursorRow();
      }
      break;
    case 'B': // back a level, or cancel a change
      if (editing) {
        editing = false;
        dirtyCursorRow();
      } else {
        back();
      }
      break;
  }
  cursorDirty = true; // every key gets an answer, so the keyer knows we're here
}

// The keyed word for a text setting.
void word(const String &text) {
  Row row;
  paused = false;
  dirtyAll();
  if (!text.length() || !rowAt(list, cursor, row) || row.type != 's') return;
  if (adapterSettingSet(adapterSettingInfo(row.item).key, text) < 0) setNote("Not changed", true);
}

// Rows whose text changed by themselves, and a list that grew or shrank.
void refresh() {
  int oldTop = top;
  settle();
  if (top != oldTop) {
    dirtyAll();
    return;
  }
  for (int r = 0; r < screenRows; r++) { // the mark's column aside: MC moves that
    if (rowLine(top + r).substring(1) != rowSent[r].substring(1)) rowDirty[r] = true;
  }
}

bool sendOne() {
  if (titleDirty) {
    if (!megaLinkSend("MH", String(beep ? "1," : "0,") + (note ? note : listName(list)))) return false;
    titleDirty = beep = false;
    return true;
  }
  for (int r = 0; r < screenRows; r++) {
    if (!rowDirty[r]) continue;
    String line = rowLine(top + r);
    if (!megaLinkSend("MR", String(r + 1) + "," + line)) return false;
    rowDirty[r] = false;
    rowSent[r] = line;
    return true;
  }
  if (cursorDirty) {
    if (!megaLinkSend("MC", String(cursor - top + 1) + "," + (editing ? "1" : "0"))) return false;
    cursorDirty = false;
    return true;
  }
  return false;
}

}  // namespace

bool keyerMenuFrame(const String &type, const String &fields) {
  if (type == "MW") {
    lastKeyMs = millis();
    if (isOpen) word(fields);
    return true;
  }
  if (type != "MK" || !fields.length()) return false;
  lastKeyMs = millis();
  char k = toupper(fields[0]);
  if (k == 'O') {
    int comma = fields.indexOf(',');
    int second = comma < 0 ? -1 : fields.indexOf(',', comma + 1);
    open(comma < 0 ? 0 : fields.substring(comma + 1).toInt(), second < 0 ? 0 : fields.substring(second + 1).toInt());
  } else if (k == 'X') {
    close();
  } else if (!isOpen) { // we restarted while it was open: start again at the top
    open(0, 0);
  } else {
    paused = false;
    if (note) {
      note = nullptr;
      titleDirty = true;
    }
    if (k == 'P') {
      settle();
      dirtyAll();
    } else {
      key(k);
    }
  }
  return true;
}

void keyerMenuLoop() {
  if (!isOpen) return;
  if (!megaLinkUp()) {
    close();
    return;
  }
  if (paused) return;
  if (!editing && millis() - lastKeyMs < IDLE_MS && millis() - lastRefreshMs >= REFRESH_MS) {
    lastRefreshMs = millis();
    refresh();
  }
  if (millis() - lastFrameMs < FRAME_SPACING_MS) return;
  if (sendOne()) lastFrameMs = millis();
}
