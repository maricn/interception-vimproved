#include <cstdlib>
#include <iostream>

#include <linux/input.h>
#include <set>
#include <unistd.h>
#include <vector>

using namespace std;

/**
 * Global constants
 **/
#define MS_TO_NS 1000000L // 1 millisecond = 1,000,000 Nanoseconds

typedef struct input_event event;

const int KEY_STROKE_UP = 0, KEY_STROKE_DOWN = 1, KEY_STROKE_REPEAT = 2;
const int INPUT_BUFFER_SIZE = 16;
const long SLEEP_INTERVAL_NS = 20 * MS_TO_NS;
const int input_event_struct_size = sizeof(struct input_event);

// clang-format off
const struct input_event
_syn             = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_SYN, .code = SYN_REPORT, .value = KEY_STROKE_UP};
const struct input_event
*syn          = &_syn;
// clang-format on

unsigned short map_space[KEY_MAX];
void map_space_init() {
  // special chars
  map_space[KEY_E] = KEY_ESC;
  map_space[KEY_D] = KEY_DELETE;
  map_space[KEY_B] = KEY_BACKSPACE;

  // vim home row
  map_space[KEY_H] = KEY_LEFT;
  map_space[KEY_J] = KEY_DOWN;
  map_space[KEY_K] = KEY_UP;
  map_space[KEY_L] = KEY_RIGHT;

  // vim above home row
  map_space[KEY_Y] = KEY_HOME;
  map_space[KEY_U] = KEY_PAGEDOWN;
  map_space[KEY_I] = KEY_PAGEUP;
  map_space[KEY_O] = KEY_END;

  // number row to F keys
  map_space[KEY_1] = KEY_F1;
  map_space[KEY_2] = KEY_F2;
  map_space[KEY_3] = KEY_F3;
  map_space[KEY_4] = KEY_F4;
  map_space[KEY_5] = KEY_F5;
  map_space[KEY_6] = KEY_F6;
  map_space[KEY_7] = KEY_F7;
  map_space[KEY_8] = KEY_F8;
  map_space[KEY_9] = KEY_F9;
  map_space[KEY_0] = KEY_F10;
  map_space[KEY_MINUS] = KEY_F11;
  map_space[KEY_EQUAL] = KEY_F12;

  // xf86 audio
  map_space[KEY_M] = KEY_MUTE;
  map_space[KEY_COMMA] = KEY_VOLUMEDOWN;
  map_space[KEY_DOT] = KEY_VOLUMEUP;
}

int read_event(event *e) {
  return fread(e, input_event_struct_size, 1, stdin) == 1;
}

void write_event(event *e) {
  if (fwrite(e, input_event_struct_size, 1, stdout) != 1)
    exit(EXIT_FAILURE);
}

void write_events(vector<input_event> *events) {
  const unsigned long size = events->size();
  if (fwrite(events->data(), input_event_struct_size, size, stdout) != size)
    exit(EXIT_FAILURE);
}

void write_combo(unsigned short keycode) {
  input_event key_down = {.time = {.tv_sec = 0, .tv_usec = 0},
                          .type = EV_KEY,
                          .code = keycode,
                          .value = KEY_STROKE_DOWN};
  input_event key_up = {.time = {.tv_sec = 0, .tv_usec = 0},
                        .type = EV_KEY,
                        .code = keycode,
                        .value = KEY_STROKE_UP};

  vector<input_event> *combo = new vector<input_event>();
  combo->push_back(key_down);
  combo->push_back(*syn);
  combo->push_back(key_up);
  write_events(combo);

  delete combo;
}

struct input_event map(const struct input_event *input, int direction = -1) {
  struct input_event result(*input);
  result.code = map_space[input->code];
  if (direction != -1) {
    result.value = direction;
  }
  return result;
}

