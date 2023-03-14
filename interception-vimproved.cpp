#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <linux/input.h>

using Event = input_event;
using KeyCode = unsigned short;
using Mapping = std::pair<KeyCode, KeyCode>;

const auto KEY_STROKE_UP = 0;
const auto KEY_STROKE_DOWN = 1;
const auto KEY_STROKE_REPEAT = 2;

// focus on key codes <= 0x151, covers most use cases including mouse buttons
// see: github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
const auto MAX_KEY = 0x151;

const auto KEYS = std::unordered_map<std::string, KeyCode> {
// the following extra inner macro is not necessary as sometimes, like in stackoverflow.com/q/240353:
// #define TO_STRING(X) #X

// In our case, the Keycodes are preprocessing tokens already
#define PAIR(K) { #K, K}

  PAIR(KEY_E),
  PAIR(KEY_ESC),
  PAIR(KEY_D),
  PAIR(KEY_DELETE),
  PAIR(KEY_B),
  PAIR(KEY_BACKSPACE),
  PAIR(KEY_H),
  PAIR(KEY_LEFT),
  PAIR(KEY_J),
  PAIR(KEY_DOWN),
  PAIR(KEY_K),
  PAIR(KEY_UP),
  PAIR(KEY_L),
  PAIR(KEY_RIGHT),
  PAIR(KEY_Y),
  PAIR(KEY_HOME),
  PAIR(KEY_U),
  PAIR(KEY_PAGEDOWN),
  PAIR(KEY_I),
  PAIR(KEY_PAGEUP),
  PAIR(KEY_O),
  PAIR(KEY_END),
  PAIR(KEY_1),
  PAIR(KEY_F1),
  PAIR(KEY_2),
  PAIR(KEY_F2),
  PAIR(KEY_3),
  PAIR(KEY_F3),
  PAIR(KEY_4),
  PAIR(KEY_F4),
  PAIR(KEY_5),
  PAIR(KEY_F5),
  PAIR(KEY_6),
  PAIR(KEY_F6),
  PAIR(KEY_7),
  PAIR(KEY_F7),
  PAIR(KEY_8),
  PAIR(KEY_F8),
  PAIR(KEY_9),
  PAIR(KEY_F9),
  PAIR(KEY_0),
  PAIR(KEY_F10),
  PAIR(KEY_MINUS),
  PAIR(KEY_F11),
  PAIR(KEY_EQUAL),
  PAIR(KEY_F12),
  PAIR(KEY_M),
  PAIR(KEY_MUTE),
  PAIR(KEY_COMMA),
  PAIR(KEY_VOLUMEDOWN),
  PAIR(KEY_DOT),
  PAIR(KEY_VOLUMEUP),
  PAIR(BTN_LEFT),
  PAIR(BTN_BACK),
  PAIR(BTN_RIGHT),
  PAIR(BTN_FORWARD),
  PAIR(KEY_E),
  PAIR(KEY_ESC),
  PAIR(KEY_D),
  PAIR(KEY_DELETE),
  PAIR(KEY_B),
  PAIR(KEY_BACKSPACE),
  PAIR(KEY_H),
  PAIR(KEY_LEFT),
  PAIR(KEY_J),
  PAIR(KEY_DOWN),
  PAIR(KEY_K),
  PAIR(KEY_UP),
  PAIR(KEY_L),
  PAIR(KEY_RIGHT),
  PAIR(KEY_Y),
  PAIR(KEY_HOME),
  PAIR(KEY_U),
  PAIR(KEY_PAGEDOWN),
  PAIR(KEY_I),
  PAIR(KEY_PAGEUP),
  PAIR(KEY_O),
  PAIR(KEY_END),
  PAIR(KEY_1),
  PAIR(KEY_F1),
  PAIR(KEY_2),
  PAIR(KEY_F2),
  PAIR(KEY_3),
  PAIR(KEY_F3),
  PAIR(KEY_4),
  PAIR(KEY_F4),
  PAIR(KEY_5),
  PAIR(KEY_F5),
  PAIR(KEY_6),
  PAIR(KEY_F6),
  PAIR(KEY_7),
  PAIR(KEY_F7),
  PAIR(KEY_8),
  PAIR(KEY_F8),
  PAIR(KEY_9),
  PAIR(KEY_F9),
  PAIR(KEY_0),
  PAIR(KEY_F10),
  PAIR(KEY_MINUS),
  PAIR(KEY_F11),
  PAIR(KEY_EQUAL),
  PAIR(KEY_F12),
  PAIR(KEY_M),
  PAIR(KEY_MUTE),
  PAIR(KEY_COMMA),
  PAIR(KEY_VOLUMEDOWN),
  PAIR(KEY_DOT),
  PAIR(KEY_VOLUMEUP),
  PAIR(BTN_LEFT),
  PAIR(BTN_BACK),
  PAIR(BTN_RIGHT),
  PAIR(BTN_FORWARD),
  PAIR(KEY_SYSRQ),
  PAIR(KEY_CONTEXT_MENU),

#undef PAIR
};

