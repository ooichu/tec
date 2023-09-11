/*
 * Copyright (c) 2023 ooichu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <tgmath.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "elis/elis.h"

/*
 * Globals
 */

typedef struct { int width, height, tiles[]; } Tilemap;
typedef struct { uint32_t length; int volume; uint8_t *buffer; } Sound;

static elis_State *S;
static int wheel;
static uint64_t time_step;

/* window */
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *pixels; /* raw RGBA pixels */
static SDL_Rect viewport;

/* audio */
static SDL_AudioDeviceID device;
static SDL_AudioSpec audio;

/* virtual screen */
static uint8_t *screen;
static int width, height, scale;
static struct { elis_Number x, y; } camera;
static struct { int x0, y0, x1, y1; } clip;

/* fixed palette */
static uint32_t colors[256];
static int num_colors;

/* circle buffer of sound sources */
static struct { Sound *sound; uint32_t position; } *sources;
static int max_sources = 64, num_sources, next_source; 

/*
 * Userdata objects
 */

static elis_Object *free_image(elis_State *S, elis_Object *obj) {
  SDL_FreeSurface(elis_to_userdata(S, obj, NULL));
  return NULL; 
}

static elis_Object *free_sound(elis_State *S, elis_Object *obj) {
  Sound *sound = elis_to_userdata(S, obj, NULL);
  free(sound->buffer);
  free(sound);
  return NULL;
}

static elis_Object *free_tilemap(elis_State *S, elis_Object *obj) {
  free(elis_to_userdata(S, obj, NULL));
  return NULL; 
}

static elis_Handlers image_handlers   = { .free = free_image   };
static elis_Handlers sound_handlers   = { .free = free_sound   };
static elis_Handlers tilemap_handlers = { .free = free_tilemap };

static void *to_userdata(elis_State *S, elis_Object *obj, elis_Handlers *type) {
  elis_Handlers *hdls = NULL;
  void *udata = elis_to_userdata(S, obj, &hdls);
  return hdls == type ? udata : NULL;
}

/*
 * API: utils
 */

static elis_Object *f_exit(elis_State *S, elis_Object *args) {
  (void) S;
  (void) args;
  exit(EXIT_SUCCESS);
}

static elis_Object *f_time(elis_State *S, elis_Object *args) {
  (void) args;
  return elis_number(S, SDL_GetTicks() / 1000.0);
}

static elis_Object *load(elis_State *S, const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) elis_error(S, "failed to open script");
  /* read and eval all forms from file */
  int gc = elis_save_gc(S);
  elis_Object *res = elis_bool(S, false);
  for (;;) {
    elis_restore_gc(S, gc);
    elis_Object *obj = elis_read_fp(S, fp);
    if (!obj) break;
    res = elis_eval(S, obj);
  }
  fclose(fp);
  return res;
}

static elis_Object *f_load(elis_State *S, elis_Object *args) {
  return load(S, elis_to_string(S, elis_next_arg(S, &args)));
}

static elis_Object *f_type(elis_State *S, elis_Object *args) {
  return elis_string(S, elis_typenames[elis_type(S, elis_next_arg(S, &args))]);
}

#define SORT_MAX_RECURSION 16

static struct { elis_State *S; elis_Object *func; int gc; } sort_stack[SORT_MAX_RECURSION],
                                                            *sort_args = sort_stack + SORT_MAX_RECURSION;

static int compare(const void *a, const void *b) {
  elis_State *S = sort_args->S;
  elis_Object *args = elis_list(S, (elis_Object *[]) { *(elis_Object **) a, *(elis_Object **) b }, 2);
  int ret = elis_to_number(S, elis_apply(S, sort_args->func, args));
  elis_restore_gc(S, sort_args->gc);
  return ret;
}

static elis_Object *f_sort(elis_State *S, elis_Object *args) {
  if (sort_args-- == sort_stack) elis_error(S, "too many nested sorting calls");
  /* copy list to plain array */
  size_t cap = 64, len = 0;
  elis_Object **arr = malloc(cap * sizeof(*arr)), *lst = elis_next_arg(S, &args);
  while (!elis_nil(S, lst)) {
    if (len == cap) {
      cap <<= 1;
      arr = realloc(arr, cap * sizeof(*arr));
    }
    arr[len++] = elis_next_arg(S, &lst);
  }
  /* sort using `qsort()` */
  sort_args->S = S;
  sort_args->func = elis_next_arg(S, &args);
  sort_args->gc = elis_save_gc(S);
  qsort(arr, len, sizeof(*arr), compare);
  /* convert sorted array back to list */
  lst = elis_bool(S, false);
  int gc = elis_save_gc(S);
  while (len) {
    lst = elis_cons(S, arr[--len], lst);
    elis_restore_gc(S, gc);
    elis_push_gc(S, lst);
  }
  free(arr);
  ++sort_args;
  return lst;
}

