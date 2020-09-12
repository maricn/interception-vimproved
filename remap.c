#include <stdio.h>
#include <stdlib.h>

#include <linux/input.h>
#include <time.h>
#include <unistd.h>

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
_space_up        = {.type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_UP},
_meta_up         = {.type = EV_KEY, .code = KEY_LEFTMETA, .value = KEY_STROKE_UP},
_space_down      = {.type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_DOWN},
_meta_down       = {.type = EV_KEY, .code = KEY_LEFTMETA, .value = KEY_STROKE_DOWN},
_space_repeat    = {.type = EV_KEY, .code = KEY_SPACE,    .value = KEY_STROKE_REPEAT},
_meta_repeat     = {.type = EV_KEY, .code = KEY_LEFTMETA, .value = KEY_STROKE_REPEAT},
_syn             = {.type = EV_SYN, .code = SYN_REPORT,   .value = KEY_STROKE_UP};
const struct input_event
*space_up     = &_space_up,
*meta_up      = &_meta_up,
*space_down   = &_space_down,
*meta_down    = &_meta_down,
*space_repeat = &_space_repeat,
*meta_repeat  = &_meta_repeat,
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
  map_space[KEY_H] = KEY_LEFT;
  map_space[KEY_J] = KEY_DOWN;
  map_space[KEY_K] = KEY_UP;
  map_space[KEY_L] = KEY_RIGHT;
}

int equal(const struct input_event *first, const struct input_event *second) {
  return first->type == second->type && first->code == second->code &&
         first->value == second->value;
}

int read_event(struct input_event *event) {
  return fread(event, input_event_struct_size, 1, stdin) == 1;
}

void write_event(const struct input_event *event) {
  if (fwrite(event, input_event_struct_size, 1, stdout) != 1)
    exit(EXIT_FAILURE);
}

void write_events(const struct input_event *ev1,
                  const struct input_event *ev2) {
  const struct input_event buffer[3] = {*ev1, _syn, *ev2};
  if (fwrite(&buffer, input_event_struct_size, 3, stdout) != 3)
    exit(EXIT_FAILURE);
}

int main() {
  struct input_event *input =
      (struct input_event *)malloc(input_event_struct_size);
  struct input_event *input_mapped_down =
                         (struct input_event *)malloc(input_event_struct_size),
                     *input_origin_up =
                         (struct input_event *)malloc(input_event_struct_size),
                     *input_mapped_up =
                         (struct input_event *)malloc(input_event_struct_size);
  *input_mapped_down = *space_down;
  *input_origin_up = *space_up;
  *input_mapped_up = *space_up;
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
          // TODO: any mapped key needs to go to a set of HELD keys and
          // only when last of HELD keys is released, we can go back to SPACE_HELD state
          // we should also accept new HELD keys while in KEY_HELD state
          *input_origin_up = *input;
          input_origin_up->value = KEY_STROKE_UP;

          *input_mapped_down = *input;
          input_mapped_down->code = map_space[input->code];

          *input_mapped_up = *input_mapped_down;
          input_mapped_up->value = KEY_STROKE_UP;

          write_event(input_mapped_down);
          state = KEY_HELD;
        } else {
          write_event(input);
        }
      } else { // KEY_STROKE_REPEAT or KEY_STROKE_UP
        if (input->code == KEY_SPACE && space_down_not_emitted) {
          write_events(space_down, input);
          space_down_not_emitted = 0;
        } else {
          write_event(input);
        }
        state = equal(input, space_up) ? START : SPACE_HELD;
      }
      break;
    case KEY_HELD:
      if (equal(input, space_down) || equal(input, space_repeat))
        break;
      if (equal(input, input_mapped_down)) // || equal(input, key_repeat))
        break;

      if (equal(input, input_origin_up)) {
        write_event(input_mapped_up);
        state = SPACE_HELD;
        // even if it actually wasn't emitted, we don't want to emit it
        // after we made sure space is supposed to behave as function key
        space_down_not_emitted = 0;
      } else {
        write_event(input);
      }
      break;
    }
  }

  free(input);
}
