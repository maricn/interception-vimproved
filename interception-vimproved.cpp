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
const int KEY_STROKE_UP = 0, KEY_STROKE_DOWN = 1, KEY_STROKE_REPEAT = 2;
const int input_event_struct_size = sizeof(struct input_event);

/**
 * Only very rare keys are above 0x151, not found on most of keyboards, probably
 * you don't need to mimic them. We cover above 0x100 which includes mouse BTNs.
 * Check what else is there at:
 * https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
 **/
const unsigned short MAX_KEY = 0x151;

typedef struct input_event Event;

// clang-format off
const Event
_syn = {.time = { .tv_sec = 0, .tv_usec = 0}, .type = EV_SYN, .code = SYN_REPORT, .value = KEY_STROKE_UP};
const Event
*syn = &_syn;
// clang-format on

int readEvent(Event *e) {
  return fread(e, input_event_struct_size, 1, stdin) == 1;
}

void writeEvent(const Event *e) {
  if (fwrite(e, input_event_struct_size, 1, stdout) != 1)
    exit(EXIT_FAILURE);
}

void writeEvents(const vector<input_event> *events) {
  const unsigned long size = events->size();
  if (fwrite(events->data(), input_event_struct_size, size, stdout) != size)
    exit(EXIT_FAILURE);
}

// @TODO: convert to a map cache to reduce amount of memory allocations
Event buildEvent(int direction, unsigned short keycode) {
  input_event ev = {.time = {.tv_sec = 0, .tv_usec = 0},
                    .type = EV_KEY,
                    .code = keycode,
                    .value = direction};
  return ev;
}

Event buildEventDown(unsigned short keycode) {
  return buildEvent(KEY_STROKE_DOWN, keycode);
}

Event buildEventUp(unsigned short keycode) {
  return buildEvent(KEY_STROKE_UP, keycode);
}

void writeCombo(unsigned short keycode) {
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
  writeEvents(combo);

  delete combo;
}

/***************************************************************
 *********************** Global classes ************************
 **************************************************************/

/**
 * Intercepted key specification
 **/
class InterceptedKey {
public:
  enum State { START = 0, INTERCEPTED_KEY_HELD = 1, OTHER_KEY_HELD = 2 };

  static int isModifier(int key) {
    switch (key) {
    default:
      return 0;
    // clang-format off
    case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
    case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
    case KEY_LEFTALT: case KEY_RIGHTALT:
    case KEY_LEFTMETA: case KEY_RIGHTMETA:
    // clang-format on
    // @TODO: handle capslock as not modifier?
    case KEY_CAPSLOCK:
      return 1;
    }
  }

  InterceptedKey(unsigned short intercepted, unsigned short tapped) {
    this->_intercepted = intercepted;
    this->_tapped = tapped;
  }

  bool matches(unsigned short code) { return this->_intercepted == code; }

  State getState() { return this->_state; }

  bool process(Event *input) {
    switch (this->_state) {
    case START:
      return this->processStart(input);
    case INTERCEPTED_KEY_HELD:
      return this->processInterceptedHeld(input);
    case OTHER_KEY_HELD:
      return this->processOtherKeyHeld(input);
    }

    throw std::exception();
  }

protected:
  bool processStart(Event *input) {
    if (this->matches(input->code) && input->value == KEY_STROKE_DOWN) {
      this->_shouldEmitTapped = true;
      this->_state = INTERCEPTED_KEY_HELD;
      return false;
    }

    return true;
  };

  virtual bool processInterceptedHeld(Event *input) = 0;
  virtual bool processOtherKeyHeld(Event *input) = 0;

  unsigned short _intercepted;
  unsigned short _tapped;
  State _state;
  bool _shouldEmitTapped = true;
};

class InterceptedKeyLayer : public InterceptedKey {
private:
  unsigned short *_map;
  set<unsigned short> *_heldKeys = new set<unsigned short>();

protected:
  Event map(Event *input) {
    Event result(*input);
    result.code = this->_map[input->code];
    return result;
  }

