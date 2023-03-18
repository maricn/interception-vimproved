#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <linux/input.h>
#include <yaml-cpp/yaml.h>

class Intercept;
using Event = input_event;
using Key = unsigned short;
using Mapping = std::unordered_map<Key, Key>;
using Intercepts = std::vector<Intercept*>;

const auto KEY_STROKE_UP = 0;
const auto KEY_STROKE_DOWN = 1;
const auto KEY_STROKE_REPEAT = 2;

auto is_keyup(Event input) -> bool { return input.value == KEY_STROKE_UP; }
auto is_keydown(Event input) -> bool { return input.value == KEY_STROKE_DOWN; }

auto is_modifier(Key key) -> bool {
  // TODO: handle capslock as not modifier?
  static const auto MODIFIERS = std::unordered_set<Key> {
    KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL,
    KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_CAPSLOCK,
  };
  return MODIFIERS.contains(key);
}

// auxilliary renamings for accessibility
// in the real world, braces = `{`, `}`, brackets: `[`, `]`
const auto KEY_BACKTICK = KEY_GRAVE;
const auto KEY_LEFTBRACKET = KEY_LEFTBRACE;
const auto KEY_RIGHTBRACKET = KEY_RIGHTBRACE;
const auto KEY_PRT_SC = KEY_SYSRQ;

