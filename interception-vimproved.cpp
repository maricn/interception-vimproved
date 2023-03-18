#include <algorithm>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdlib>

#include <filesystem>
#include <linux/input.h>

#include <yaml-cpp/yaml.h>

class InterceptedKeyModifier;
class InterceptedKeyLayer;

using Event = input_event;
using KeyCode = unsigned short;
using Mapping = std::unordered_map<KeyCode, KeyCode>;
using Layers = std::vector<InterceptedKeyLayer>;
using Modifiers = std::vector<InterceptedKeyModifier>;

// NOTE: modifier keys must go first because layerKey.processInterceptedHeld
// emits mapped key as soon as the for loop calls layerKey.process..
// if that process is run before modifierKey.process, the modifier key will
// not be emitted
struct Configuration {Modifiers modifiers; Layers layers;};

const auto KEY_STROKE_UP = 0;
const auto KEY_STROKE_DOWN = 1;
const auto KEY_STROKE_REPEAT = 2;

const auto MODIFIERS = std::unordered_set<KeyCode> {
  KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL,
  KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA,
  // TODO: handle capslock as not modifier?
  KEY_CAPSLOCK,
};

auto is_modifier(int key) -> bool {
  return MODIFIERS.contains(key);
}

// auxilliary renamings for accessibility
const auto KEY_BACKTICK = KEY_GRAVE;
// in the real world, braces = `{`, `}`, brackets: `[`, `]`
const auto KEY_LEFTBRACKET = KEY_LEFTBRACE;
const auto KEY_RIGHTBRACKET = KEY_RIGHTBRACE;
// print screen
const auto KEY_PRT_SC = KEY_SYSRQ;

// all mappable keys (for now)
// see: github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
const auto KEYS = std::unordered_map<std::string, KeyCode> {
// KEY(KEY_K) preprocesses to {"KEY_K", KEY_K}
#define KEY(KEYCODE) {#KEYCODE, KEYCODE}

  KEY(KEY_ESC), KEY(KEY_F1), KEY(KEY_F2), KEY(KEY_F3), KEY(KEY_F4), KEY(KEY_F5),
    KEY(KEY_F6), KEY(KEY_F7), KEY(KEY_F8), KEY(KEY_F9), KEY(KEY_F10), KEY(KEY_F11), KEY(KEY_F12),

  KEY(KEY_BACKTICK), KEY(KEY_1), KEY(KEY_2), KEY(KEY_3), KEY(KEY_4), KEY(KEY_5), KEY(KEY_6),
    KEY(KEY_7), KEY(KEY_8), KEY(KEY_9), KEY(KEY_0), KEY(KEY_MINUS), KEY(KEY_EQUAL), KEY(KEY_BACKSPACE),
 
  KEY(KEY_TAB), KEY(KEY_Q), KEY(KEY_W), KEY(KEY_E), KEY(KEY_R), KEY(KEY_T), KEY(KEY_Y),
    KEY(KEY_U), KEY(KEY_I), KEY(KEY_O), KEY(KEY_P), KEY(KEY_LEFTBRACKET), KEY(KEY_RIGHTBRACKET), KEY(KEY_BACKSLASH),

  KEY(KEY_CAPSLOCK), KEY(KEY_A), KEY(KEY_S), KEY(KEY_D), KEY(KEY_F), KEY(KEY_G), KEY(KEY_H),
    KEY(KEY_J), KEY(KEY_K), KEY(KEY_L), KEY(KEY_SEMICOLON), KEY(KEY_APOSTROPHE), KEY(KEY_ENTER),

  KEY(KEY_LEFTSHIFT), KEY(KEY_Z), KEY(KEY_X), KEY(KEY_C), KEY(KEY_V), KEY(KEY_B), KEY(KEY_N),
    KEY(KEY_M), KEY(KEY_COMMA), KEY(KEY_DOT), KEY(KEY_SLASH), KEY(KEY_RIGHTSHIFT),

  KEY(KEY_LEFTCTRL), KEY(KEY_LEFTALT), KEY(KEY_RIGHTALT), KEY(KEY_RIGHTCTRL),

  KEY(KEY_LEFT), KEY(KEY_DOWN), KEY(KEY_UP), KEY(KEY_RIGHT), KEY(KEY_PAGEDOWN), KEY(KEY_PAGEUP),

  KEY(KEY_HOME), KEY(KEY_END), KEY(KEY_INSERT), KEY(KEY_DELETE),

  // mouse buttons
  KEY(BTN_LEFT), KEY(BTN_RIGHT), KEY(BTN_BACK), KEY(BTN_FORWARD),

  // display brightness
  KEY(KEY_BRIGHTNESSDOWN), KEY(KEY_BRIGHTNESSUP),

  // audio
  KEY(KEY_MUTE), KEY(KEY_VOLUMEDOWN), KEY(KEY_VOLUMEUP),

  KEY(KEY_PRT_SC), KEY(KEY_CONTEXT_MENU), KEY(KEY_PRINT),

#undef KEY
};