  bool processInterceptedHeld(Event *input) override {
    if (this->matches(input->code) && input->value != KEY_STROKE_UP) {
      return false; // don't emit anything
    }

    if (this->matches(input->code)) { // && stroke up
      // TODO: find a way to have a mouse click mark the key as intercepted
      // or just make it time based
      // this->_shouldEmitTapped &= mouse clicked || timeout;

      if (this->_shouldEmitTapped) {
        writeCombo(this->_tapped);
        this->_shouldEmitTapped = false;
      }

      this->_state = START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) { // any other key

      // @NOTE: if we don't blindly set _shouldEmitTapped to false on any
      // keypress, we can type faster because only in case of mapped key down,
      // the intercepted key will not be emitted - useful for scenario:
      // L_DOWN, SPACE_DOWN, A_DOWN, L_UP, A_UP, SPACE_UP
      this->_shouldEmitTapped &= !this->hasMapped(input->code) &&
                                 !InterceptedKey::isModifier(input->code);

      if (this->hasMapped(input->code)) {
        this->_heldKeys->insert(input->code);
        Event mapped = this->map(input);
        writeEvent(&mapped);
        this->_state = InterceptedKey::OTHER_KEY_HELD;
        return false;
      }
    }

    return true;
  }

  bool processOtherKeyHeld(Event *input) override {
    if (input->code == this->_intercepted && input->value != KEY_STROKE_UP)
      return false;
    if (input->value == KEY_STROKE_DOWN &&
        this->_heldKeys->find(input->code) != this->_heldKeys->end()) {
      return false;
    }

    bool shouldEmitInput = true;
    if (input->value == KEY_STROKE_UP) {

      if (this->_heldKeys->find(input->code) !=
          this->_heldKeys->end()) { // one of mapped held keys goes up
        Event mapped = this->map(input);
        writeEvent(&mapped);
        this->_heldKeys->erase(input->code);
        if (this->_heldKeys->empty()) {
          this->_state = InterceptedKey::INTERCEPTED_KEY_HELD;
        }
        shouldEmitInput = false;
      } else { // key that was not mapped & held goes up
        if (this->matches(input->code)) {
          vector<Event> *held_keys_up = new vector<Event>();
          for (auto held_key_code : *this->_heldKeys) {
            Event held_key_up = buildEventUp(this->_map[held_key_code]);
            held_keys_up->push_back(held_key_up);
            held_keys_up->push_back(*syn);
          }

          writeEvents(held_keys_up);
          delete held_keys_up;
          this->_heldKeys->clear();
          this->_state = InterceptedKey::START;
          shouldEmitInput = false;
        }
      }
    } else { // KEY_STROKE_DOWN or KEY_STROKE_REPEAT
      if (this->hasMapped(input->code)) {
        auto mapped = this->map(input);
        writeEvent(&mapped);
        if (input->value == KEY_STROKE_DOWN) {
          this->_heldKeys->insert(input->code);
        }
        shouldEmitInput = false;
      }
    }

    return shouldEmitInput;
  }

public:
  InterceptedKeyLayer(unsigned short intercepted, unsigned short tapped)
      : InterceptedKey(intercepted, tapped) {
    this->_map = new unsigned short[MAX_KEY]{0};
  }

  ~InterceptedKeyLayer() { delete this->_map; }

  InterceptedKey *addMapping(unsigned short from, unsigned short to) {
    this->_map[from] = to;
    return this;
  }

  bool hasMapped(unsigned short from) { return this->_map[from] != 0; }
};

class InterceptedKeyModifier : public InterceptedKey {
protected:
  unsigned short _modifier;

  bool processInterceptedHeld(Event *input) override {
    if (this->matches(input->code) && input->value != KEY_STROKE_UP) {
      return false;
    }

    bool shouldEmitInput = true;
    if (this->matches(input->code)) { // && stroke up
      if (this->_shouldEmitTapped) {
        writeCombo(this->_tapped);
      } else { // intercepted is mapped to modifier and key stroke up
        Event *modifier_up = new Event(*input);
        modifier_up->code = this->_modifier;
        writeEvent(modifier_up);
        delete modifier_up;
      }

      this->_state = START;
      return false;
    }

    if (input->value == KEY_STROKE_DOWN) { // any other than intercepted
      if (this->_shouldEmitTapped) { // on first non-matching input after a
                                     // matching input
        Event *modifier_down = new Event(*input);
        modifier_down->code = this->_modifier;

        // for some reason, need to push "syn" after modifier here
        vector<Event> *modifier_and_input = new vector<Event>();
        modifier_and_input->push_back(*modifier_down);
        modifier_and_input->push_back(*syn);
        writeEvents(modifier_and_input);

        this->_shouldEmitTapped = false;
        delete modifier_and_input;
        return true; // gotta emit input event independently so we can process layer+modifier+input together
      }
    }

    return shouldEmitInput;
  }