// all mappable keys (for now)
// see: github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
const auto KEYS = std::unordered_map<std::string, Key> {
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

  KEY(KEY_LEFTCTRL), KEY(KEY_LEFTALT), KEY(KEY_SPACE), KEY(KEY_RIGHTALT), KEY(KEY_RIGHTCTRL),

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

auto key_event(int value, Key code, Key type=EV_KEY) -> Event {
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

auto write_keytap(Key code) {
  write_events({key_event(KEY_STROKE_DOWN, code), SYNC, key_event(KEY_STROKE_UP, code)});
}

class Intercept {
public:
  auto process(Event input) -> bool {
    switch (state) {
      case State::START: return process_start(input);
      case State::INTERCEPT_KEY_HELD: return process_intercept_held(input);
      case State::OTHER_KEY_HELD: return process_other_key_held(input);
      default: throw std::exception();
    }
  }

protected:
  Intercept(Key intercept, Key tap)
      : intercept{intercept}, tap{tap}, state{State::START}, emit_tap{true} {}

  enum class State {START, INTERCEPT_KEY_HELD, OTHER_KEY_HELD};
  Key intercept;
  Key tap;
  State state;
  bool emit_tap;

  auto is_intercept(Event input) -> bool { return input.code == intercept; }

  auto process_start(Event input) -> bool {
    if (is_intercept(input) && is_keydown(input)) {
      emit_tap = true;
      state = State::INTERCEPT_KEY_HELD;
      return false;
    }
    return true;
  };

  virtual auto process_intercept_held(Event input) -> bool = 0;
  virtual auto process_other_key_held(Event input) -> bool = 0;
};

class Layer : public Intercept {
public:
  Layer(Key intercept, Key tap, Mapping mapping)
      : Intercept{intercept, tap}, mapping{mapping} {}

private:
  Mapping mapping;
  std::unordered_set<Key> held_keys;

  auto is_mapped(Event input) -> bool { return mapping.contains(input.code); }

  auto is_held(Event input) -> bool { return held_keys.contains(input.code); }

  auto remapped(Event input) -> Event {
    input.code = mapping[input.code];
    return input;
  }

  auto process_intercept_held(Event input) -> bool override {
    // don't emit anything
    if (is_intercept(input) && !is_keyup(input)) return false;

    if (is_intercept(input)) { // && stroke up
      // TODO: find a way to have a mouse click mark the key as intercepted
      // or just make it time based
      // emit_tap &= mouse clicked || timeout;

      if (emit_tap) {
        write_keytap(tap);
        emit_tap = false;
      }

      state = State::START;
      return false;
    }

    if (is_keydown(input)) {
      // any other key

      // NOTE: if we don't blindly set emit_tap to false on any
      // keypress, we can type faster because only in case of mapped key down,
      // the intercepted key will not be emitted - useful for scenario:
      // L_DOWN, SPACE_DOWN, A_DOWN, L_UP, A_UP, SPACE_UP
      emit_tap &= !is_mapped(input) && !is_modifier(input.code);

      if (is_mapped(input)) {
        held_keys.insert(input.code);
        write_event(remapped(input));
        state = State::OTHER_KEY_HELD;
        return false;
      }
    }

    return true;
  }

  auto process_other_key_held(Event input) -> bool override {
    if (is_intercept(input) && !is_keyup(input)) return false;
    if (is_keydown(input) && is_held(input)) return false;

    auto emit_input = true;
    if (is_keyup(input)) {
      // one of mapped held keys goes up
      if (is_held(input)) {
        write_event(remapped(input));
        held_keys.erase(input.code);
        if (held_keys.empty()) {
          state = State::INTERCEPT_KEY_HELD;
        }
        emit_input = false;
      } else {
        // key that was not mapped & held goes up
        if (is_intercept(input)) {
          auto held_keys_up = std::vector<Event>{};
          for (auto held_key_code : held_keys) {
            auto held_key_up = key_event(KEY_STROKE_UP, mapping[held_key_code]);
            held_keys_up.push_back(held_key_up);
            held_keys_up.push_back(SYNC);
          }
          write_events(held_keys_up);
          held_keys.clear();
          state = State::START;
          emit_input = false;
        }
      }
    } else {
      if (is_mapped(input)) {
        write_event(remapped(input));
        if (is_keydown(input)) {
          held_keys.insert(input.code);
        }
        emit_input = false;
      }
    }
    return emit_input;
  }
};

class Modifier : public Intercept {
public:
  Modifier(Key intercept, Key tap, Key modifier)
      : Intercept{intercept, tap}, modifier{modifier} {
    if (!is_modifier(modifier)) {
      throw std::invalid_argument("Specified wrong modifier key");
    }
  }

protected:
  Key modifier;

  auto process_intercept_held(Event input) -> bool override {
    if (is_intercept(input) && !is_keyup(input)) return false;

    bool emit_input = true;
    if (is_intercept(input)) { // && stroke up
      if (emit_tap) {
        write_keytap(tap);
      } else { // intercepted is mapped to modifier and key stroke up
        input.code = modifier;
        write_event(input);
      }
      state = State::START;
      return false;
    }
    if (is_keydown(input)) {
      // any other than intercepted
      if (emit_tap) {
        // on first non-matching input after a matching input
        input.code = modifier;

        // for some reason, need to push "SYNC" after modifier here
        write_events({input, SYNC});

        emit_tap = false;
        return true; // gotta emit input event independently so we can process layer+modifier+input together
      }
    }
    return emit_input;
  }

  auto process_other_key_held([[maybe_unused]] Event _) -> bool override { return true; }
};

auto default_config() -> Intercepts {
  auto caps = new Modifier(KEY_CAPSLOCK, KEY_ESC, KEY_LEFTCTRL);
  auto enter = new Modifier(KEY_ENTER, KEY_ENTER, KEY_RIGHTCTRL);
  auto space = new Layer(KEY_SPACE, KEY_SPACE, Mapping {
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
  });
  return Intercepts{caps, enter, space};
}

auto read_config_or_default(int argc, char** argv) -> Intercepts {
  if (argc < 2) return default_config();
  try {
    auto modifiers = Intercepts{};
    auto layers = Intercepts{};

    auto read_key = [&](auto item){
      //TODO(WTF C++): "expected 'template' keyword before dependent template name"
      // | (this is not necessary in the yaml manual)
      // `----------------vvvvvvvv
      return KEYS.at(item.template as<std::string>());
    };

    auto read_mapping = [&](auto yaml) {
      auto mapping = Mapping{};
      for (const auto& item : yaml) {
        mapping[read_key(item["from"])] = read_key(item["to"]);
      }
      return mapping;
    };

    auto read_intercept = [&](auto yaml) {
      auto intercept = read_key(yaml["intercept"]);
      auto ontap = yaml["ontap"] ? read_key(yaml["ontap"]) : intercept;

      // TODO: treat empty `onhold` as a simple remapping (like caps2esc)
      auto onhold = yaml["onhold"];
      if (!onhold) throw std::invalid_argument(
        "Invalid Configuration: onhold cannot be empty"
      );
      if (onhold.IsSequence()) {
        layers.push_back(new Layer{intercept, ontap, read_mapping(onhold)});
      }
      else if (onhold.IsScalar()) {
        modifiers.push_back(new Modifier{intercept, ontap, read_key(onhold)});
      }
      else {
        throw std::invalid_argument(
          "Invalid Configuration: onhold must be sequence or scalar"
        );
      }
    };

    // NOTE: modifiers must go first because layer.process_intercept_held
    // emits mapped key as soon as layer.process is called
    // if layer.process is run before modifier.process, the modifier is not emitted
    auto intercepts = Intercepts{};
    std::ranges::for_each(YAML::LoadFile(argv[1]), read_intercept);
    std::ranges::copy(modifiers, std::back_inserter(intercepts));
    std::ranges::copy(layers, std::back_inserter(intercepts));
    return intercepts;
  } catch (...) {
    return default_config();
  }
}

class Interceptor {
public:
  Interceptor(Intercepts intercepts) : intercepts{intercepts} {}

  auto event_loop() -> void {
    while (auto input = read_event()) {
      intercept(*input);
    }
  }

private:
  Intercepts intercepts;

  auto intercept(Event input) -> void {
    if (input.type == EV_MSC && input.code == MSC_SCAN) return;
    if (input.type != EV_KEY || processed(input)) {
      write_event(input);
    }
  }

  auto processed(Event input) -> bool {
    auto processed = true;
    for (auto intercept : intercepts) {
      processed &= intercept->process(input);
    }
    return processed;
  }
};

auto main(int argc, char** argv) -> int {
  std::setbuf(stdin, NULL);
  std::setbuf(stdout, NULL);
  Interceptor(read_config_or_default(argc, argv)).event_loop();
}