static elis_Object *f_random(elis_State *S, elis_Object *args) {
  /* generate random number from 0 to 1 */
  elis_Number x = rand() / (elis_Number) RAND_MAX;
  if (elis_nil(S, args)) return elis_number(S, x);
  /* generate random number from 0 to n */
  elis_Number n = elis_to_number(S, elis_next_arg(S, &args));
  if (elis_nil(S, args)) return elis_number(S, x * n);
  /* generate random number from n to m */
  elis_Number m = elis_to_number(S, elis_next_arg(S, &args));
  return elis_number(S, n + x * (m - n));
} 

/*
 * API: graphics
 */

static elis_Object *f_clear(elis_State *S, elis_Object *args) {
  int col = elis_to_number(S, elis_next_arg(S, &args));
  if (elis_nil(S, args)) {
    memset(screen, col % num_colors, width * height);
  } else {
    Tilemap *map = to_userdata(S, args, &tilemap_handlers);
    for (int i = 0; i < map->width * map->height; ++i) map->tiles[i] = col;
  }
  return elis_bool(S, false);
}

static elis_Object *f_fill(elis_State *S, elis_Object *args) {
  int c = elis_to_number(S, elis_next_arg(S, &args));
  int x = elis_to_number(S, elis_next_arg(S, &args));
  int y = elis_to_number(S, elis_next_arg(S, &args));
  switch (elis_type(S, elis_car(S, args))) {
    case ELIS_NUMBER: {
      int w = elis_to_number(S, elis_next_arg(S, &args));
      int h = elis_to_number(S, elis_next_arg(S, &args));
      c %= num_colors, x += camera.x, y += camera.y;
      /* do horizontal clip */
      if (x + w >= clip.x1) w = clip.x1 - x;
      if (x < clip.x0) w += x - clip.x0, x = clip.x0;
      /* early exit if completely clipped */
      if (w > 0) {
        /* do vertical clip */
        if (y + h >= clip.y1) h = clip.y1 - y;
        if (y < clip.y0) h += y - clip.y0, y = clip.y0;
        /* fill scanlines */
        for (uint8_t *row = screen + x + y * width; h-- > 0; row += width) memset(row, c, w);
      }
      break;
    }
    case ELIS_USERDATA: {
      Tilemap *map = to_userdata(S, elis_next_arg(S, &args), &tilemap_handlers);
      if (x >= 0 && x < map->width && y >= 0 && y < map->height) map->tiles[x + y * map->width] = c;
      break;
    }
    default: elis_error(S, "expected number or map");
  }
  return elis_bool(S, false);
}

static elis_Object *f_peek(elis_State *S, elis_Object *args) {
  int x = elis_to_number(S, elis_next_arg(S, &args));
  int y = elis_to_number(S, elis_next_arg(S, &args));
  if (!elis_nil(S, args)) {
    Tilemap *map = to_userdata(S, elis_next_arg(S, &args), &tilemap_handlers);
    return elis_number(S, x < 0 || x >= map->width || y < 0 || y >= map->height
                          ? 0
                          : map->tiles[x + y * width]);
  }
  return elis_number(S, x < 0 || x >= width || y < 0 || y >= height ? 0 : screen[x + y * width]);
}

static void draw(SDL_Surface *surface, int x, int y, int s) {
  int w = surface->w, h = w, sx = 0, sy = (s * w % surface->h + surface->h) % surface->h;
  /* do horizontal clip */
  if (x + w >= clip.x1) w = clip.x1 - x;
  if (x < clip.x0) x -= clip.x0, sx -= x, w += x, x = clip.x0;
  if (w <= 0) return;
  /* do vertical clip */
  if (y + h >= clip.y1) h = clip.y1 - y;
  if (y < clip.y0) y -= clip.y0, sy -= y, h += y, y = clip.y0;
  /* copy scanlines from sprite to virtual screen */
  uint8_t *dst = (uint8_t *) screen + x + y * width;
  uint8_t *src = (uint8_t *) surface->pixels + sx + sy * surface->pitch;
  uint8_t key = (uintptr_t) surface->userdata;
  while (h-- > 0) {
    for (int i = 0; i < w; ++i) {
      if (src[i] != key) dst[i] = src[i];
    }
    dst += width, src += surface->pitch;
  }
}