const auto DEFAULT_MAPPING = Mapping {
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

  // FIXME: these never did anything even in original src tree (thinkpad x1 yoga)
  // mouse navigation
  {BTN_LEFT, BTN_BACK},
  {BTN_RIGHT, BTN_FORWARD},

  // PrtSc -> Context Menu
  // FIXME: this is not working, even though `wev` says keycode 99 is Print
  // {KEY_SYSRQ, KEY_CONTEXT_MENU},
};

auto key_event(int value, KeyCode code, KeyCode type=EV_KEY) -> Event {
  return {.time={.tv_sec=0, .tv_usec=0}, .type=type, .code=code, .value=value};
}

const auto SYNC = key_event(KEY_STROKE_UP, SYN_REPORT, EV_SYN);

auto read_event() -> std::optional<Event> {
  if (auto event = Event{}; std::fread(&event, sizeof(Event), 1, stdin) == 1) {
    return event;
  }
  return {};
}

auto write_event(Event event) {
  if (std::fwrite(&event, sizeof(Event), 1, stdout) != 1) {
    std::exit(EXIT_FAILURE);
  }
}

auto write_events(const std::vector<Event>& events) {
  if (std::fwrite(events.data(), sizeof(Event), events.size(), stdout) != events.size()) {
    std::exit(EXIT_FAILURE);
  }
}

auto write_keytap(KeyCode code) {
  write_events({key_event(KEY_STROKE_DOWN, code), SYNC, key_event(KEY_STROKE_UP, code)});
}

class InterceptedKey {
public:
  InterceptedKey(KeyCode intercept, KeyCode tapped)
      : intercept{intercept}, tapped{tapped}, state{State::START}, shouldEmitTapped{true} {}

  auto process(Event *input) -> bool {
    switch (state) {
      case State::START: return processStart(input);
      case State::INTERCEPTED_KEY_HELD: return processInterceptedHeld(input);
      case State::OTHER_KEY_HELD: return processOtherKeyHeld(input);
      default: throw std::exception();
    }
  }

protected:
  enum class State {START, INTERCEPTED_KEY_HELD, OTHER_KEY_HELD};

  auto processStart(Event *input) -> bool {
    if (is_intercept(input->code) && input->value == KEY_STROKE_DOWN) {
      shouldEmitTapped = true;
      state = State::INTERCEPTED_KEY_HELD;
      return false;
    }
    return true;
  };

  auto is_intercept(KeyCode code) -> bool { return intercept == code; }

  virtual auto processInterceptedHeld(Event* input) -> bool = 0;
  virtual auto processOtherKeyHeld(Event* input) -> bool = 0;

  KeyCode intercept;
  KeyCode tapped;
  State state;
  bool shouldEmitTapped;
};

class InterceptedKeyLayer : public InterceptedKey {
private:
  Mapping mapping;
  std::unordered_set<KeyCode>* held_keys = new std::unordered_set<KeyCode>();

protected:
  auto remapped(Event input) -> Event {
    input.code = mapping[input.code];
    return input;
  }

  auto processInterceptedHeld(Event *input) -> bool override {
    if (is_intercept(input->code) && input->value != KEY_STROKE_UP) {
      return false; // don't emit anything
    }

    if (is_intercept(input->code)) { // && stroke up
      // TODO: find a way to have a mouse click mark the key as intercepted
      // or just make it time based
      // shouldEmitTapped &= mouse clicked || timeout;

      if (shouldEmitTapped) {
        write_keytap(tapped);
        shouldEmitTapped = false;
      }

      state = State::START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) {
      // any other key

      // NOTE: if we don't blindly set shouldEmitTapped to false on any
      // keypress, we can type faster because only in case of mapped key down,
      // the intercepted key will not be emitted - useful for scenario:
      // L_DOWN, SPACE_DOWN, A_DOWN, L_UP, A_UP, SPACE_UP
      shouldEmitTapped &= !hasMapped(input->code) && !is_modifier(input->code);

      if (hasMapped(input->code)) {
        held_keys->insert(input->code);
        auto mapped = remapped(*input);
        write_event(mapped);
        state = InterceptedKey::State::OTHER_KEY_HELD;
        return false;
      }
    }

    return true;
  }

  auto processOtherKeyHeld(Event *input) -> bool override {
    if (input->code == intercept && input->value != KEY_STROKE_UP) {
      return false;
    }
    if (input->value == KEY_STROKE_DOWN
        && held_keys->find(input->code) != held_keys->end()) {
      return false;
    }

    auto shouldEmitInput = true;
    if (input->value == KEY_STROKE_UP) {
      // one of mapped held keys goes up
      if (held_keys->find(input->code) != held_keys->end()) {
        auto mapped = remapped(*input);
        write_event(mapped);
        held_keys->erase(input->code);
        if (held_keys->empty()) {
          state = InterceptedKey::State::INTERCEPTED_KEY_HELD;
        }
        shouldEmitInput = false;
      } else {
        // key that was not mapped & held goes up
        if (is_intercept(input->code)) {
          auto held_keys_up = std::vector<Event>{};
          for (auto held_key_code : *held_keys) {
            auto held_key_up = key_event(KEY_STROKE_UP, mapping[held_key_code]);
            held_keys_up.push_back(held_key_up);
            held_keys_up.push_back(SYNC);
          }

          write_events(held_keys_up);
          held_keys->clear();
          state = InterceptedKey::State::START;
          shouldEmitInput = false;
        }
      }
    } else { // KEY_STROKE_DOWN or KEY_STROKE_REPEAT
      if (hasMapped(input->code)) {
        auto mapped = remapped(*input);
        write_event(mapped);
        if (input->value == KEY_STROKE_DOWN) {
          held_keys->insert(input->code);
        }
        shouldEmitInput = false;
      }
    }
    return shouldEmitInput;
  }

public:
  InterceptedKeyLayer(KeyCode intercept, KeyCode tapped, Mapping mapping)
      : InterceptedKey{intercept, tapped}, mapping{mapping} {}