const auto DEFAULT_MAPPINGS = std::vector<Mapping>{
  // special chars
  {KEY_E, KEY_ESC},
  {KEY_D, KEY_DELETE},
  {KEY_B, KEY_BACKSPACE},

  // vim home row
  {KEY_H, KEY_LEFT},
  {KEY_J, KEY_DOWN},
  {KEY_K, KEY_UP},
  {KEY_L, KEY_RIGHT},

  // vim above home row
  {KEY_Y, KEY_HOME},
  {KEY_U, KEY_PAGEDOWN},
  {KEY_I, KEY_PAGEUP},
  {KEY_O, KEY_END},

  // number row to F keys
  {KEY_1, KEY_F1},
  {KEY_2, KEY_F2},
  {KEY_3, KEY_F3},
  {KEY_4, KEY_F4},
  {KEY_5, KEY_F5},
  {KEY_6, KEY_F6},
  {KEY_7, KEY_F7},
  {KEY_8, KEY_F8},
  {KEY_9, KEY_F9},
  {KEY_0, KEY_F10},
  {KEY_MINUS, KEY_F11},
  {KEY_EQUAL, KEY_F12},

  // xf86 audio
  {KEY_M, KEY_MUTE},
  {KEY_COMMA, KEY_VOLUMEDOWN},
  {KEY_DOT, KEY_VOLUMEUP},

  // mouse navigation
  {BTN_LEFT, BTN_BACK},
  {BTN_RIGHT, BTN_FORWARD},

  // special chars
  {KEY_E, KEY_ESC},
  {KEY_D, KEY_DELETE},
  {KEY_B, KEY_BACKSPACE},

  // vim home row
  {KEY_H, KEY_LEFT},
  {KEY_J, KEY_DOWN},
  {KEY_K, KEY_UP},
  {KEY_L, KEY_RIGHT},

  // vim above home row
  {KEY_Y, KEY_HOME},
  {KEY_U, KEY_PAGEDOWN},
  {KEY_I, KEY_PAGEUP},
  {KEY_O, KEY_END},

  // number row to F keys
  {KEY_1, KEY_F1},
  {KEY_2, KEY_F2},
  {KEY_3, KEY_F3},
  {KEY_4, KEY_F4},
  {KEY_5, KEY_F5},
  {KEY_6, KEY_F6},
  {KEY_7, KEY_F7},
  {KEY_8, KEY_F8},
  {KEY_9, KEY_F9},
  {KEY_0, KEY_F10},
  {KEY_MINUS, KEY_F11},
  {KEY_EQUAL, KEY_F12},

  // xf86 audio
  {KEY_M, KEY_MUTE},
  {KEY_COMMA, KEY_VOLUMEDOWN},
  {KEY_DOT, KEY_VOLUMEUP},

  // mouse navigation
  {BTN_LEFT, BTN_BACK},
  {BTN_RIGHT, BTN_FORWARD},

  // FIXME: this is not working, even though `wev` says keycode 99 is Print
  // PrtSc -> Context Menu
  {KEY_SYSRQ, KEY_CONTEXT_MENU}
};

const auto syn = new Event{
  .time = {.tv_sec = 0, .tv_usec = 0},
  .type = EV_SYN,
  .code = SYN_REPORT,
  .value = KEY_STROKE_UP
};

auto readEvent(Event *e) -> int {
  return std::fread(e, sizeof(Event), 1, stdin) == 1;
}

auto writeEvent(const Event *event) {
  if (std::fwrite(event, sizeof(Event), 1, stdout) != 1) {
    std::exit(EXIT_FAILURE);
  }
}

auto writeEvents(const std::vector<Event>& events) {
  if (std::fwrite(events.data(), sizeof(Event), events.size(), stdout) != events.size()) {
    std::exit(EXIT_FAILURE);
  }
}

// TODO: convert to a map cache to reduce amount of memory allocations
auto buildEvent(int direction, KeyCode keycode) -> Event {
  return {
    .time = {.tv_sec = 0, .tv_usec = 0},
    .type = EV_KEY,
    .code = keycode,
    .value = direction
  };
}

auto buildEventUp(KeyCode keycode) -> Event {
  return buildEvent(KEY_STROKE_UP, keycode);
}