static elis_Object *f_draw(elis_State *S, elis_Object *args) {
  int x = elis_to_number(S, elis_next_arg(S, &args)) + camera.x;
  int y = elis_to_number(S, elis_next_arg(S, &args)) + camera.y;
  SDL_Surface *surface = to_userdata(S, elis_next_arg(S, &args), &image_handlers);
  args = elis_next_arg(S, &args);
  switch (elis_type(S, args)) {
    case ELIS_NUMBER:
      draw(surface, x, y, elis_to_number(S, args));
      break;
    case ELIS_STRING:
      for (const char *str = elis_to_string(S, args); *str; ++str, x += surface->w) {
        if (*str >= ' ' && *str <= '~') draw(surface, x, y, *str - 32);
      }
      break;
    case ELIS_USERDATA: {
      Tilemap *map = to_userdata(S, args, &tilemap_handlers);
      for (int ty = 0, py = y; ty < map->height; ++ty, py += surface->w) {
        for (int tx = 0, px = x; tx < map->width; ++tx, px += surface->w) {
          draw(surface, px, py, map->tiles[tx + ty * map->width]);
        }
      }
      break;
    }
    default: elis_error(S, "expected number, string or map");
  }
  return elis_bool(S, false);
}

static elis_Object *f_clip(elis_State *S, elis_Object *args) {
  if (elis_nil(S, args)) {
    elis_Object *objs[] = {
      elis_number(S, clip.x0),
      elis_number(S, clip.y0),
      elis_number(S, clip.x1 - clip.x0),
      elis_number(S, clip.y1 - clip.y0)
    };
    /* get clip rect */
    return elis_list(S, objs, 4);
  }
  /* start point */
  clip.x0 = elis_to_number(S, elis_next_arg(S, &args));
  clip.y0 = elis_to_number(S, elis_next_arg(S, &args));
  /* width and height */
  clip.x1 = elis_to_number(S, elis_next_arg(S, &args));
  clip.y1 = elis_to_number(S, elis_next_arg(S, &args));
  /* width and height positive? */
  if (clip.x1 > 0 && clip.y1 > 0) {
    /* clip rect */
    clip.x1 += clip.x0;
    clip.y1 += clip.y0;
    clip.x0 = clip.x0 > 0 ? clip.x0 : 0;
    clip.y0 = clip.y0 > 0 ? clip.y0 : 0;
    clip.x1 = clip.x1 > width  ? width  : clip.x1;
    clip.y1 = clip.y1 > height ? height : clip.y1;
  } else {
    /* zero rect */
    clip.x1 = clip.x0;
    clip.y1 = clip.y0;
  }
  return elis_bool(S, false);
}

static elis_Object *f_camera(elis_State *S, elis_Object *args) {
  elis_Object *res = elis_cons(S, elis_number(S, camera.x), elis_number(S, camera.y));
  if (!elis_nil(S, args)) {
    elis_Object *obj = elis_next_arg(S, &args);
    camera.x = floor(elis_to_number(S, elis_car(S, obj)));
    camera.y = floor(elis_to_number(S, elis_cdr(S, obj)));
  }
  return res;
}

static elis_Object *f_width(elis_State *S, elis_Object *args) {
  if (elis_nil(S, args)) return elis_number(S, width);
  elis_Handlers *type = NULL;
  void *udata = elis_to_userdata(S, elis_next_arg(S, &args), &type);
  if (type == &image_handlers) return elis_number(S, ((SDL_Surface *) udata)->w);
  if (type == &tilemap_handlers) return elis_number(S, ((Tilemap *) udata)->width);
  elis_error(S, "expected image or map");
  return NULL;
}

static elis_Object *f_height(elis_State *S, elis_Object *args) {
  if (elis_nil(S, args)) return elis_number(S, height);
  elis_Handlers *type = NULL;
  void *udata = elis_to_userdata(S, elis_next_arg(S, &args), &type);
  if (type == &image_handlers) return elis_number(S, ((SDL_Surface *) udata)->h);
  if (type == &tilemap_handlers) return elis_number(S, ((Tilemap *) udata)->height);
  elis_error(S, "expected image or map");
  return NULL;
}

static int parse_int(FILE *fp) {
  elis_Object *obj = elis_read_fp(S, fp);
  if (!obj) elis_error(S, "bad map format");
  return elis_to_number(S, obj);
}