  auto hasMapped(KeyCode key) -> bool { return mapping.contains(key); }
};

class InterceptedKeyModifier : public InterceptedKey {
protected:
  KeyCode modifier;

  bool processInterceptedHeld(Event *input) override {
    if (is_intercept(input->code) && input->value != KEY_STROKE_UP) {
      return false;
    }

    bool shouldEmitInput = true;
    if (is_intercept(input->code)) { // && stroke up
      if (shouldEmitTapped) {
        write_keytap(tapped);
      } else { // intercepted is mapped to modifier and key stroke up
        Event modifier_up = *input;
        modifier_up.code = modifier;
        write_event(modifier_up);
      }

      state = State::START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) { // any other than intercepted
      if (shouldEmitTapped) { // on first non-matching input after a
                                     // matching input
        auto modifier_down = new Event(*input);
        modifier_down->code = modifier;

        // for some reason, need to push "SYNC" after modifier here
        write_events({*modifier_down, SYNC});

        shouldEmitTapped = false;
        return true; // gotta emit input event independently so we can process layer+modifier+input together
      }
    }

    return shouldEmitInput;
  }

  auto processOtherKeyHeld([[maybe_unused]] Event *_) -> bool override { return true; }

public:
  InterceptedKeyModifier(KeyCode intercept, KeyCode tapped, KeyCode modifier)
      : InterceptedKey{intercept, tapped}, modifier{modifier} {
    if (!is_modifier(modifier)) {
      throw std::invalid_argument("Specified wrong modifier key");
    }
  }
};

auto old_default_config() -> std::vector<InterceptedKey*> {
  auto caps = new InterceptedKeyModifier(KEY_CAPSLOCK, KEY_ESC, KEY_LEFTCTRL);
  auto enter = new InterceptedKeyModifier(KEY_ENTER, KEY_ENTER, KEY_RIGHTCTRL);
  auto space = new InterceptedKeyLayer(KEY_SPACE, KEY_SPACE, DEFAULT_MAPPING);
  return std::vector<InterceptedKey*> { caps, enter, space };
}

auto default_config() -> Configuration {
  return Configuration {
    Modifiers {
      InterceptedKeyModifier{KEY_CAPSLOCK, KEY_ESC, KEY_LEFTCTRL},
      InterceptedKeyModifier{KEY_ENTER, KEY_ENTER, KEY_RIGHTCTRL},
    },
    Layers {
      InterceptedKeyLayer{KEY_SPACE, KEY_SPACE, DEFAULT_MAPPING},
    }
  };
}

auto read_config_or_default(int argc, char** argv) -> Configuration {
  if (argc < 2) return default_config();
  // TODO: parse the new structured yaml
  /* try { */
  /*   auto mapping = Mapping{}; */
  /*   for (const auto& item : YAML::LoadFile(argv[1])) { */
  /*     auto from = KEYS.at(item["from"].as<std::string>()); */
  /*     auto to = KEYS.at(item["to"].as<std::string>()); */
  /*     mapping[from] = to; */
  /*   } */
  /*   return mapping; */
  /* } catch (...) { */
  /*   return DEFAULT_MAPPING; */
  /* } */
  return default_config();
}

class Interceptor {
public:
  Interceptor(Configuration config)
      : modifiers{config.modifiers},
        layers{config.layers},
        // TODO: get rid of this guy
        intercepted_keys{old_default_config()} { }

  auto event_loop() -> void {
    while (auto input = read_event()) {
      intercept(*input);
    }
  }

private:
  Modifiers modifiers;
  Layers layers;
  std::vector<InterceptedKey*> intercepted_keys;

  auto intercept(Event input) -> void {
    if (input.type == EV_MSC && input.code == MSC_SCAN) return;
    if (input.type != EV_KEY || processed(input)) {
      write_event(input);
    }
  }

  auto processed(Event input) -> bool {
    auto processed = true;
    for (auto key : intercepted_keys) {
      processed &= key->process(&input);
    }
    return processed;
  }
};

auto main(int argc, char** argv) -> int {
  std::setbuf(stdin, NULL);
  std::setbuf(stdout, NULL);
  Interceptor(read_config_or_default(argc, argv)).event_loop();
}