auto writeCombo(KeyCode keycode) {
  auto key_down = Event{
    .time = {.tv_sec = 0, .tv_usec = 0},
    .type = EV_KEY,
    .code = keycode,
    .value = KEY_STROKE_DOWN
  };

  auto key_up = Event{
    .time = {.tv_sec = 0, .tv_usec = 0},
    .type = EV_KEY,
    .code = keycode,
    .value = KEY_STROKE_UP
  };

  writeEvents({key_down, *syn, key_up});
}

// Intercepted key specification
class InterceptedKey {
public:
  enum class State{START, INTERCEPTED_KEY_HELD, OTHER_KEY_HELD};

  static auto isModifier(int key) -> int {
    switch (key) {
      default: return 0;
      case KEY_LEFTSHIFT:
      case KEY_RIGHTSHIFT:
      case KEY_LEFTCTRL:
      case KEY_RIGHTCTRL:
      case KEY_LEFTALT:
      case KEY_RIGHTALT:
      case KEY_LEFTMETA:
      case KEY_RIGHTMETA:
      // TODO: handle capslock as not modifier?
      case KEY_CAPSLOCK: return 1;
    }
  }

  InterceptedKey(KeyCode intercepted, KeyCode tapped)
    :_intercepted{intercepted}, _tapped{tapped} { }

  auto matches(KeyCode code) -> bool { return _intercepted == code; }

  auto getState() -> State { return _state; }

  auto process(Event *input) -> bool {
    switch (_state) {
      case State::START: return processStart(input);
      case State::INTERCEPTED_KEY_HELD: return processInterceptedHeld(input);
      case State::OTHER_KEY_HELD: return processOtherKeyHeld(input);
      default: throw std::exception();
    }
  }

protected:
  auto processStart(Event *input) -> bool {
    if (matches(input->code) && input->value == KEY_STROKE_DOWN) {
      _shouldEmitTapped = true;
      _state = State::INTERCEPTED_KEY_HELD;
      return false;
    }
    return true;
  };

  virtual auto processInterceptedHeld(Event* input) -> bool = 0;
  virtual auto processOtherKeyHeld(Event* input) -> bool = 0;

  KeyCode _intercepted;
  KeyCode _tapped;
  State _state;
  bool _shouldEmitTapped = true;
};

class InterceptedKeyLayer : public InterceptedKey {
private:
  KeyCode* _map;
  std::set<KeyCode>* _heldKeys = new std::set<KeyCode>();

protected:
  auto map(Event* input) -> Event {
    auto result = *input;
    result.code = _map[input->code];
    return result;
  }

  auto processInterceptedHeld(Event *input) -> bool override {
    if (matches(input->code) && input->value != KEY_STROKE_UP) {
      return false; // don't emit anything
    }

    if (matches(input->code)) { // && stroke up
      // TODO: find a way to have a mouse click mark the key as intercepted
      // or just make it time based
      // _shouldEmitTapped &= mouse clicked || timeout;

      if (_shouldEmitTapped) {
        writeCombo(_tapped);
        _shouldEmitTapped = false;
      }

      _state = State::START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) {
      // any other key

      // NOTE: if we don't blindly set _shouldEmitTapped to false on any
      // keypress, we can type faster because only in case of mapped key down,
      // the intercepted key will not be emitted - useful for scenario:
      // L_DOWN, SPACE_DOWN, A_DOWN, L_UP, A_UP, SPACE_UP
      _shouldEmitTapped &= !hasMapped(input->code) && !InterceptedKey::isModifier(input->code);

      if (hasMapped(input->code)) {
        _heldKeys->insert(input->code);
        auto mapped = map(input);
        writeEvent(&mapped);
        _state = InterceptedKey::State::OTHER_KEY_HELD;
        return false;
      }
    }

    return true;
  }

  auto processOtherKeyHeld(Event *input) -> bool override {
    if (input->code == _intercepted && input->value != KEY_STROKE_UP) {
      return false;
    }
    if (input->value == KEY_STROKE_DOWN
        && _heldKeys->find(input->code) != _heldKeys->end()) {
      return false;
    }

    auto shouldEmitInput = true;
    if (input->value == KEY_STROKE_UP) {
      // one of mapped held keys goes up
      if (_heldKeys->find(input->code) != _heldKeys->end()) {
        auto mapped = map(input);
        writeEvent(&mapped);
        _heldKeys->erase(input->code);
        if (_heldKeys->empty()) {
          _state = InterceptedKey::State::INTERCEPTED_KEY_HELD;
        }
        shouldEmitInput = false;
      } else {
        // key that was not mapped & held goes up
        if (matches(input->code)) {
          auto held_keys_up = std::vector<Event>{};
          for (auto held_key_code : *_heldKeys) {
            auto held_key_up = buildEventUp(_map[held_key_code]);
            held_keys_up.push_back(held_key_up);
            held_keys_up.push_back(*syn);
          }

          writeEvents(held_keys_up);
          _heldKeys->clear();
          _state = InterceptedKey::State::START;
          shouldEmitInput = false;
        }
      }
    } else { // KEY_STROKE_DOWN or KEY_STROKE_REPEAT
      if (hasMapped(input->code)) {
        auto mapped = map(input);
        writeEvent(&mapped);
        if (input->value == KEY_STROKE_DOWN) {
          _heldKeys->insert(input->code);
        }
        shouldEmitInput = false;
      }
    }
    return shouldEmitInput;
  }

public:
  InterceptedKeyLayer(KeyCode intercepted, KeyCode tapped):
    InterceptedKey(intercepted, tapped), _map{new KeyCode[MAX_KEY]{0}} {}