static elis_Object *f_tilemap(elis_State *S, elis_Object *args) {
  FILE *fp = NULL;
  int w, h;
  /* load map from file or create blank map? */
  if (elis_nil(S, elis_cdr(S, args))) {
    fp = fopen(elis_to_string(S, elis_next_arg(S, &args)), "r");
    if (!fp) elis_error(S, "failed to open map");
    w = parse_int(fp), h = parse_int(fp);
  } else {
    w = elis_to_number(S, elis_next_arg(S, &args)), h = elis_to_number(S, elis_next_arg(S, &args));
  }
  /* allocate map */
  Tilemap *map = calloc(1, sizeof(*map) + w * h * sizeof(int));
  map->width = w;
  map->height = h;
  /* load map contents from file */
  if (fp) {
    for (int i = 0, gc = elis_save_gc(S); (args = elis_read_fp(S, fp)) && i < w * h; ++i) {
      map->tiles[i] = elis_to_number(S, args);
      elis_restore_gc(S, gc);
    }
    fclose(fp);
  }
  return elis_userdata(S, map, &tilemap_handlers);
}

/*
 * API: audio
 */

enum { SOUND = 0x0, MUSIC = 0x1, PAUSE = 0x2, ANY = MUSIC | PAUSE | SOUND };

#define GET_TAG(s)    ((uintptr_t) (s) & ANY)
#define SET_TAG(s, T) ((Sound *) ((uintptr_t) (s) | T))
#define UNTAG(s, T)   ((Sound *) ((uintptr_t) (s) & ~(uintptr_t) (T)))

#define ANY_SOUND        ((Sound *) ~(uintptr_t) ANY)
#define SAME_SOUND(a, b) (((uintptr_t) a & ~(uintptr_t) b) <= ANY)

static elis_Object *f_play(elis_State *S, elis_Object *args) {
  Sound *sound = elis_nil(S, args) ? ANY_SOUND
               : to_userdata(S, elis_next_arg(S, &args), &sound_handlers);
  /* do some special actions for music source */
  bool resumed = elis_nil(S, args);
  if (sound == ANY_SOUND || resumed || !elis_nil(S, elis_next_arg(S, &args))) {
    /* that music is already playing or paused? */
    for (int i = 0; i < max_sources; ++i) {
      if (sources[i].sound && SAME_SOUND(sources[i].sound, sound)) {
        sources[i].sound = UNTAG(sources[i].sound, PAUSE);
        resumed = true;
      }
    }
    if (sound == ANY_SOUND || resumed) return elis_bool(S, false);
    /* mark source as music */
    sound = SET_TAG(sound, MUSIC);
  }
  /* time to strech source buffer? */
  if (num_sources == max_sources) {
    int offset = max_sources;
    next_source = max_sources;
    max_sources += max_sources >> 2;
    sources = realloc(sources, max_sources * sizeof(*sources));
    memset(sources + offset, 0, (max_sources - offset) * sizeof(*sources));
  }
  /* find free place for new source */
  while (sources[next_source].sound) next_source = (next_source + 1) % max_sources;
  /* add new source to queue */
  sources[next_source].sound = sound;
  sources[next_source].position = 0;
  ++num_sources;
  return elis_bool(S, false);
}

static elis_Object *f_stop(elis_State *S, elis_Object *args) {
  Sound *sound = elis_nil(S, args) ? ANY_SOUND
               : to_userdata(S, elis_next_arg(S, &args), &sound_handlers);
  /* find and stop music */
  for (int i = 0; i < max_sources; ++i) {
    if (sources[i].sound && SAME_SOUND(sources[i].sound, sound)) {
      sources[i].sound = NULL;
    }
  }
  return elis_bool(S, false);
}

static elis_Object *f_pause(elis_State *S, elis_Object *args) {
  Sound *sound = elis_nil(S, args) ? ANY_SOUND
               : to_userdata(S, elis_next_arg(S, &args), &sound_handlers);
  /* find and pause music */
  for (int i = 0; i < max_sources; ++i) {
    if (sources[i].sound && SAME_SOUND(sources[i].sound, sound)) {
      sources[i].sound = SET_TAG(sources[i].sound, PAUSE);
    }
  }
  return elis_bool(S, false);
}

/*
 * API: input
 */

