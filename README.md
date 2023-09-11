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
| `WIDTH`  | window width                                                                    |
| `HEIGHT` | window height                                                                   |
| `SCALE`  | how many times window should be stretched                                       |
| `FPS`    | target FPS                                                                      |
| `COLORS` | list of available colors, should contain triplets: R, G, B (in range 0-255)     |
| `IMAGES` | list of available images, should contain triplets: variable, filename, colorkey |
| `SOUNDS` | list of available sounds, should contain triplets: variable, filename, volume   |

After initialization, all configuration constants are set to nil.

The game loop consists of three stages:

| Callback |            When called             |
|----------|------------------------------------|
| `init`   | after the framework initialization |
| `step`   | every frame                        |

Example of configuration can be found in `demo/config.elis` and `demo/main.elis`.

Graphics
--------

The graphics system only allows you to draw images and filled rectangles.

|             Function             |                          Purpose                          |
|----------------------------------|-----------------------------------------------------------|
| `(clear col [map])`              | clear screen or map                                       |
| `(fill col x y w h)`             | fill screen rectangle                                     |
| `(fill col x y map)`             | set tile on map                                           |
| `(peek x y [map])`               | peek screen pixel or map tile                             |
| `(draw x y img sprite-num)`      | draw sprite, `img` is spritesheet image                   |
| `(draw x y img string)`          | draw string, `img` is font                                |
| `(draw x y img map)`             | draw tilemap, `img` is tilesheet                          |
| `(clip [x y w h])`               | clip screen (if any arguments passed) or return clip rect |
| `(camera [pos])`                 | get camera position or set camera to `pos = (x . y)`      |
| `(width [img \| map])`           | get width of screen, image or map                         |
| `(height [img \| map])`          | get height of screen, image or map                        |
| `(tilemap filename)`             | create new tilemap from file                              |
| `(tilemap w h)`                  | create blank tilemap                                      |

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

|       Function        |                           Purpose                            |
|-----------------------|--------------------------------------------------------------|
| `(play sound music?)` | play `sound`, if `music?` isn't `nil` -- start music         |
| `(stop music)`        | stop music                                                   |
| `(pause music)`       | pause music                                                  |
| `(play)`              | play all paused sounds                                       |
| `(stop)`              | stop all sounds                                              |
| `(pause)`             | pause all sounds                                             |

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
| `(exit)`             | close app                                              |
| `(time)`             | get current time from app start (in seconds)           |
| `(load filename)`    | load script                                            |
| `(type any)`         | get type name of `any` as string                       |
| `(sort list func)`   | sort `list` using `func` as compare function           |
| `(random [n [m]])`   | generate random number                                 |

Math
----

|   Function    |                Purpose                |
|---------------|---------------------------------------|
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