  ~InterceptedKeyLayer() { delete _map; }


  auto add_mappings(const std::vector<Mapping>& mappings) {
    for (const auto& [from, to] : mappings) {
      _map[from] = to;
    }
  }

  auto hasMapped(KeyCode from) -> bool { return _map[from] != 0; }
};

class InterceptedKeyModifier : public InterceptedKey {
protected:
  KeyCode _modifier;

  bool processInterceptedHeld(Event *input) override {
    if (matches(input->code) && input->value != KEY_STROKE_UP) {
      return false;
    }

    bool shouldEmitInput = true;
    if (matches(input->code)) { // && stroke up
      if (_shouldEmitTapped) {
        writeCombo(_tapped);
      } else { // intercepted is mapped to modifier and key stroke up
        Event *modifier_up = new Event(*input);
        modifier_up->code = _modifier;
        writeEvent(modifier_up);
        delete modifier_up;
      }

      _state = State::START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) { // any other than intercepted
      if (_shouldEmitTapped) { // on first non-matching input after a
                                     // matching input
        auto modifier_down = new Event(*input);
        modifier_down->code = _modifier;

        // for some reason, need to push "syn" after modifier here
        writeEvents({*modifier_down, *syn});

        _shouldEmitTapped = false;
        return true; // gotta emit input event independently so we can process layer+modifier+input together
      }
    }

    return shouldEmitInput;
  }

  auto processOtherKeyHeld([[maybe_unused]] Event *input) -> bool override { return true; }

public:
  InterceptedKeyModifier(KeyCode intercepted, KeyCode tapped, KeyCode modifier) : InterceptedKey(intercepted, tapped) {
    if (!InterceptedKey::isModifier(modifier)) {
      throw std::invalid_argument("Specified wrong modifier key");
    }
    _modifier = modifier;
  }
};

auto initInterceptedKeys(const std::vector<Mapping>& mappings) -> std::vector<InterceptedKey*> {
  // tap space for space, hold for layer mapping
  auto space = new InterceptedKeyLayer(KEY_SPACE, KEY_SPACE);
  space->add_mappings(mappings);

  // tap caps for esc, hold for ctrl
  auto caps = new InterceptedKeyModifier(KEY_CAPSLOCK, KEY_ESC, KEY_LEFTCTRL);

  // tap enter for enter, hold for ctrl
  auto enter = new InterceptedKeyModifier(KEY_ENTER, KEY_ENTER, KEY_RIGHTCTRL);

  // NOTE: modifier keys must go first because layerKey.processInterceptedHeld
  // emits mapped key as soon as the for loop calls layerKey.process..
  // if that process is run before modifierKey.process, the modifier key will
  // not be emitted
  auto interceptedKeys = std::vector<InterceptedKey*>{caps, enter, space};
  return interceptedKeys;
}

auto read_mappings_or_default(int argc, char** argv) -> std::vector<Mapping> {
  if (argc < 2) return DEFAULT_MAPPINGS;
  try {
    auto file = std::ifstream(argv[1]);
    auto mappings = std::vector<Mapping>();
    for (auto line = std::string(); std::getline(file, line);) {
      auto from = std::string();
      auto to = std::string();
      std::istringstream(line) >> from >> to;
      mappings.push_back(Mapping{KEYS.at(from), KEYS.at(to)});
    }
    return mappings;
  } catch (...) {
    return DEFAULT_MAPPINGS;
  }
}

auto main(int argc, char** argv) -> int {
  std::setbuf(stdin, NULL);
  std::setbuf(stdout, NULL);

  auto intercepted_keys = initInterceptedKeys(read_mappings_or_default(argc, argv));

  auto process_intercepted_keys = [&](auto input) {
    return std::ranges::all_of(intercepted_keys, [&](auto k){return k->process(input);});
  };

  for (auto input = Event(); readEvent(&input);) {
    if (input.type == EV_MSC && input.code == MSC_SCAN) continue;
    if (input.type != EV_KEY || process_intercepted_keys(&input)) {
      writeEvent(&input);
    }
  }
}