static elis_Object *f_key(elis_State *S, elis_Object *args) {
  SDL_Scancode code = SDL_GetScancodeFromName(elis_to_string(S, elis_next_arg(S, &args)));
  return elis_bool(S, code == SDL_SCANCODE_UNKNOWN ? false : SDL_GetKeyboardState(NULL)[code]);
}

static inline int umax(int x, int max) {
  return x < 0 ? 0 : x > max ? max : x;
}

static elis_Object *f_mouse(elis_State *S, elis_Object *args) {
  int x, y;
  uint8_t keys = SDL_GetMouseState(&x, &y); 
  switch (*elis_to_string(S, elis_next_arg(S, &args))) {
    case 'L': case 'l': return elis_bool(S, keys & SDL_BUTTON(SDL_BUTTON_LEFT));
    case 'M': case 'm': return elis_bool(S, keys & SDL_BUTTON(SDL_BUTTON_MIDDLE));
    case 'R': case 'r': return elis_bool(S, keys & SDL_BUTTON(SDL_BUTTON_RIGHT));
    case 'W': case 'w': return elis_number(S, wheel);
    case 'X': case 'x': return elis_number(S, umax((x - viewport.x) / scale, width));
    case 'Y': case 'y': return elis_number(S, umax((y - viewport.y) / scale, height));
  }
  return elis_bool(S, false);
}

/*
 * API: math
 */

