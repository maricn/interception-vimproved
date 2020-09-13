#include <cstdlib>
#include <iostream>

#include <linux/input.h>
#include <set>
#include <time.h>
#include <unistd.h>
#include <vector>

/**
 * Global constants
 **/
#define MS_TO_NS 1000000L // 1 millisecond = 1,000,000 Nanoseconds
const int KEY_STROKE_UP = 0, KEY_STROKE_DOWN = 1, KEY_STROKE_REPEAT = 2;
const int INPUT_BUFFER_SIZE = 16;
const long SLEEP_INTERVAL_NS = 20 * MS_TO_NS;
const int input_event_struct_size = sizeof(struct input_event);

int map_space[KEY_MAX];

// clang-format off
const struct input_event
_space_up        = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_UP},
_space_down      = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_DOWN},
_space_repeat    = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_REPEAT},
_syn             = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_SYN, .code = SYN_REPORT,   .value = KEY_STROKE_UP};
const struct input_event
*space_up     = &_space_up,
*space_down   = &_space_down,
*space_repeat = &_space_repeat,
*syn          = &_syn;
// clang-format on

/*
static int key_ismod(int code) {
  switch (code) {
  default:
    return 0;
  case KEY_LEFTSHIFT:
  case KEY_RIGHTSHIFT:
  case KEY_LEFTCTRL:
  case KEY_RIGHTCTRL:
  case KEY_LEFTALT:
  case KEY_RIGHTALT:
  case KEY_LEFTMETA:
  case KEY_RIGHTMETA:
    return 1;
  }
}
*/
void map_space_init() {
  // vim home row
  map_space[KEY_H] = KEY_LEFT;
  map_space[KEY_J] = KEY_DOWN;
  map_space[KEY_K] = KEY_UP;
  map_space[KEY_L] = KEY_RIGHT;

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

int equal(const struct input_event *first, const struct input_event *second) {
  return first->type == second->type && first->code == second->code &&
         first->value == second->value;
}

auto cmp = [](const struct input_event *e1, const struct input_event *e2) {
  return equal(e1, e2) != 0;
};

int read_event(struct input_event *event) {
  return fread(event, input_event_struct_size, 1, stdin) == 1;
}

void write_event(const struct input_event *event) {
  if (fwrite(event, input_event_struct_size, 1, stdout) != 1)
    exit(EXIT_FAILURE);
}

void write_events(std::vector<struct input_event> *events) {
  const unsigned long size = events->size();
  if (fwrite(events->data(), input_event_struct_size, size, stdout) != size)
    exit(EXIT_FAILURE);
}

struct input_event *map(const struct input_event *input, int direction = -1) {
  struct input_event *result = new input_event(*input);
  result->code = map_space[input->code];
  if (direction != -1) {
    result->value = direction;
  }
  return result;
}

int main() {
  struct input_event *input = new input_event();
  std::set<int> held_keys;
  enum { START, SPACE_HELD, KEY_HELD } state = START;
  int space_down_not_emitted = 1;

  map_space_init();
  setbuf(stdin, NULL), setbuf(stdout, NULL);

  while (read_event(input)) {
    if (input->type == EV_MSC && input->code == MSC_SCAN)
      continue;

    if (input->type != EV_KEY) {
      write_event(input);
      continue;
    }

    switch (state) {
    case START:
      if (equal(input, space_down) || equal(input, space_repeat)) {
        state = SPACE_HELD;
        space_down_not_emitted = 1;
      } else {
        write_event(input);
      }
      break;
    case SPACE_HELD:
      if (equal(input, space_down) || equal(input, space_repeat))
        break;
      if (input->value == KEY_STROKE_DOWN) {
        if (map_space[input->code] != 0) {
          held_keys.insert(input->code);
          space_down_not_emitted = 0;
          struct input_event *mapped_input = map(input);
          write_event(mapped_input);
          delete mapped_input;
          state = KEY_HELD;
        } else {
          write_event(input);
        }
      } else { // KEY_STROKE_REPEAT or KEY_STROKE_UP
        if (input->code == KEY_SPACE && space_down_not_emitted) {
          space_down_not_emitted = 0;
          struct input_event space_down_var(*space_down);
          std::vector<struct input_event> *combo =
              new std::vector<struct input_event>();
          combo->push_back(*space_down);
          combo->push_back(*input);
          write_events(combo);
          delete combo;
        } else {
          write_event(input);
        }
        state = equal(input, space_up) ? START : SPACE_HELD;
      }
      break;
    case KEY_HELD:
      if (equal(input, space_down) || equal(input, space_repeat))
        break;
      if (input->value == KEY_STROKE_DOWN &&
          held_keys.find(input->code) != held_keys.end()) {
        break;
      }

      if (input->value == KEY_STROKE_UP) {
        if (held_keys.find(input->code) != held_keys.end()) { // one of mapped held keys goes up
          write_event(map(input));
          held_keys.erase(input->code);
          if (held_keys.empty()) {
            state = SPACE_HELD;
          }
        } else { // regular key goes up
          if (equal(input, space_up)) {
            std::vector<struct input_event> *held_keys_up =
                new std::vector<struct input_event>();
            for (auto held_key_code : held_keys) {
              struct input_event held_key_up;
              held_key_up.code = held_key_code;
              held_key_up.value = KEY_STROKE_UP;
              held_keys_up->push_back(held_key_up);
            }
            held_keys_up->push_back(*space_up);

            write_events(held_keys_up);
            delete held_keys_up;
            held_keys.clear();
            state = START;
          } else {
            write_event(input);
          }
        }
      } else { // KEY_STROKE_DOWN or KEY_STROKE_REPEAT
        if (map_space[input->code] != 0) {
          auto mapped = map(input);
          write_event(mapped);
          delete mapped;
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
