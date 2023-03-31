# Interception plugin for vimproved input

~My hideous~ Humble, performant key remapping C++ code that should work on Linux for any input device that emits keys.

## tl;dr;

- space cadet: Maps space key into modifier (layer) key when held for long that transforms hjkl (vim homerow) into arrows, number row into F-keys, and a bit more. Space still emits SPACE key when tapped without holding.
- caps2esc: Makes CAPS key send ESC when tapped and L_CTRL when held.
- return2ctrl: Makes RETURN key send RETURN when tapped and R_CTRL when held.
- other possible uses:
  - Map CAPS to ESC when tapped and to LEFTMETA when held (useful with window managers like `dwm`)
  - Declare any key (i.e. LEFTALT) to be a custom layer key when held

## Requirements
Basically any OS that works with `libevdev` (linux with kernel newer than 2.6.36), no matter what desktop environment, or even if any DE is used (yes, it works the same in X server instead of `xmodmap`, but also in plain terminal without graphical environment).
For building, `meson` (and `ninja`), and `make`.
At runtime, the `https://github.com/jbeder/yaml-cpp` YAML library.

## Installation
```bash
$ git clone "https://github.com/maricn/interception-vimproved"
$ cd interception-vimproved
$ sudo make install
```

## Running
Use it with a job specification for `udevmon` (from [Interception Tools](https://gitlab.com/interception/linux/tools)). I install the binary to `/opt/interception/interception-vimproved` and use it like the following on Arch linux on Thinkpad x1c gen7.

```yaml
- JOB: "intercept -g $DEVNODE | interception-vimproved | uinput -d $DEVNODE"
  DEVICE:
    NAME: ".*((k|K)(eyboard|EYBOARD)|TADA68).*"
```

That matches any udev devices containing keyboard in the name (or my external TADA68 keyboard).

Alternatively, you can run it with `udevmon` binary straight, just make sure to be negatively nice (`nice -n -20 udevmon -c /etc/udevmon.yml`) so your input is always available.

### Configuration
If you want to customize the functionality, you can take a look at the [`config.yaml`](./config.yaml).
The configuration file is copied to `/etc/interception-vimproved/config.yaml` when invoking `sudo make install`.
You can configure that file and add its path as an argument to `interception-vimproved` when run by updating your `/etc/interception/udevmon.yaml`:
```yaml
...
- JOB:
  - |
    intercept -g $DEVNODE \
    | interception-vimproved /etc/interception-vimproved/config.yaml \
    | uinput -d $DEVNODE
...
```


### Testing
In case you want to edit the source code, kill the `udevmon` daemon, and manually try the following to avoid getting stuck with broken input. Trust me, you can get yourself in a dead end situation easily.

```bash
# sleep buys you some time to focus away from terminal to your playground, also you'll probably need to add a sudo
sleep 1 && timeout 10 udevmon -c /etc/udevmon.yml
```

## Why make this
1. I have problems switching back and forth between my external keyboard and laptop keyboard. I customized my external keyboard with QMK to reduce my pinky strain and improve usability, but when I switch back to laptop keyboard, it's all lost, plus I have to fight my muscle memory.
2. I used to use X.Org server with xinput, where I had an [xkbcomp based solution with xcape and xmodmap](https://github.com/maricn/dotfiles/blob/master/.xinitrc-keyboard-remap). However, since moving to wayland, that solution doesn't work anymore, and I needed to move to `libevdev` based solution.
3. Enter [Interception Tools](https://gitlab.com/interception/linux/tools). It advertises itself as "a minimal composable infrastructure on top of libudev and libevdev". It, on one side, intercepts events from devices and writes them raw to `stdout`, and on the other side, it receives events from `stdin` and writes them to virtual input device.
4. This plugin sits piped between Interception Tool's `intercept` and `uinput` (`intercept | interception-vimproved | uinput`). It interprets input from `intercept` and maps it to desired events which are then passed to `uinput` for emitting.

- Other solutions I tried didn't behave as expected and produced unexpected artefacts.
- In this solution, the use of sleeps is eliminated, there's no buffers for input and the interaction with output is minimized to reduce extra costs of writing to it often.

## Acknowledgments

Kudos to [@dceluis](https://github.com/dceluis) and [@exprpapi](https://github.com/exprpapi) for their contributions.

### Related work / inspiration

- [Interception Tools](https://gitlab.com/interception/linux/tools)
- [space2meta](https://gitlab.com/interception/linux/plugins/space2meta)
- [dual-function-keys](https://gitlab.com/interception/linux/plugins/dual-function-keys)
- [interception-k2k](https://github.com/zsugabubus/interception-k2k)