int main() {
  input_event *input = new input_event();
  set<int> held_keys;
  enum {
    START,
    MODIFIER_HELD,
    KEY_HELD
  } state_space = START,
    state_caps = START;
  bool space_tapped_should_emit = true, caps_tapped_should_emit = true;

  map_space_init();
  setbuf(stdin, NULL), setbuf(stdout, NULL);

  while (read_event(input)) {
    if (input->type == EV_MSC && input->code == MSC_SCAN)
      continue;

    if (input->type != EV_KEY) {
      write_event(input);
      continue;
    }

    // @TODO: handle return as RCTRL

    switch (state_caps) {
    case START:
      if (input->code == KEY_CAPSLOCK && input->value == KEY_STROKE_DOWN) {
        caps_tapped_should_emit = true;
        state_caps = MODIFIER_HELD;
        continue;
      }
      break;
    case MODIFIER_HELD:
      if (input->code == KEY_CAPSLOCK && input->value != KEY_STROKE_UP)
        continue;

      if (input->code == KEY_CAPSLOCK) {
        if (caps_tapped_should_emit) { // and key stroke up
          write_combo(KEY_ESC);
          caps_tapped_should_emit = false;
        } else { // caps is ctrl and key stroke up
          event ctrl_up(*input);
          ctrl_up.code = KEY_LEFTCTRL;
          write_event(&ctrl_up);
        }
        state_caps = START;
        continue;
      }

      if (input->value == KEY_STROKE_DOWN) { // any key != capslock goes down
        // TODO: find a way to have a mouse click mark caps as ctrl
        // or just make it time based
        if (caps_tapped_should_emit) {
          event ctrl_down = {.time = {.tv_sec = 0, .tv_usec = 0},
                             .type = EV_KEY,
                             .code = KEY_LEFTCTRL,
                             .value = KEY_STROKE_DOWN};
          write_event(&ctrl_down);
          caps_tapped_should_emit = false;
        }
      }

      // go on to the next key and eventually write_event(input)
      break;
    default:
      break;
    }

    switch (state_space) {
    case START:
      if (input->code == KEY_SPACE && input->value != KEY_STROKE_UP) {
        space_tapped_should_emit = true;
        state_space = MODIFIER_HELD;
        continue;
      } else {
        write_event(input);
      }
      break;
    case MODIFIER_HELD:
      if (input->code == KEY_SPACE && input->value != KEY_STROKE_UP)
        continue;

      if (input->code == KEY_SPACE) { // && stroke up
        if (space_tapped_should_emit) {
          write_combo(KEY_SPACE);
          space_tapped_should_emit = false;
        } // else if modifier key => emit modifier down
        state_space = START;
        continue;
      }

      if (input->value == KEY_STROKE_DOWN) {
        if (map_space[input->code] != 0) { // mapped key down
          held_keys.insert(input->code);
          space_tapped_should_emit = false;
          event mapped = map(input);
          write_event(&mapped);
          state_space = KEY_HELD;
        } else { // key stroke down any unmapped key
          if (input->code == KEY_CAPSLOCK) {
            space_tapped_should_emit = false;
          }
          write_event(input);
        }
      } 

      else { // any key (unmapped and not space) KEY_STROKE_REPEAT or KEY_STROKE_UP
          write_event(input);
      }
      break;
    case KEY_HELD:
      if (input->code == KEY_SPACE && input->value != KEY_STROKE_UP)
        break;
      if (input->value == KEY_STROKE_DOWN &&
          held_keys.find(input->code) != held_keys.end()) {
        break;
      }

      if (input->value == KEY_STROKE_UP) {
        if (held_keys.find(input->code) !=
            held_keys.end()) { // one of mapped held keys goes up
          struct input_event mapped = map(input);
          write_event(&mapped);
          held_keys.erase(input->code);
          if (held_keys.empty()) {
            state_space = MODIFIER_HELD;
          }
        } else { // key that was not mapped & held goes up
          if (input->code == KEY_SPACE) {
            vector<input_event> *held_keys_up = new vector<input_event>();
            for (auto held_key_code : held_keys) {
              event held_key_up = {.time = {.tv_sec = 0, .tv_usec = 0},
                                   .type = EV_KEY,
                                   .code = map_space[held_key_code],
                                   .value = KEY_STROKE_UP};
              held_keys_up->push_back(held_key_up);
            }

            write_events(held_keys_up);
            delete held_keys_up;
            held_keys.clear();
            state_space = START;
          } else { // unmapped key goes up
            write_event(input);
          }
        }
      } else { // KEY_STROKE_DOWN or KEY_STROKE_REPEAT
        if (map_space[input->code] != 0) {
          auto mapped = map(input);
          write_event(&mapped);
          if (input->value == KEY_STROKE_DOWN) {
            held_keys.insert(input->code);
          }
        } else {
          write_event(input);
        }
      }
      break;
    }
  }

  free(input);
}
