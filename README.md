tec
===

Framework for making tiny games with [elis](https://github.com/ooichu/elis) language.

![demo](https://github.com/ooichu/tec/assets/50176726/fa28258b-6c1b-4ce1-ac12-a8c7d6669433)

Getting started
---------------

Compile `tec.c` with `build.sh` (SDL2 required) and run the resulting executable with path to game
script. For example, execute `(cd demo; ../tec main.elis)` to run demo.

Configuration
-------------

When `tec` is started, main game script is executed. Other scripts can be loaded from main using
the `(load "script.elis")` function. Once is loaded, the following **constants** are expected to be
defined in global scope:

| Constant |                                      Value                                      |
|----------|---------------------------------------------------------------------------------|
| `TITLE`  | window name                                                                     |
| `ICON`   | window icon                                                                     |
| `WIDTH`  | window width                                                                    |
| `HEIGHT` | window height                                                                   |
| `SCALE`  | how many times window should be stretched                                       |
| `FPS`    | target FPS                                                                      |
| `DEBUG`  | print current FPS (not a constant) to `stderr`?                                 |
| `COLORS` | list of available colors, e.g. `((255 0 255) (10 128 80) (228 113 84))`         |
| `IMAGES` | list of available images, should contain triplets: variable, filename, colorkey |
| `SOUNDS` | list of available sounds, should contain triplets: variable, filename, volume   |

The game loop consists of three stages:

| Callback |            When called             |
|----------|------------------------------------|
| `init`   | after the framework initialization |
| `step`   | every frame                        |
| `quit`   | before closing the application     |

Example of configuration can be found in `demo/config.elis` and `demo/main.elis`.

Graphics
--------

The graphics system only allows you to draw images and filled rectangles.

|             Function             |                          Purpose                          |
|----------------------------------|-----------------------------------------------------------|
| `(fill col [x y w h])`           | fill whole screen or rectangle                            |
| `(peek x y)`                     | peek screen pixel                                         |
| `(draw x y img {spr, str, map})` | draw sprite, string or map                                |
| `(clip [x y w h])`               | clip screen (if any arguments passed) or return clip rect |
| `(camera [x y])`                 | set camera (if any arguments passed) or return camera     |
| `(width [img, map])`             | get width of screen, image or map                         |
| `(height [img, map])`            | get height of screen, image or map                        |
| `(tilemap filename)`             | create new tilemap from file                              |
| `(tile map x y [tile])`          | get/set tile on map                                       |

Only BMP format images are supported. All images automatically converted to `COLORS` palette.
Images to be used as spritesheets must have a resolution `WxH`, where `W` — width and height of
sprite and `H = W * number of sprites`. If an image is used as font, then it must contain 95
sprites (for 32-126 ASCII characters). Examples of images can be found in `demo/images/`.

A tilemap is a matrix of sprite indexes. A tilemap is specified by a file that must contain only
numbers separated by spaces. The first two numbers are the width and height of the map, and the
rest are the tiles themselves. Examples of tilemaps can be found in `demo/maps/`.

Audio
-----

The audio system in quite simple. It supports only sounds and music (looping sounds).

|       Function       |                           Purpose                            |
|----------------------|--------------------------------------------------------------|
| `(play sound loop?)` | play `sound`, if `loop?` isn't `nil` — play/restart music    |
| `(stop [sound])`     | stop all sounds and music, or stop music with `sound` source |
| `(mute yes?)`        | global mute/unmute                                           |

Only WAV sounds are supported. Only one music using a certain sound, the sound can play at the same
time.  Examples of using the audio system can be found in the `demo/`.

Input
-----

You can read input only from keyboard and mouse.

|    Function     |                               Get                               |
|-----------------|-----------------------------------------------------------------|
| `(key keyname)` | keyboard [key](https://wiki.libsdl.org/SDL2/SDL_Scancode) state |
| `(mouse "l")`   | left mouse key                                                  |
| `(mouse "m")`   | middle mouse key                                                |
| `(mouse "r")`   | right mouse key                                                 |
| `(mouse "x")`   | mouse position at x-axis                                        |
| `(mouse "y")`   | mouse position at y-axis                                        |
| `(mouse "w")`   | mouse wheel state                                               |

Miscellaneous
-------------

|       Function       |                        Purpose                         |
|----------------------|--------------------------------------------------------|
| `(load filename)`    | load script                                            |
| `(exit)`             | close app                                              |
| `(time)`             | get current time from app start (in seconds)           |
| `(each list func)`   | apply `func` to each element of `list`                 |
| `(each number func)` | apply `func` to each integer number from 0 to `number` |
| `(random [n [m]])`   | generate random number                                 |

Math
----

|   Function    |                Purpose                |
|---------------|---------------------------------------|
| `(min ...)`   | get smallest number from arguments    |
| `(max ...)`   | get biggest number from arguments     |
| `(sign x)`    | get sign of number (zero is positive) |
| `(abs x)`     | get absolute value                    |
| `(sin x)`     | sine function                         |
| `(cos x)`     | cosine function                       |
| `(pow x y)`   | power function                        |
| `(sqrt x)`    | square root function                  |
| `(atan2 x y)` | arc tangent function of two variables |
| `(floor x)`   | round down                            |
| `(round x)`   | round to nearest                      |
| `(ceil x)`    | round up                              |

Strings
-------

|         Function         |                           Purpose                            |
|--------------------------|--------------------------------------------------------------|
| `(char str)`             | convert character number to string                           |
| `(number str)`           | convert string to number (return `nil` if fails)             |
| `(symbol str)`           | convert string to symbol                                     |
| `(string ...)`           | convert and concatenate all arguments                        |
| `(strlen str)`           | get length of string                                         |
| `(ascii str)`            | convert string to character number                           |
| `(substr str start len)` | get substring of `len` characters starting at `start` offset |