  bool processOtherKeyHeld(Event *input) override { return true; }

public:
  InterceptedKeyModifier(unsigned short intercepted, unsigned short tapped,
                         unsigned short modifier)
      : InterceptedKey(intercepted, tapped) {
    if (!InterceptedKey::isModifier(modifier))
      throw invalid_argument("Specified wrong modifier key");
    this->_modifier = modifier;
  }
};

vector<InterceptedKey *> *initInterceptedKeys() {
  // tap space for space, hold for layer mapping
  InterceptedKeyLayer *space = new InterceptedKeyLayer(KEY_SPACE, KEY_SPACE);

  // special chars
  space->addMapping(KEY_E, KEY_ESC);
  space->addMapping(KEY_D, KEY_DELETE);
  space->addMapping(KEY_B, KEY_BACKSPACE);

  // vim home row
  space->addMapping(KEY_H, KEY_LEFT);
  space->addMapping(KEY_J, KEY_DOWN);
  space->addMapping(KEY_K, KEY_UP);
  space->addMapping(KEY_L, KEY_RIGHT);

  // vim above home row
  space->addMapping(KEY_Y, KEY_HOME);
  space->addMapping(KEY_U, KEY_PAGEDOWN);
  space->addMapping(KEY_I, KEY_PAGEUP);
  space->addMapping(KEY_O, KEY_END);

  // number row to F keys
  space->addMapping(KEY_1, KEY_F1);
  space->addMapping(KEY_2, KEY_F2);
  space->addMapping(KEY_3, KEY_F3);
  space->addMapping(KEY_4, KEY_F4);
  space->addMapping(KEY_5, KEY_F5);
  space->addMapping(KEY_6, KEY_F6);
  space->addMapping(KEY_7, KEY_F7);
  space->addMapping(KEY_8, KEY_F8);
  space->addMapping(KEY_9, KEY_F9);
  space->addMapping(KEY_0, KEY_F10);
  space->addMapping(KEY_MINUS, KEY_F11);
  space->addMapping(KEY_EQUAL, KEY_F12);

  // xf86 audio
  space->addMapping(KEY_M, KEY_MUTE);
  space->addMapping(KEY_COMMA, KEY_VOLUMEDOWN);
  space->addMapping(KEY_DOT, KEY_VOLUMEUP);

  // mouse navigation
  space->addMapping(BTN_LEFT, BTN_BACK);
  space->addMapping(BTN_RIGHT, BTN_FORWARD);

  // @FIXME: this is not working, even though `wev` says keycode 99 is Print
  // PrtSc -> Context Menu
  space->addMapping(KEY_SYSRQ, KEY_CONTEXT_MENU);

  // tap caps for esc, hold for ctrl
  InterceptedKeyModifier *caps =
      new InterceptedKeyModifier(KEY_CAPSLOCK, KEY_ESC, KEY_LEFTCTRL);

  // tap enter for enter, hold for ctrl
  InterceptedKeyModifier *enter =
      new InterceptedKeyModifier(KEY_ENTER, KEY_ENTER, KEY_RIGHTCTRL);

  // @NOTE: modifier keys must go first because layerKey.processInterceptedHeld
  // emits mapped key as soon as the for loop calls layerKey.process..
  // if that process is run before modifierKey.process, the modifier key will
  // not be emitted
  vector<InterceptedKey *> *interceptedKeys = new vector<InterceptedKey *>();
  interceptedKeys->push_back(caps);
  interceptedKeys->push_back(enter);
  interceptedKeys->push_back(space);
  return interceptedKeys;
}

int main() {
  auto interceptedKeys = initInterceptedKeys();

  setbuf(stdin, NULL), setbuf(stdout, NULL);

  /* event *input = (event*) malloc(input_event_struct_size); */
  Event *input = new Event();
  while (readEvent(input)) {
    if (input->type == EV_MSC && input->code == MSC_SCAN)
      continue;

    if (input->type != EV_KEY) {
      writeEvent(input);
      continue;
    }

    /* cerr << input->type << "," << input->code << " "; */

    bool shouldEmitInput = true;
    for (auto key : *interceptedKeys) {
      shouldEmitInput &= key->process(input);
    }

    if (shouldEmitInput) {
      writeEvent(input);
    }
  }

  /* free(input); */
  delete input;
  delete interceptedKeys;
}