static elis_Object *f_abs(elis_State *S, elis_Object *args) {
  return elis_number(S, fabs(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_sin(elis_State *S, elis_Object *args) {
  return elis_number(S, sin(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_cos(elis_State *S, elis_Object *args) {
  return elis_number(S, cos(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_pow(elis_State *S, elis_Object *args) {
  elis_Number x = elis_to_number(S, elis_next_arg(S, &args));
  elis_Number y = elis_to_number(S, elis_next_arg(S, &args));
  return elis_number(S, pow(x, y));
}

static elis_Object *f_sqrt(elis_State *S, elis_Object *args) {
  return elis_number(S, sqrt(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_atan2(elis_State *S, elis_Object *args) {
  elis_Number x = elis_to_number(S, elis_next_arg(S, &args));
  elis_Number y = elis_to_number(S, elis_next_arg(S, &args));
  return elis_number(S, atan2(x, y));
}

static elis_Object *f_floor(elis_State *S, elis_Object *args) {
  return elis_number(S, floor(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_round(elis_State *S, elis_Object *args) {
  return elis_number(S, round(elis_to_number(S, elis_next_arg(S, &args))));
}

static elis_Object *f_ceil(elis_State *S, elis_Object *args) {
  return elis_number(S, ceil(elis_to_number(S, elis_next_arg(S, &args))));
}

/*
 * API: string
 */

static elis_Object *f_char(elis_State *S, elis_Object *args) {
  char chr = elis_to_number(S, elis_next_arg(S, &args));
  return elis_substring(S, &chr, 1);
}

static elis_Object *f_number(elis_State *S, elis_Object *args) {
  char *ptr = NULL;
  const char *str = elis_to_string(S, elis_next_arg(S, &args));
  while (*str && strchr(" \n\t\r", *str)) ++str;
  elis_Number num = strtod(str, &ptr);
  return ptr != str && *ptr == '\0' ? elis_number(S, num) : elis_bool(S, false);
}

static elis_Object *f_symbol(elis_State *S, elis_Object *args) {
  elis_Object *obj = elis_next_arg(S, &args);
  const char *str = elis_to_string(S, obj);
  return str ? elis_symbol(S, str) : obj;
}

typedef struct { unsigned capacity, length; char *string; } StringBuffer;

static void write_buffer(elis_State *S, void *udata, char chr) {
  StringBuffer *buf = udata;
  (void) S;
  if (buf->length == buf->capacity) {
    buf->capacity = (buf->capacity + 1) << 1;
    buf->string = realloc(buf->string, buf->capacity);
  }
  buf->string[buf->length++] = chr;
}

static elis_Object *f_string(elis_State *S, elis_Object *args) {
  StringBuffer buf = { 64, 0, NULL };
  buf.string = malloc(buf.capacity + 1);
  int gc = elis_save_gc(S);
  /* stringify and concatenate all arguments */
  do {
    elis_Object *obj = elis_next_arg(S, &args);
    /* append string to end of buffer */
    if (elis_type(S, obj) == ELIS_STRING) {
      const char *str = elis_to_string(S, obj);
      while (*str) write_buffer(S, &buf, *str++);
    } else {
      elis_write(S, obj, write_buffer, &buf); 
    }
    elis_restore_gc(S, gc);
  } while (!elis_nil(S, args));
  /* create string and free temponary buffer */
  args = elis_substring(S, buf.string, buf.length);
  free(buf.string);
  return args;
}

static elis_Object *f_strlen(elis_State *S, elis_Object *args) {
  return elis_number(S, strlen(elis_to_string(S, elis_next_arg(S, &args))));
}

static elis_Object *f_ascii(elis_State *S, elis_Object *args) {
  const char *str = elis_to_string(S, elis_next_arg(S, &args));
  int idx = elis_nil(S, args) ? 0 : elis_to_number(S, elis_next_arg(S, &args));
  return elis_number(S, str[idx % strlen(str)]);
}

static elis_Object *f_substr(elis_State *S, elis_Object *args) {
  const char *str = elis_to_string(S, elis_next_arg(S, &args));
  int str_len = strlen(str);
  int start = elis_to_number(S, elis_next_arg(S, &args));
  int len = elis_to_number(S, elis_next_arg(S, &args));
  /* clip substring bounds */
  if (len > 0) {
    if (start < 0) start = str_len % start;
    if (start > str_len) start = str_len;
    if (start + len > str_len) len = str_len - start;
  }
  /* create substring */
  return elis_substring(S, str + start, len);
}

/*
 * Complete API list
 */

static struct { const char *name; elis_CFunction func; } functions[] = {
  /*       utils        */
  { "exit",    f_exit   },
  { "time",    f_time   },
  { "load",    f_load   },
  { "type",    f_type   },
  { "sort",    f_sort   },
  { "random",  f_random },
  /*      graphics     */
  { "clear",   f_clear  },
  { "fill",    f_fill   },
  { "peek",    f_peek   },
  { "draw",    f_draw   },
  { "clip",    f_clip   },
  { "camera",  f_camera },
  { "width",   f_width  },
  { "height",  f_height },
  { "tilemap", f_tilemap },
  /*       audio        */
  { "play",    f_play    },
  { "stop",    f_stop    },
  { "pause",   f_pause   },
  /*       input        */
  { "key",     f_key     },
  { "mouse",   f_mouse   },
  /*        math        */
  { "abs",     f_abs     },
  { "sin",     f_sin     },
  { "cos",     f_cos     },
  { "pow",     f_pow     },
  { "sqrt",    f_sqrt    },
  { "atan2",   f_atan2   },
  { "floor",   f_floor   },
  { "round",   f_round   },
  { "ceil",    f_ceil    },
  /*       string       */
  { "char",    f_char    },
  { "number",  f_number  },
  { "symbol",  f_symbol  },
  { "string",  f_string  },
  { "strlen",  f_strlen  },
  { "ascii",   f_ascii   },
  { "substr",  f_substr  },
  /*         end        */
  { NULL,      NULL      }
};

/*
 * Configuration utils
 */

static const char *config_info;

static void config_error(elis_State *S, const char *msg, elis_Object *calls) {
  (void) S;
  (void) calls;
  fprintf(stderr, "error: %s (after getting `%s`)\n", msg, config_info);
  exit(EXIT_FAILURE);
}

static elis_Object *get_config(const char *name) {
  config_info = name;
  return elis_eval(S, elis_symbol(S, name));
}

static elis_Number get_number(const char *name, elis_Number num) {
  elis_Object *obj = get_config(name);
  num = elis_nil(S, obj) ? num : elis_to_number(S, obj);
  if (num <= 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s should be > 0", name);
    elis_error(S, buf);
  }
  return num;
}

static const char *get_string(const char *name) {
  elis_Object *obj = get_config(name);
  return elis_nil(S, obj) ? NULL : elis_to_string(S, obj);
}

/*
 * Startup
 */

static void cleanup(void) {
  /* stop audio */
  SDL_CloseAudioDevice(device);
  free(sources);
  /* free graphics stuff */
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  free(screen);
  free(pixels);
  /* free elis state and SDL */
  elis_free(S);
  SDL_Quit();
}

static void audio_callback(void *udata, uint8_t *stream, int len) {
  (void) udata;
  /* make silence */
  memset(stream, 0, len);
  /* mix sources */
  for (int i = 0; i < max_sources; ++i) {
    if (!sources[i].sound || GET_TAG(sources[i].sound) > MUSIC) continue;
    Sound *sound = UNTAG(sources[i].sound, ANY);
    uint32_t remain = sound->length - sources[i].position;
    if ((uint32_t) len < remain) {
      /* just mix portion of `len` samples */
      SDL_MixAudioFormat(stream, sound->buffer + sources[i].position, audio.format, len, sound->volume);
      sources[i].position += len;
    } else {
      /* mix remaining samples */
      SDL_MixAudioFormat(stream, sound->buffer + sources[i].position, audio.format, remain, sound->volume);
      /* if source isn't looping -- remove it */
      if (GET_TAG(sources[i].sound) == SOUND) {
        sources[i].sound = NULL;
        --num_sources;
        continue;
      }
      /* reset position and mix more samples to end of stream */
      sources[i].position = len - remain;
      SDL_MixAudioFormat(stream, sound->buffer, audio.format, sources[i].position, sound->volume);
    }
  }
}

static inline void callback(elis_Object *sym) {
  if (!elis_nil(S, elis_eval(S, sym))) elis_apply(S, sym, elis_bool(S, false));
  elis_restore_gc(S, 0);
}

int main(int argc, char *argv[]) {
  atexit(cleanup);
  srand(time(NULL));

  /*
   * Init Elis, register foreign function and load script
   */

  S = elis_init(NULL, NULL);
  elis_set(S, elis_symbol(S, "PI"), elis_number(S, 3.14159265358979323846));
  elis_set(S, elis_symbol(S, "HUGE"), elis_number(S, HUGE_VAL));
  for (int i = 0; functions[i].name; ++i) {
    elis_set(S, elis_symbol(S, functions[i].name), elis_cfunction(S, functions[i].func));
    elis_restore_gc(S, 0);
  }
  if (argc < 2) elis_error(S, "script name is missing");
  load(S, argv[1]);
  elis_on_error(S, config_error);
  
  /*
   * Get configuration
   */

  width  = clip.x1 = get_number("WIDTH",  128);
  height = clip.y1 = get_number("HEIGHT", 128);
  scale  = get_number("SCALE", 3);
  viewport.w = width * scale;
  viewport.h = height * scale;
  time_step = round(SDL_GetPerformanceFrequency() / get_number("FPS", 30));

  /*
   * Init SDL and create window
   */

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) elis_error(S, SDL_GetError());
  window = SDL_CreateWindow(get_string("TITLE"),
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            viewport.w, viewport.h,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) elis_error(S, SDL_GetError());
  SDL_ShowCursor(SDL_DISABLE);
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) elis_error(S, SDL_GetError());
  texture = SDL_CreateTexture(renderer,
                              SDL_GetWindowPixelFormat(window), SDL_TEXTUREACCESS_STREAMING,
                              width, height);
  if (!texture) elis_error(S, SDL_GetError());
  screen = malloc(width * height);
  pixels = malloc(width * height * sizeof(uint32_t));

  /*
   * Init colors
   */
 
  SDL_Palette palette = (SDL_Palette) { .colors = (void *) &colors };
  for (elis_Object *args = get_config("COLORS"); !elis_nil(S, args); ++num_colors) {
    palette.colors[num_colors].r = elis_to_number(S, elis_next_arg(S, &args));
    palette.colors[num_colors].g = elis_to_number(S, elis_next_arg(S, &args));
    palette.colors[num_colors].b = elis_to_number(S, elis_next_arg(S, &args));
  }
  palette.ncolors = num_colors;
  if (num_colors == 0) elis_error(S, "screen palette isn't defined");

  /*
   * Load images
   */

  SDL_PixelFormat fmt = (SDL_PixelFormat) { .palette = &palette, .BitsPerPixel = 8, .BytesPerPixel = 1 };
  for (elis_Object *args = get_config("IMAGES"); !elis_nil(S, args); elis_restore_gc(S, 0)) {
    elis_Object *sym = elis_next_arg(S, &args);
    /* load source image */
    const char *filename = elis_to_string(S, elis_next_arg(S, &args));
    SDL_Surface *bmp = SDL_LoadBMP(filename);
    if (!bmp) elis_error(S, SDL_GetError());
    if (bmp->h % bmp->w) {
      config_info = filename;
      elis_error(S, "incorrect image dimensions, height should be multiple of width");
    }
    /* convert image to palette format and set colorkey */
    SDL_Surface *surface = SDL_ConvertSurface(bmp, &fmt, 0);
    if (!surface) elis_error(S, SDL_GetError());
    SDL_FreeSurface(bmp);
    surface->userdata = (void *) (uintptr_t) elis_to_number(S, elis_next_arg(S, &args));
    /* create image object */
    elis_set(S, sym, elis_userdata(S, surface, &image_handlers));
  }

  /*
   * Convert palette colors to window pixel format
   */

  for (int i = 0; i < num_colors; ++i) {
    SDL_Color col = palette.colors[i];
    colors[i] = SDL_MapRGB(SDL_GetWindowSurface(window)->format, col.r, col.g, col.b);
  }

  /*
   * Init audio
   */

  audio.freq = 44100;
  audio.format = AUDIO_S16;
  audio.channels = 2;
  audio.samples = 1024;
  audio.callback = audio_callback;
  device = SDL_OpenAudioDevice(NULL, 0, &audio, &audio, SDL_AUDIO_ALLOW_ANY_CHANGE);
  if (!device) elis_error(S, SDL_GetError());
  sources = calloc(1, max_sources * sizeof(*sources));
  SDL_PauseAudioDevice(device, false);

  /*
   * Load sounds
   */

  for (elis_Object *args = get_config("SOUNDS"); !elis_nil(S, args); elis_restore_gc(S, 0)) {
    elis_Object *sym = elis_next_arg(S, &args); 
    /* load audio */
    SDL_AudioSpec spec;
    Sound *sound = malloc(sizeof(*sound));
    if (!SDL_LoadWAV(elis_to_string(S, elis_next_arg(S, &args)), &spec, &sound->buffer, &sound->length)) {
      elis_error(S, SDL_GetError());
    }
    /* audio format matches to system format? */
    SDL_AudioCVT cvt;
    int ret = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, audio.format, audio.channels, audio.freq); 
    if (ret == -1) elis_error(S, "can't convert audio");
    /* convert audio, if needed */
    if (ret == 1) {
      /* allocate memory for conversion */
      cvt.buf = malloc(sound->length * cvt.len_mult);
      cvt.len = sound->length;
      memcpy(cvt.buf, sound->buffer, cvt.len);
      /* convert and replace buffer in `sound` */
      if (SDL_ConvertAudio(&cvt)) elis_error(S, SDL_GetError());
      free(sound->buffer);
      sound->buffer = cvt.buf;
      sound->length = cvt.len_cvt; 
    }
    /* create sound object */
    sound->volume = elis_to_number(S, elis_next_arg(S, &args)) * SDL_MIX_MAXVOLUME;
    elis_set(S, sym, elis_userdata(S, sound, &sound_handlers));
  }
  
  /*
   * Erase config variables
   */

  const char *config[] = { "TITLE", "WIDTH", "HEIGHT", "SCALE", "FPS", "COLORS", "IMAGES", "SOUNDS" };
  for (int i = 0; i < 8; ++i) elis_set(S, elis_symbol(S, config[i]), elis_bool(S, false));

  /*
   * Game loop
   */

  elis_on_error(S, NULL);
  /* call `init` handler */
  callback(elis_symbol(S, "init"));
  /* start main loop */
  uint64_t prev_time = SDL_GetPerformanceCounter();
  elis_Object *step = elis_symbol(S, "step");
  for (;;) {
    /* process events */
    wheel = 0;
    for (SDL_Event e; SDL_PollEvent(&e); ) {
      switch (e.type) {
        case SDL_QUIT:
          return EXIT_SUCCESS;
        case SDL_MOUSEWHEEL:
          wheel = e.wheel.y;
          break;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
            int w = e.window.data1, h = e.window.data2;
            /* find smallest scale */
            int scale_w = w / width, scale_h = h / height;
            scale = scale_w < scale_h ? scale_w : scale_h;
            if (!scale) scale = 1;
            /* recalculate letterbox */
            viewport.w = width * scale;
            viewport.h = height * scale;
            viewport.x = (w - viewport.w) / 2;
            viewport.y = (h - viewport.h) / 2;
            /* clear window screen */
            SDL_RenderClear(renderer);
          }
          break;
      }
    }
    /* call `step` handler */
    callback(step);
    /* draw scaled virtual framebuffer on window (how make it faster?) */
    for (int i = 0; i < width * height; ++i) pixels[i] = colors[screen[i]];
    SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, &viewport);
    SDL_RenderPresent(renderer);
    /* clip framerate */
    uint64_t cur_time = SDL_GetPerformanceCounter(), end_time = prev_time + time_step;
    if (end_time > cur_time) {
      SDL_Delay((end_time - cur_time) * 1000 / SDL_GetPerformanceFrequency());
      prev_time += time_step;
    } else {
      prev_time = cur_time;
    }
  }

  return EXIT_SUCCESS;
}
