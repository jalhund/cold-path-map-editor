// Extension lib defines
#define LIB_NAME "DrawPixels"
#define MODULE_NAME "drawpixels"

#define DLIB_LOG_DOMAIN LIB_NAME
#define M_PI 3.14159265358979323846

// This is necessary if we need to save images to one of the readable image formats. PPM can be converted to PNG
#define EXPORT_TO_PPM false

#include <dmsdk/sdk.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <string>
#include <stack>
#include <stdio.h>

// For profiling
#include <chrono>

class Timer {
  private:
    using clock_t = std::chrono::high_resolution_clock;
  using second_t = std::chrono::duration < double, std::ratio < 1 > > ;

  std::chrono::time_point < clock_t > m_beg;

  public:
    Timer(): m_beg(clock_t::now()) {}

  void reset() {
    m_beg = clock_t::now();
  }

  double elapsed() const {
    return std::chrono::duration_cast < second_t > (clock_t::now() - m_beg).count();
  }
};

struct BufferInfo {
  dmBuffer::HBuffer buffer;
  int width;
  int height;
  int channels;
  bool premultiply_alpha;
  uint8_t * bytes;
  uint32_t src_size;
};

BufferInfo buffer_info;

struct Point {
  int x;
  int y;
};

struct Color {
  int r;
  int g;
  int b;
};

static float PI_2 = M_PI * 2;
static bool is_record_point = false;
static int * points = nullptr;

static void clear_point() {
  if (points != nullptr) {
    delete[] points;
    points = nullptr;
  }
}

static void start_record_points() {
  clear_point();
  is_record_point = true;
  points = new int[buffer_info.width * buffer_info.height];
  for (int i = 0; i < buffer_info.width * buffer_info.height; i++) {
    points[i] = 0;
  }
}

static void stop_record_points() {
  is_record_point = false;
  clear_point();
}

static int in_buffer(int x, int y) {
  return (x >= 0) && (y >= 0) && (x < buffer_info.width) && (y < buffer_info.height);
}

static int xytoi(int x, int y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= buffer_info.width)
    x = buffer_info.width - 1;
  if (y >= buffer_info.height)
    y = buffer_info.height - 1;
  return (y * buffer_info.width * buffer_info.channels) + (x * buffer_info.channels);
}

static void recordtobuffer(int i, float v) {
  buffer_info.bytes[i] = v > 1 ? 255 : v * 255.0;
}

static float getmixrgb(float dc, float da, float sc, float sa, float oa) {
  if (oa == 0) {
    return 0.0;
  }
  return (sc * sa + dc * da * (1 - sa)) / oa;
}

static void putpixel(int x, int y, int r, int g, int b, int a) {
  // ignore the pixel if it's outside the area of the buffer
  if (!in_buffer(x, y)) {
    return;
  }
  int i = xytoi(x, y);
  buffer_info.bytes[i] = r;
  buffer_info.bytes[i + 1] = g;
  buffer_info.bytes[i + 2] = b;
  if (buffer_info.channels == 4) {
    buffer_info.bytes[i + 3] = a;
  }
}

static void record_mix_pixel(int i, float r, float g, float b, float a) {
  float ba = buffer_info.bytes[i + 3] / 255.0;
  a = a / 255.0;
  float oa = a + ba * (1 - a);
  r = getmixrgb(buffer_info.bytes[i] / 255.0, ba, r / 255.0, a, oa);
  g = getmixrgb(buffer_info.bytes[i + 1] / 255.0, ba, g / 255.0, a, oa);
  b = getmixrgb(buffer_info.bytes[i + 2] / 255.0, ba, b / 255.0, a, oa);
  recordtobuffer(i, r);
  recordtobuffer(i + 1, g);
  recordtobuffer(i + 2, b);
  recordtobuffer(i + 3, oa);
}

static void record_premul_alpha_pixel(int i, unsigned short r, unsigned short g, unsigned short b, unsigned short a) {
  buffer_info.bytes[i] = r * a >> 8;
  buffer_info.bytes[i + 1] = g * a >> 8;
  buffer_info.bytes[i + 2] = b * a >> 8;
  buffer_info.bytes[i + 3] = a;
}

static void add_point(int x, int y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= buffer_info.width)
    x = buffer_info.width - 1;
  if (y >= buffer_info.height)
    y = buffer_info.height - 1;
  points[y * buffer_info.width + x] = 1;
}

static void record_pixel(int i, int r, int g, int b, int a) {
  buffer_info.bytes[i] = r;
  buffer_info.bytes[i + 1] = g;
  buffer_info.bytes[i + 2] = b;
  if (buffer_info.channels == 4) {
    buffer_info.bytes[i + 3] = a;
  }
}

static void mixpixel(int x, int y, float r, float g, float b, float a) {
  if (!in_buffer(x, y)) {
    return;
  }
  if (is_record_point && a != 0) {
    add_point(x, y);
  }

  int i = xytoi(x, y);
  if (buffer_info.channels == 3 || a == 255 || (!buffer_info.premultiply_alpha && buffer_info.bytes[i + 3] == 0)) {
    record_pixel(i, r, g, b, a);
    return;
  }
  if (a == 0) {
    return;
  }
  if (buffer_info.premultiply_alpha && buffer_info.bytes[i + 3] == 0) {
    record_premul_alpha_pixel(i, r, g, b, a);
  }

  record_mix_pixel(i, r, g, b, a);
}

static void fill_mixed_line(int fromx, int tox, int y, int r, int g, int b, int a) {
  if (fromx > tox) {
    int temp = fromx;
    fromx = tox;
    tox = temp;
  }
  fromx = fmax(0, fromx);
  tox = fmin(buffer_info.width - 1, tox);
  int start = xytoi(fromx, y);
  int end = xytoi(tox, y);
  for (int i = start; i < end; i += 4) {
    record_mix_pixel(i, r, g, b, a);
  }
}

static Color lerp_color(Point center, Point pixel, float fd_2, Color c1, Color c2) {
  Color nc;
  float coef = (float)((center.x - pixel.x) * (center.x - pixel.x) + (center.y - pixel.y) * (center.y - pixel.y)) / fd_2;
  if (coef <= 0) {
    return c1;
  }
  if (coef >= 1) {
    return c2;
  }
  nc.r = c1.r + (c2.r - c1.r) * coef;
  nc.g = c1.g + (c2.g - c1.g) * coef;
  nc.b = c1.b + (c2.b - c1.b) * coef;
  return nc;
}

static void lerp_pixel(Point center, Point pixel, int fd_2, Color c1, Color c2, int a) {
  Color nc = lerp_color(center, pixel, fd_2, c1, c2);
  mixpixel(pixel.x, pixel.y, nc.r, nc.g, nc.b, a);
}

static void fill_line(int fromx, int tox, int y, int r, int g, int b, int a) {
  if (fromx > tox) {
    int temp = fromx;
    fromx = tox;
    tox = temp;
  }
  fromx = fmax(0, fromx);
  tox = fmin(buffer_info.width - 1, tox);
  int size = 10;
  //prepare line for memcpy
  int line_size = 10 * buffer_info.channels;
  uint8_t * line = new uint8_t[line_size];
  for (int i = 0; i < line_size; i += buffer_info.channels) {
    line[i] = r;
    line[i + 1] = g;
    line[i + 2] = b;
    if (buffer_info.channels == 4) {
      line[i + 3] = a;
    }
  }
  int start = xytoi(fromx, y);
  int end = xytoi(tox, y);
  int width = (end - start + buffer_info.channels);
  for (int i = start; i < end; i += line_size) {
    if (width >= line_size) {
      memcpy( & buffer_info.bytes[i], line, line_size);
    } else {
      memcpy( & buffer_info.bytes[i], line, width);
    }
    width = width - line_size;
  }
  delete[] line;
}

static bool is_contain(int x, int y) {
  return points[y * buffer_info.width + x] == 1;
}

static bool is_new(int i, int r, int g, int b) {
  return buffer_info.bytes[i] == r && buffer_info.bytes[i + 1] == g && buffer_info.bytes[i + 2] == b;
}

static void find_seed_pixel(std::stack < Point > & top, Point pixel, int x_right, int r, int g, int b, int a) {
  while (pixel.x <= x_right) {
    int flag = 0;
    while (pixel.x < x_right && !is_contain(pixel.x, pixel.y)) {
      if (flag == 0) {
        flag = 1;
      }
      pixel.x += 1;
    }
    if (flag == 1) {
      if (pixel.x == x_right && !is_contain(pixel.x, pixel.y)) {
        Point new_pixel;
        new_pixel.x = pixel.x;
        new_pixel.y = pixel.y;
        if (in_buffer(new_pixel.x, new_pixel.y)) {
          top.push(new_pixel);
        }
      } else {
        Point new_pixel;
        new_pixel.x = pixel.x - 1;
        new_pixel.y = pixel.y;
        if (in_buffer(new_pixel.x, new_pixel.y)) {
          top.push(new_pixel);
        }
      }
      flag = 0;
    }

    int x_in = pixel.x;
    while (pixel.x < x_right && !is_contain(pixel.x, pixel.y)) {
      mixpixel(pixel.x, pixel.y, r, g, b, a);
      pixel.x += 1;
    }
    if (pixel.x == x_in) {
      pixel.x += 1;
    }
  }
}

// Seed-by-line filling algorithm
static void fill_area(int x, int y, int r, int g, int b, int a) {
  if (!in_buffer(x, y)) {
    return;
  }
  Point _pixel;
  _pixel.x = x;
  _pixel.y = y;
  std::stack < Point > top;
  top.push(_pixel);

  while (!top.empty()) {
    Point pixel = top.top();
    top.pop();
    if (!is_contain(pixel.x, pixel.y))
      mixpixel(pixel.x, pixel.y, r, g, b, a);
    int temp_x = pixel.x;
    pixel.x += 1;
    while (pixel.x < buffer_info.width - 1 && !is_contain(pixel.x, pixel.y)) {
      mixpixel(pixel.x, pixel.y, r, g, b, a);
      pixel.x += 1;
    }
    if (!is_contain(pixel.x, pixel.y))
      mixpixel(pixel.x, pixel.y, r, g, b, a);
    int x_right = pixel.x >= buffer_info.width ? buffer_info.width - 1 : pixel.x;
    pixel.x = temp_x;
    pixel.x -= 1;
    while (pixel.x >= 1 && !is_contain(pixel.x, pixel.y)) {
      mixpixel(pixel.x, pixel.y, r, g, b, a);
      pixel.x -= 1;
    }
    if (!is_contain(pixel.x, pixel.y))
      mixpixel(pixel.x, pixel.y, r, g, b, a);
    pixel.x++;
    int x_left = pixel.x;
    pixel.y += 1;
    if (pixel.y >= buffer_info.height) {
      continue;
    }
    find_seed_pixel(top, pixel, x_right, r, g, b, a);
    pixel.y -= 2;
    if (pixel.y < 0) {
      continue;
    }
    pixel.x = x_left;
    find_seed_pixel(top, pixel, x_right, r, g, b, a);
  }
}

static void gradient_find_seed_pixel(std::stack < Point > & top, Point pixel, int x_right, Point center, int fd_2, Color c1, Color c2, int a) {
  while (pixel.x <= x_right) {
    int flag = 0;
    while (pixel.x < x_right && !is_contain(pixel.x, pixel.y)) {
      if (flag == 0) {
        flag = 1;
      }
      pixel.x += 1;
    }
    if (flag == 1) {
      if (pixel.x == x_right && !is_contain(pixel.x, pixel.y)) {
        Point new_pixel;
        new_pixel.x = pixel.x;
        new_pixel.y = pixel.y;
        if (in_buffer(new_pixel.x, new_pixel.y)) {
          top.push(new_pixel);
        }
      } else {
        Point new_pixel;
        new_pixel.x = pixel.x - 1;
        new_pixel.y = pixel.y;
        if (in_buffer(new_pixel.x, new_pixel.y)) {
          top.push(new_pixel);
        }
      }
      flag = 0;
    }

    int x_in = pixel.x;
    while (pixel.x < x_right && is_contain(pixel.x, pixel.y)) {
      lerp_pixel(center, pixel, fd_2, c1, c2, a);
      pixel.x += 1;
    }
    if (pixel.x == x_in) {
      pixel.x += 1;
    }
  }
}

static void gradient_fill_area(int x, int y, Point center, int fd_2, Color c1, Color c2, int a) {
  if (!in_buffer(x, y)) {
    return;
  }
  Point _pixel;
  _pixel.x = x;
  _pixel.y = y;
  std::stack < Point > top;
  top.push(_pixel);

  while (!top.empty()) {
    Point pixel = top.top();
    top.pop();
    lerp_pixel(center, pixel, fd_2, c1, c2, a);
    int temp_x = pixel.x;
    pixel.x += 1;
    while (pixel.x < buffer_info.width - 1 && !is_contain(pixel.x, pixel.y)) {
      lerp_pixel(center, pixel, fd_2, c1, c2, a);
      pixel.x += 1;
    }
    lerp_pixel(center, pixel, fd_2, c1, c2, a);
    int x_right = pixel.x >= buffer_info.width ? buffer_info.width - 1 : pixel.x;
    pixel.x = temp_x;
    pixel.x -= 1;
    while (pixel.x >= 1 && !is_contain(pixel.x, pixel.y)) {
      lerp_pixel(center, pixel, fd_2, c1, c2, a);
      pixel.x -= 1;
    }
    lerp_pixel(center, pixel, fd_2, c1, c2, a);
    pixel.x++;
    int x_left = pixel.x;
    pixel.y += 1;
    if (pixel.y >= buffer_info.height) {
      continue;
    }
    gradient_find_seed_pixel(top, pixel, x_right, center, fd_2, c1, c2, a);
    pixel.y -= 2;
    if (pixel.y < 0) {
      continue;
    }
    pixel.x = x_left;
    gradient_find_seed_pixel(top, pixel, x_right, center, fd_2, c1, c2, a);
  }
}

//http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
static void fill_bottom_flat_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) {
  float invslope1 = float(x2 - x1) / float(y2 - y1);
  float invslope2 = float(x3 - x1) / float(y3 - y1);
  float curx1 = x1;
  float curx2 = x1;
  for (int scanlineY = y1; scanlineY <= y2; scanlineY++) {
    fill_line(curx1, curx2, scanlineY, r, g, b, a);
    curx1 += invslope1;
    curx2 += invslope2;
  }
}

static void fill_top_flat_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) {
  float invslope1 = float(x3 - x1) / float(y3 - y1);
  float invslope2 = float(x3 - x2) / float(y3 - y2);
  float curx1 = x3;
  float curx2 = x3;
  for (int scanlineY = y3; scanlineY > y1; scanlineY--) {
    fill_line(curx1, curx2, scanlineY, r, g, b, a);
    curx1 -= invslope1;
    curx2 -= invslope2;
  }
}

static void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) {
  /* values should be sorted by y */
  /* here we know that y1 <= y2 <= y3 */
  /* check for trivial case of bottom-flat triangle */
  if (y2 == y3) {
    fill_bottom_flat_triangle(x1, y1, x2, y2, x3, y3, r, g, b, a);
  }
  /* check for trivial case of top-flat triangle */
  else if (y1 == y2) {
    fill_top_flat_triangle(x1, y1, x2, y2, x3, y3, r, g, b, a);
  } else {
    /* general case - split the triangle in a topflat and bottom-flat one */
    int x4 = x1 + ((float(y2 - y1) / float(y3 - y1)) * (x3 - x1));
    int y4 = y2;
    fill_bottom_flat_triangle(x1, y1, x2, y2, x4, y4, r, g, b, a);
    fill_top_flat_triangle(x2, y2, x4, y4, x3, y3, r, g, b, a);
  }
}

static void read_and_validate_buffer_info(lua_State * L, int index) {
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "buffer");
  lua_getfield(L, index, "width");
  lua_getfield(L, index, "height");
  lua_getfield(L, index, "channels");
  lua_getfield(L, index, "premultiply_alpha");
  dmScript::LuaHBuffer * lua_buffer = dmScript::CheckBuffer(L, -5);
  buffer_info.buffer = lua_buffer -> m_Buffer;

  if (!dmBuffer::IsBufferValid(buffer_info.buffer)) {
    luaL_error(L, "Buffer is invalid");
  }

  dmBuffer::Result result = dmBuffer::GetBytes(buffer_info.buffer, (void ** ) & buffer_info.bytes, & buffer_info.src_size);
  if (result != dmBuffer::RESULT_OK) {
    luaL_error(L, "Buffer is invalid");
  }

  buffer_info.width = lua_tointeger(L, -4);
  if (buffer_info.width == 0) {
    luaL_error(L, "'width' of the buffer should be an integer and > 0");
  }

  buffer_info.height = lua_tointeger(L, -3);
  if (buffer_info.height == 0) {
    luaL_error(L, "'height' of the buffer should be an integer and > 0");
  }

  buffer_info.channels = lua_tointeger(L, -2);
  if (buffer_info.channels == 0) {
    luaL_error(L, "'channels' of should be an integer and 3 or 4");
  }

  if (lua_isboolean(L, -1) == 1) {
    buffer_info.premultiply_alpha = lua_toboolean(L, -1);
  } else {
    buffer_info.premultiply_alpha = false;
  }
}

static void draw_line(int x0, int y0, int x1, int y1, int r, int g, int b, int a) {
  // https://gist.github.com/bert/1085538#file-plot_line-c
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2; /* error value e_xy */

  for (;;) {
    /* loop */
    mixpixel(x0, y0, r, g, b, a);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    } /* e_xy+e_x > 0 */
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    } /* e_xy+e_y < 0 */
  }
}

static void draw_gradient_line(int x0, int y0, int x1, int y1, Color c1, Color c2, int a) {
  // https://gist.github.com/bert/1085538#file-plot_line-c
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2; /* error value e_xy */
  int fd_2 = (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1);

  Point center;
  center.x = x0;
  center.y = y0;
  Point pixel;

  for (;;) {
    /* loop */
    pixel.x = x0;
    pixel.y = y0;
    lerp_pixel(center, pixel, fd_2, c1, c2, a);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    } /* e_xy+e_x > 0 */
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    } /* e_xy+e_y < 0 */
  }
}

static int IPart(float x) {
  return (int) x;
}

static float FPart(float x) {
  while (x >= 0)
    x--;
  x++;
  return x;
}

// http://grafika.me/node/38
static void draw_line_vu(int x0, int y0, int x1, int y1, int r, int g, int b, int a, float w) {
  int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int w_min = ceilf(w / 2) - 1;
  int w_max = w / 2 + 1;
  if (dx == 0) {
    for (int i = -w_min; i < w_max; i++) {
      draw_line(x0 + i, y0, x1 + i, y1, r, g, b, a);
    }
    return;
  }
  if (dy == 0) {
    for (int i = -w_min; i < w_max; i++) {
      draw_line(x0, y0 + i, x1, y1 + i, r, g, b, a);
    }
    return;
  }
  float na = (float) a / 255;

  if (dy < dx) {
    if (x1 < x0) {
      x1 += x0;
      x0 = x1 - x0;
      x1 -= x0;
      y1 += y0;
      y0 = y1 - y0;
      y1 -= y0;
    }
    float grad = (float) dy / dx;
    if (y1 < y0)
      grad = -grad;
    float intery = y0 + grad;
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x0, y0 + i, r, g, b, a);
    }
    float intens = 0;
    int ipart = 0;
    for (int x = x0 + 1; x < x1; x++) {
      intens = FPart(intery) * 255;
      ipart = IPart(intery);
      mixpixel(x, ipart - w_min, r, g, b, (255 - intens) * na);
      for (int i = -w_min + 1; i < w_max; i++) {
        mixpixel(x, ipart + i, r, g, b, a);
      }
      mixpixel(x, ipart + w_max, r, g, b, intens * na);
      intery += grad;
    }
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x1, y1 + i, r, g, b, a);
    }
  } else {
    if (y1 < y0) {
      x1 += x0;
      x0 = x1 - x0;
      x1 -= x0;
      y1 += y0;
      y0 = y1 - y0;
      y1 -= y0;
    }
    float grad = (float) dx / dy;
    if (x1 < x0)
      grad = -grad;
    float interx = x0 + grad;
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x0 + i, y0, r, g, b, a);
    }
    float intens = 0;
    int ipart = 0;
    for (int y = y0 + 1; y < y1; y++) {
      intens = FPart(interx) * 255;
      ipart = IPart(interx);
      mixpixel(ipart - w_min, y, r, g, b, (255 - intens) * na);
      for (int i = -w_min + 1; i < w_max; i++) {
        mixpixel(ipart + i, y, r, g, b, a);
      }
      mixpixel(ipart + w_max, y, r, g, b, intens * na);
      interx += grad;
    }
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x1 + i, y1, r, g, b, a);
    }
  }
}

static void draw_gradient_line_vu(int x0, int y0, int x1, int y1, Color c1, Color c2, int a, float w) {
  Point center;
  center.x = x0;
  center.y = y0;
  int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int fd_2 = (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1);
  int w_min = ceilf(w / 2) - 1;
  int w_max = w / 2 + 1;
  if (dx == 0) {
    for (int i = -w_min; i < w_max; i++) {
      draw_gradient_line(x0 + i, y0, x1 + i, y1, c1, c2, a);
    }
    return;
  }
  if (dy == 0) {
    for (int i = -w_min; i < w_max; i++) {
      draw_gradient_line(x0, y0 + i, x1, y1 + i, c1, c2, a);
    }
    return;
  }
  float na = (float) a / 255;
  if (dy < dx) {
    if (x1 < x0) {
      x1 += x0;
      x0 = x1 - x0;
      x1 -= x0;
      y1 += y0;
      y0 = y1 - y0;
      y1 -= y0;
    }
    float grad = (float) dy / dx;
    if (y1 < y0)
      grad = -grad;
    float intery = y0 + grad;
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x0, y0 + i, c1.r, c1.g, c1.b, a);
    }
    Point pixel;
    float intens = 0;
    int ipart = 0;
    for (int x = x0 + 1; x < x1; x++) {
      intens = FPart(intery) * 255;
      ipart = IPart(intery);
      pixel.x = x;
      pixel.y = ipart - w_min;

      lerp_pixel(center, pixel, fd_2, c1, c2, (255 - intens) * na);
      for (int i = -w_min + 1; i < w_max; i++) {
        pixel.y = ipart + i;
        lerp_pixel(center, pixel, fd_2, c1, c2, a);
      }
      pixel.y = ipart + w_max;
      lerp_pixel(center, pixel, fd_2, c1, c2, intens * na);
      intery += grad;
    }
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x1, y1 + i, c2.r, c2.g, c2.b, a);
    }
  } else {
    if (y1 < y0) {
      x1 += x0;
      x0 = x1 - x0;
      x1 -= x0;
      y1 += y0;
      y0 = y1 - y0;
      y1 -= y0;
    }
    float grad = (float) dx / dy;
    if (x1 < x0)
      grad = -grad;
    float interx = x0 + grad;
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x0 + i, y0, c1.r, c1.g, c1.b, a);
    }
    Point pixel;
    float intens = 0;
    int ipart = 0;
    for (int y = y0 + 1; y < y1; y++) {
      intens = FPart(interx) * 255;
      ipart = IPart(interx);
      pixel.x = ipart - w_min;
      pixel.y = y;

      lerp_pixel(center, pixel, fd_2, c1, c2, (255 - intens) * na);
      for (int i = -w_min + 1; i < w_max; i++) {
        pixel.x = ipart + i;
        lerp_pixel(center, pixel, fd_2, c1, c2, a);
      }
      pixel.x = ipart + w_max;
      lerp_pixel(center, pixel, fd_2, c1, c2, intens * na);
      interx += grad;
    }
    for (int i = -w_min; i < w_max; i++) {
      mixpixel(x1 + i, y1, c2.r, c2.g, c2.b, a);
    }
  }
}

// modified http://www.opita.net/node/699
static void draw_circle_vu(int _x, int _y, int radius, int r, int g, int b, int a, float w) {
  if (radius < 1) {
    return;
  }
  int w_min = ceilf(w / 2) - 1;
  int w_max = w / 2 + 1;
  float iy = 0;
  int intens = 0;
  int inv_intens = 0;
  int iiy = 0;
  int cx = 0;
  int cy = 0;
  for (int x = 0; x <= (radius + w) * cos(M_PI / 4); x++) {
    iy = (float) sqrt(radius * radius - x * x);
    intens = (int)((FPart(iy) * 255.0) * a / 255);
    inv_intens = (255 - (int)(FPart(iy) * 255)) * a / 255;
    iiy = IPart(iy);
    cx = _x - x;
    cy = _y + iiy;
    mixpixel(cx, cy - w_min, r, g, b, inv_intens);
    mixpixel(cx, cy + w_max, r, g, b, intens);
    cy = _y - iiy - 1;
    mixpixel(cx, cy - w_min, r, g, b, intens);
    mixpixel(cx, cy + w_max, r, g, b, inv_intens);
    cx = _x + x;
    cy = _y + iiy;
    mixpixel(cx, cy - w_min, r, g, b, inv_intens);
    mixpixel(cx, cy + w_max, r, g, b, intens);
    cy = _y - iiy - 1;
    mixpixel(cx, cy - w_min, r, g, b, intens);
    mixpixel(cx, cy + w_max, r, g, b, inv_intens);

    cx = _x + iiy;
    cy = _y + x;
    mixpixel(cx - w_min, cy, r, g, b, inv_intens);
    mixpixel(cx + w_max, cy, r, g, b, intens);
    cy = _y - x;
    mixpixel(cx - w_min, cy, r, g, b, inv_intens);
    mixpixel(cx + w_max, cy, r, g, b, intens);
    cx = _x - iiy - 1;
    cy = _y + x;
    mixpixel(cx - w_min, cy, r, g, b, intens);
    mixpixel(cx + w_max, cy, r, g, b, inv_intens);
    cy = _y - x;
    mixpixel(cx - w_min, cy, r, g, b, intens);
    mixpixel(cx + w_max, cy, r, g, b, inv_intens);
    for (int i = -w_min + 1; i < w_max; i++) {
      cx = _x - x;
      cy = _y + iiy;
      mixpixel(cx, cy + i, r, g, b, a);
      cy = _y - iiy - 1;
      mixpixel(cx, cy + i, r, g, b, a);
      cx = _x + x;
      cy = _y + iiy;
      mixpixel(cx, cy + i, r, g, b, a);
      cy = _y - iiy - 1;
      mixpixel(cx, cy + i, r, g, b, a);

      cx = _x + iiy;
      cy = _y + x;
      mixpixel(cx + i, cy, r, g, b, a);
      cy = _y - x;
      mixpixel(cx + i, cy, r, g, b, a);
      cx = _x - iiy - 1;
      cy = _y + x;
      mixpixel(cx + i, cy, r, g, b, a);
      cy = _y - x;
      mixpixel(cx + i, cy, r, g, b, a);
    }
  }
}

static bool sectorcont(float x, float y, int radius, float from, float to) {
  float atan = atan2(y, x) - M_PI / 2;
  if (atan < 0) {
    atan = M_PI * 2 + atan;
  }
  if (from <= to) {
    return from <= atan && atan <= to;
  } else {
    return to >= atan || atan >= from;
  }
}

static void draw_gradient_arc_lines(int _x, int _y, int radius, float from, float to, Color c1, Color c2, int a) {
  float fx = radius * cos(from + M_PI / 2);
  float fy = radius * sin(from + M_PI / 2);
  if (from == to) {
    draw_gradient_line_vu(_x, _y, fx + _x, fy + _y, c1, c2, a, 1);
    return;
  }

  float tx = radius * cos(to + M_PI / 2);
  float ty = radius * sin(to + M_PI / 2);
  draw_gradient_line_vu(_x, _y, fx + _x, fy + _y, c1, c2, a, 1);
  draw_gradient_line_vu(_x, _y, tx + _x, ty + _y, c1, c2, a, 1);
}

static void draw_arc_lines(int _x, int _y, int radius, float from, float to, int r, int g, int b, int a) {
  float fx = radius * cos(from + M_PI / 2);
  float fy = radius * sin(from + M_PI / 2);
  if (from == to) {
    draw_line_vu(_x, _y, fx + _x, fy + _y, r, g, b, a, 1);
    return;
  }

  float tx = radius * cos(to + M_PI / 2);
  float ty = radius * sin(to + M_PI / 2);
  draw_line_vu(_x, _y, fx + _x, fy + _y, r, g, b, a, 1);
  draw_line_vu(_x, _y, tx + _x, ty + _y, r, g, b, a, 1);
}

static void draw_arc_vu(int _x, int _y, int radius, float from, float to, int r, int g, int b, int a) {
  if (from == to) {
    return;
  }
  float iy = 0;
  float shift = M_PI * 0.0007;
  from = from - shift;
  to = to + shift;
  if (sectorcont(radius, 0, radius, from, to)) {
    mixpixel(_x + radius, _y, r, g, b, a);
  }
  if (sectorcont(0, radius, radius, from, to)) {
    mixpixel(_x, _y + radius, r, g, b, a);
  }
  if (sectorcont(-radius + 1, 0, radius, from, to)) {
    mixpixel(_x - radius + 1, _y, r, g, b, a);
  }
  if (sectorcont(0, -radius + 1, radius, from, to)) {
    mixpixel(_x, _y - radius + 1, r, g, b, a);
  }

  for (int x = 0; x <= radius * cos(M_PI / 4) + 1; x++) {
    iy = (float) sqrt(radius * radius - x * x);
    int intens = (int)((FPart(iy) * 255.0) * a / 255);
    int inv_intens = (255 - (int)(FPart(iy) * 255)) * a / 255;
    int iiy = IPart(iy);
    int cx = _x - x;
    int cy = _y + iiy;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, inv_intens);
      mixpixel(cx, cy + 1, r, g, b, intens);
    }
    cx = _x + x;
    cy = _y + iiy;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, inv_intens);
      mixpixel(cx, cy + 1, r, g, b, intens);
    }
    cx = _x + iiy;
    cy = _y + x;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, inv_intens);
      mixpixel(cx + 1, cy, r, g, b, intens);
    }
    cx = _x + iiy;
    cy = _y - x;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, inv_intens);
      mixpixel(cx + 1, cy, r, g, b, intens);
    }
    x++;
    cx = _x + x;
    cy = _y - iiy;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, intens);
      mixpixel(cx, cy + 1, r, g, b, inv_intens);
    }
    cx = _x - x;
    cy = _y - iiy;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, intens);
      mixpixel(cx, cy + 1, r, g, b, inv_intens);
    }
    cx = _x - iiy;
    cy = _y - x;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, intens);
      mixpixel(cx + 1, cy, r, g, b, inv_intens);
    }
    cx = _x - iiy;
    cy = _y + x;
    if (sectorcont(cx - _x, cy - _y, radius, from, to)) {
      mixpixel(cx, cy, r, g, b, intens);
      mixpixel(cx + 1, cy, r, g, b, inv_intens);
    }
    x--;
  }
}

static void draw_normalized_arc(int32_t posx, int32_t posy, int32_t radius, float & from, float & to, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  if (abs(from - to) >= PI_2) {
    float fx = radius * cos(from);
    float fy = radius * sin(from);
    draw_circle_vu(posx, posy, radius, r, g, b, a, 1);
  } else {
    from -= M_PI / 2;
    to -= M_PI / 2;
    from = from >= PI_2 ? from - (((int)(from / PI_2)) * PI_2) : from;
    to = to >= PI_2 ? to - (((int)(to / PI_2)) * PI_2) : to;
    if (to < from && to < 0 && from < 0) {
      float temp = to;
      to = from;
      from = temp;
    }
    from = from < 0 ? (from - PI_2 * (int)(from / PI_2)) + PI_2 : from;
    to = to < 0 ? (to - PI_2 * (int)(to / PI_2)) + PI_2 : to;
    draw_arc_vu(posx, posy, radius, from, to, r, g, b, a);
  }
}

//https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
static void draw_circle(int32_t posx, int32_t posy, int32_t diameter, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  if (diameter > 0) {
    int radius = diameter / 2;
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (x >= y) {
      mixpixel(posx + x, posy + y, r, g, b, a);
      mixpixel(posx + y, posy + x, r, g, b, a);
      mixpixel(posx - y, posy + x, r, g, b, a);
      mixpixel(posx - x, posy + y, r, g, b, a);
      mixpixel(posx - x, posy - y, r, g, b, a);
      mixpixel(posx - y, posy - x, r, g, b, a);
      mixpixel(posx + y, posy - x, r, g, b, a);
      mixpixel(posx + x, posy - y, r, g, b, a);

      if (err <= 0) {
        y++;
        err += dy;
        dy += 2;
      }

      if (err > 0) {
        x--;
        dx += 2;
        err += dx - (radius << 1);
      }
    }
  }
}

static void draw_filled_circle(int32_t posx, int32_t posy, int32_t diameter, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  if (diameter > 0) {
    int radius = diameter / 2;
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);
    while (x >= y) {
      fill_line(posx - x, posx + x, posy + y, r, g, b, a);
      fill_line(posx - y, posx + y, posy + x, r, g, b, a);
      fill_line(posx - x, posx + x, posy - y, r, g, b, a);
      fill_line(posx - y, posx + y, posy - x, r, g, b, a);

      if (err <= 0) {
        y++;
        err += dy;
        dy += 2;
      }

      if (err > 0) {
        x--;
        dx += 2;
        err += dx - (radius << 1);
      }
    }
  }
}

static void draw_filled_circle_vu(int _x, int _y, int radius, int r, int g, int b, int a) {
  if (radius < 1) {
    return;
  }

  float iy = 0;
  int intens = 0;
  int inv_intens = 0;
  int iiy = 0;
  int cx = 0;
  int cy = 0;
  int last_iiy1 = 0;
  int last_iiy2 = 0;
  int last_iiy3 = 0;
  int last_iiy4 = 0;
  int first_x = (radius + 1) * cos(M_PI / 4);
  int first_y = sqrt(radius * radius - first_x * first_x);
  for (int x = first_x; x >= 0; x--) {
    iy = (float) sqrt(radius * radius - x * x);
    intens = (int)((FPart(iy) * 255.0) * a / 255);
    inv_intens = (255 - (int)(FPart(iy) * 255)) * a / 255;
    iiy = IPart(iy);
    cx = _x - x;
    cy = _y + iiy;
    mixpixel(cx, cy, r, g, b, inv_intens);
    mixpixel(cx, cy + 1, r, g, b, intens);
    cy = _y - iiy - 1;
    mixpixel(cx, cy, r, g, b, intens);
    mixpixel(cx, cy + 1, r, g, b, inv_intens);
    cx = _x + x;
    cy = _y + iiy;
    mixpixel(cx, cy, r, g, b, inv_intens);
    mixpixel(cx, cy + 1, r, g, b, intens);
    cy = _y - iiy - 1;
    mixpixel(cx, cy, r, g, b, intens);
    mixpixel(cx, cy + 1, r, g, b, inv_intens);
    cx = _x + iiy;
    cy = _y + x;
    mixpixel(cx, cy, r, g, b, inv_intens);
    mixpixel(cx + 1, cy, r, g, b, intens);
    cy = _y - x;
    mixpixel(cx, cy, r, g, b, inv_intens);
    mixpixel(cx + 1, cy, r, g, b, intens);
    cx = _x - iiy - 1;
    cy = _y + x;
    mixpixel(cx, cy, r, g, b, intens);
    mixpixel(cx + 1, cy, r, g, b, inv_intens);
    cy = _y - x;
    mixpixel(cx, cy, r, g, b, intens);
    mixpixel(cx + 1, cy, r, g, b, inv_intens);
    // Fill circle
    if (_y + iiy != last_iiy1) {
      fill_mixed_line(_x - x, _x + x + 1, _y + iiy, r, g, b, a);
      fill_mixed_line(_x - x, _x + x + 1, _y - iiy, r, g, b, a);
      last_iiy1 = _y + iiy;
    }
    if (_y - x > _y - first_y && _y - x != last_iiy3) {
      last_iiy3 = _y - x;
      fill_mixed_line(_x - iiy, _x + iiy + 1, _y - x, r, g, b, a);
    }
    if (x != 0 && _y + x < _y + first_y && _y + x != last_iiy4) {
      last_iiy4 = _y + x;
      fill_mixed_line(_x - iiy, _x + iiy + 1, _y + x, r, g, b, a);
    }
  }
}

static int start_record_points_lua(lua_State * L) {
  int top = lua_gettop(L);

  start_record_points();

  assert(top == lua_gettop(L));
  return 0;
}

static int stop_record_points_lua(lua_State * L) {
  int top = lua_gettop(L);

  stop_record_points();

  assert(top == lua_gettop(L));
  return 0;
}

static int fill_area_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;
  read_and_validate_buffer_info(L, 1);
  int32_t x = luaL_checknumber(L, 2);
  int32_t y = luaL_checknumber(L, 3);
  int32_t r = luaL_checknumber(L, 4);
  int32_t g = luaL_checknumber(L, 5);
  int32_t b = luaL_checknumber(L, 6);
  int32_t a = 0;
  if (lua_isnumber(L, 7) == 1) {
    a = luaL_checknumber(L, 7);
  }
  if (a == 0 && buffer_info.channels == 4) {
    assert(top == lua_gettop(L));
    return 0;
  }

  fill_area(x, y, r, g, b, a);

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_triangle_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x0 = luaL_checknumber(L, 2);
  int32_t y0 = luaL_checknumber(L, 3);
  int32_t x1 = luaL_checknumber(L, 4);
  int32_t y1 = luaL_checknumber(L, 5);
  int32_t x2 = luaL_checknumber(L, 6);
  int32_t y2 = luaL_checknumber(L, 7);
  uint32_t r = luaL_checknumber(L, 8);
  uint32_t g = luaL_checknumber(L, 9);
  uint32_t b = luaL_checknumber(L, 10);
  uint32_t a = 0;
  if (lua_isnumber(L, 11) == 1) {
    a = luaL_checknumber(L, 11);
  }

  draw_triangle(x0, y0, x1, y1, x2, y2, r, g, b, a);
  draw_line_vu(x0, y0, x1, y1, r, g, b, a, 1);
  draw_line_vu(x1, y1, x2, y2, r, g, b, a, 1);
  draw_line_vu(x2 - 1, y2, x0 - 1, y0, r, g, b, a, 1);

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_line_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x0 = luaL_checknumber(L, 2);
  int32_t y0 = luaL_checknumber(L, 3);
  int32_t x1 = luaL_checknumber(L, 4);
  int32_t y1 = luaL_checknumber(L, 5);
  uint32_t r = luaL_checknumber(L, 6);
  uint32_t g = luaL_checknumber(L, 7);
  uint32_t b = luaL_checknumber(L, 8);
  uint32_t a = 0;
  int w = 1;
  if (lua_isnumber(L, 9) == 1) {
    a = luaL_checknumber(L, 9);
  }
  bool antialiasing = false;
  if (lua_isboolean(L, 10) == 1) {
    antialiasing = lua_toboolean(L, 10);
    if (lua_isnumber(L, 11) == 1) {
      w = luaL_checknumber(L, 11);
      if (w < 1) {
        w = 1;
      }
    }
  }

  if (antialiasing && buffer_info.channels == 4) {
    draw_line_vu(x0, y0, x1, y1, r, g, b, a, w);
  } else {
    draw_line(x0, y0, x1, y1, r, g, b, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_gradient_line_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x0 = luaL_checknumber(L, 2);
  int32_t y0 = luaL_checknumber(L, 3);
  int32_t x1 = luaL_checknumber(L, 4);
  int32_t y1 = luaL_checknumber(L, 5);
  uint32_t r1 = luaL_checknumber(L, 6);
  uint32_t g1 = luaL_checknumber(L, 7);
  uint32_t b1 = luaL_checknumber(L, 8);
  uint32_t r2 = luaL_checknumber(L, 9);
  uint32_t g2 = luaL_checknumber(L, 10);
  uint32_t b2 = luaL_checknumber(L, 11);
  uint32_t a = 0;
  int w = 1;
  if (lua_isnumber(L, 12) == 1) {
    a = luaL_checknumber(L, 12);
  }
  bool antialiasing = false;
  if (lua_isboolean(L, 13) == 1) {
    antialiasing = lua_toboolean(L, 13);
    if (lua_isnumber(L, 14) == 1) {
      w = luaL_checknumber(L, 14);
      if (w < 1) {
        w = 1;
      }
    }
  }

  Color c1;
  c1.r = r1;
  c1.g = g1;
  c1.b = b1;
  Color c2;
  c2.r = r2;
  c2.g = g2;
  c2.b = b2;

  if (antialiasing && buffer_info.channels == 4) {
    draw_gradient_line_vu(x0, y0, x1, y1, c1, c2, a, w);
  } else {
    draw_gradient_line(x0, y0, x1, y1, c1, c2, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_arc_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  int32_t radius = luaL_checknumber(L, 4);
  float from = luaL_checknumber(L, 5);
  float to = luaL_checknumber(L, 6);
  uint32_t r = luaL_checknumber(L, 7);
  uint32_t g = luaL_checknumber(L, 8);
  uint32_t b = luaL_checknumber(L, 9);
  uint32_t a = 0;
  if (lua_isnumber(L, 10) == 1) {
    a = luaL_checknumber(L, 10);
  }

  draw_normalized_arc(posx, posy, radius, from, to, r, g, b, a);
  draw_arc_lines(posx, posy, radius, from, to, r, g, b, a);

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_filled_arc_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  int32_t radius = luaL_checknumber(L, 4);
  float from = luaL_checknumber(L, 5);
  float to = luaL_checknumber(L, 6);
  uint32_t r = luaL_checknumber(L, 7);
  uint32_t g = luaL_checknumber(L, 8);
  uint32_t b = luaL_checknumber(L, 9);
  uint32_t a = 0;
  if (lua_isnumber(L, 10) == 1) {
    a = luaL_checknumber(L, 10);
  }
  start_record_points();
  draw_normalized_arc(posx, posy, radius, from, to, r, g, b, a);
  draw_arc_lines(posx, posy, radius, from, to, r, g, b, a);

  float center = (from + to) / 2;
  center = from > to ? center - M_PI / 2 : center + M_PI / 2;

  fill_area(posx + radius * 2 / 3 * cos(center), posy + radius * 2 / 3 * sin(center), r, g, b, a);
  stop_record_points();
  draw_line_vu(posx, posy, posx + radius * 2 / 3 * cos(center), posy + radius * 2 / 3 * sin(center), r, g, b, a, 2);

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_gradient_arc_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  int32_t radius = luaL_checknumber(L, 4);
  float from = luaL_checknumber(L, 5);
  float to = luaL_checknumber(L, 6);
  uint32_t r1 = luaL_checknumber(L, 7);
  uint32_t g1 = luaL_checknumber(L, 8);
  uint32_t b1 = luaL_checknumber(L, 9);
  uint32_t r2 = luaL_checknumber(L, 10);
  uint32_t g2 = luaL_checknumber(L, 11);
  uint32_t b2 = luaL_checknumber(L, 12);
  uint32_t a = 0;
  if (lua_isnumber(L, 13) == 1) {
    a = luaL_checknumber(L, 13);
  }

  Color c1;
  c1.r = r1;
  c1.g = g1;
  c1.b = b1;
  Color c2;
  c2.r = r2;
  c2.g = g2;
  c2.b = b2;
  start_record_points();
  draw_normalized_arc(posx, posy, radius, from, to, r2, g2, b2, a);
  draw_gradient_arc_lines(posx, posy, radius, from, to, c1, c2, a);

  float center = (from + to) / 2;
  center = from > to ? center - M_PI / 2 : center + M_PI / 2;

  Point center_point;
  center_point.x = posx;
  center_point.y = posy;

  gradient_fill_area(posx + radius * 2 / 3 * cos(center), posy + radius * 2 / 3 * sin(center), center_point, radius * radius, c1, c2, a);
  stop_record_points();
  draw_gradient_line_vu(posx, posy, posx + radius * 2 / 2.05 * cos(center), posy + radius * 2 / 2.05 * sin(center), c1, c2, a, 1);

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_circle_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  int32_t diameter = luaL_checknumber(L, 4);
  uint32_t r = luaL_checknumber(L, 5);
  uint32_t g = luaL_checknumber(L, 6);
  uint32_t b = luaL_checknumber(L, 7);
  uint32_t a = 0;
  int w = 1;
  if (lua_isnumber(L, 8) == 1) {
    a = luaL_checknumber(L, 8);
  }
  bool antialiasing = false;
  if (lua_isboolean(L, 9) == 1) {
    antialiasing = lua_toboolean(L, 9);
    if (lua_isnumber(L, 10) == 1) {
      w = luaL_checknumber(L, 10);
      if (w < 1) {
        w = 1;
      }
    }
  }

  if (antialiasing && buffer_info.channels == 4) {
    draw_circle_vu(posx, posy, diameter / 2, r, g, b, a, w);
  } else {
    draw_circle(posx, posy, diameter, r, g, b, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_filled_circle_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  int32_t diameter = luaL_checknumber(L, 4);
  uint32_t r = luaL_checknumber(L, 5);
  uint32_t g = luaL_checknumber(L, 6);
  uint32_t b = luaL_checknumber(L, 7);
  uint32_t a = 0;
  if (lua_isnumber(L, 8) == 1) {
    a = luaL_checknumber(L, 8);
  }
  bool antialiasing = false;
  if (lua_isboolean(L, 9) == 1) {
    antialiasing = lua_toboolean(L, 9);
  }

  if (antialiasing && buffer_info.channels == 4) {
    draw_filled_circle_vu(posx, posy, diameter / 2, r, g, b, a);
  } else {
    draw_filled_circle(posx, posy, diameter, r, g, b, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int fill_texture_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  uint8_t r = luaL_checknumber(L, 2);
  uint8_t g = luaL_checknumber(L, 3);
  uint8_t b = luaL_checknumber(L, 4);
  uint8_t a = 0;
  if (lua_isnumber(L, 5) == 1) {
    a = luaL_checknumber(L, 5);
  }
  int line_size = buffer_info.width * buffer_info.channels;
  uint8_t * line = new uint8_t[line_size];
  for (int i = 0; i < line_size; i += buffer_info.channels) {
    line[i] = r;
    line[i + 1] = g;
    line[i + 2] = b;
    if (buffer_info.channels == 4) {
      line[i + 3] = a;
    }
  }
  for (int i = 0; i < buffer_info.src_size; i += line_size) {
    memcpy( & buffer_info.bytes[i], line, line_size);
  }
  delete[] line;
  assert(top == lua_gettop(L));
  return 0;
}

static int draw_rect_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  uint32_t sizex = luaL_checknumber(L, 4);
  uint32_t sizey = luaL_checknumber(L, 5);
  uint32_t r = luaL_checknumber(L, 6);
  uint32_t g = luaL_checknumber(L, 7);
  uint32_t b = luaL_checknumber(L, 8);
  uint32_t a = 0;
  if (lua_isnumber(L, 9) == 1) {
    a = luaL_checknumber(L, 9);
  }

  int half_size_x = sizex / 2;
  int half_size_y = sizey / 2;
  int newposx = 0;
  int newposy = 0;
  for (int y = -half_size_y; y < half_size_y; y++) {
    if (y == -half_size_y || y == half_size_y - 1) {
      for (int x = -half_size_x; x < half_size_x; x++) {
        newposx = x + posx;
        newposy = y + posy;
        putpixel(newposx, newposy, r, g, b, a);
      }
    } else {
      putpixel(-half_size_x + posx, y + posy, r, g, b, a);
      putpixel(half_size_x - 1 + posx, y + posy, r, g, b, a);
    }
  }

  assert(top == lua_gettop(L));
  return 0;
}

bool sort_coords(const Point & p1,
  const Point & p2) {
  if (p1.y == p2.y)
    return p1.x < p2.x;
  return p1.y < p2.y;
}

static int draw_filled_rect_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t posx = luaL_checknumber(L, 2);
  int32_t posy = luaL_checknumber(L, 3);
  uint32_t sizex = luaL_checknumber(L, 4);
  uint32_t sizey = luaL_checknumber(L, 5);
  uint32_t r = luaL_checknumber(L, 6);
  uint32_t g = luaL_checknumber(L, 7);
  uint32_t b = luaL_checknumber(L, 8);
  uint32_t a = 0;
  if (lua_isnumber(L, 9) == 1) {
    a = luaL_checknumber(L, 9);
  }
  int angle = 0;
  if (lua_isnumber(L, 10) == 1) {
    angle = luaL_checknumber(L, 10);
  }

  int half_size_x = sizex / 2;
  int half_size_y = sizey / 2;
  if (angle == 0) {
    int newposx = 0;
    int newposy = 0;
    for (int y = -half_size_y; y < half_size_y; y++) {
      newposy = y + posy;
      fill_line(posx - half_size_x, posx + half_size_x, newposy, r, g, b, a);
    }
  } else {
    std::vector < Point > vec(4);
    float rd = M_PI / 180 * angle;
    float cs = cos(rd);
    float sn = sin(rd);
    vec[0].x = (-half_size_x) * cs - (-half_size_y) * sn;
    vec[0].y = (-half_size_x) * sn + (-half_size_y) * cs;
    vec[1].x = half_size_x * cs - half_size_y * sn;
    vec[1].y = half_size_x * sn + half_size_y * cs;
    vec[2].x = (-half_size_x) * cs - half_size_y * sn;
    vec[2].y = (-half_size_x) * sn + half_size_y * cs;
    vec[3].x = half_size_x * cs - (-half_size_y) * sn;
    vec[3].y = half_size_x * sn + (-half_size_y) * cs;
    for (int i = 0; i < 4; i++) {
      vec[i].x += posx;
      vec[i].y += posy;
    }
    sort(vec.begin(), vec.end(), sort_coords);
    draw_triangle(vec[0].x, vec[0].y, vec[1].x, vec[1].y, vec[2].x, vec[2].y, r, g, b, a);
    draw_triangle(vec[1].x, vec[1].y, vec[2].x, vec[2].y, vec[3].x, vec[3].y, r, g, b, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int draw_pixel_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x = luaL_checknumber(L, 2);
  int32_t y = luaL_checknumber(L, 3);
  uint32_t r = luaL_checknumber(L, 4);
  uint32_t g = luaL_checknumber(L, 5);
  uint32_t b = luaL_checknumber(L, 6);
  uint32_t a = 0;
  if (lua_isnumber(L, 7) == 1) {
    a = luaL_checknumber(L, 7);
  }

  mixpixel(x, y, r, g, b, a);

  assert(top == lua_gettop(L));
  return 0;
}

static int read_color_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x = luaL_checknumber(L, 2);
  int32_t y = luaL_checknumber(L, 3);

  if (!in_buffer(x, y)) {
    assert(top == lua_gettop(L));
    return 0;
  }

  int i = xytoi(x, y);
  lua_pushnumber(L, buffer_info.bytes[i]);
  lua_pushnumber(L, buffer_info.bytes[i + 1]);
  lua_pushnumber(L, buffer_info.bytes[i + 2]);
  if (buffer_info.channels == 4) {
    lua_pushnumber(L, buffer_info.bytes[i + 3]);
  }
  assert(top + buffer_info.channels == lua_gettop(L));
  return buffer_info.channels;
}

static int draw_bezier_lua(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x0 = luaL_checknumber(L, 2);
  int32_t y0 = luaL_checknumber(L, 3);
  int32_t xc = luaL_checknumber(L, 4);
  int32_t yc = luaL_checknumber(L, 5);
  int32_t x1 = luaL_checknumber(L, 6);
  int32_t y1 = luaL_checknumber(L, 7);
  uint32_t r = luaL_checknumber(L, 8);
  uint32_t g = luaL_checknumber(L, 9);
  uint32_t b = luaL_checknumber(L, 10);
  uint32_t a = 0;
  if (lua_isnumber(L, 11) == 1) {
    a = luaL_checknumber(L, 11);
  }

  int max_dx = fmax(fmax(x0, xc), x1) - fmin(fmin(x0, xc), x1);
  int max_dy = fmax(fmax(y0, yc), y1) - fmin(fmin(y0, yc), y1);

  double max_d = fmax(max_dx, max_dy) * 2;
  double dt = 1.0 / max_d;

  for (double t = 0; t < 1; t += dt) {
    double opt1 = pow((1 - t), 2);
    double opt2 = 2 * t * (1 - t);
    double opt3 = pow(t, 2);
    int xt = (opt1 * x0) + (opt2 * xc) + (opt3 * x1);
    int yt = (opt1 * y0) + (opt2 * yc) + (opt3 * y1);
    putpixel(xt, yt, r, g, b, a);
  }

  assert(top == lua_gettop(L));
  return 0;
}

// To display export progress
dmScript::LuaCallbackInfo* g_MyCallbackInfo = 0;

static void InvokeCallback(dmScript::LuaCallbackInfo* cbk, double progress)
{
  if (!dmScript::IsCallbackValid(cbk))
      return;

  lua_State* L = dmScript::GetCallbackLuaContext(cbk);
  DM_LUA_STACK_CHECK(L, 0)

  if (!dmScript::SetupCallback(cbk))
  {
      dmLogError("Failed to setup callback");
      return;
  }

  lua_pushnumber(L, progress);

  // printf("Call lua callback %f\n", progress);

  dmScript::PCall(L, 2, 0); // instance + 2

  dmScript::TeardownCallback(cbk);
}

static int register_progress_callback(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    g_MyCallbackInfo = dmScript::CreateCallback(L, 1);

    return 0;
}

static int set_pixel(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x = luaL_checknumber(L, 2);
  int32_t y = luaL_checknumber(L, 3);
  uint32_t r = luaL_checknumber(L, 4);
  uint32_t g = luaL_checknumber(L, 5);
  uint32_t b = luaL_checknumber(L, 6);
  uint32_t a = 255;
  putpixel(x, y, r, g, b, a);

  assert(top == lua_gettop(L));
  return 0;
}

static int get_pixel(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t x = luaL_checknumber(L, 2);
  int32_t y = luaL_checknumber(L, 3);

  int32_t i = xytoi(x, y);

  for (int l = 0; l < buffer_info.channels; ++l)
    lua_pushnumber(L, buffer_info.bytes[i + l]);
  return buffer_info.channels;
}

static int prepare_points(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  int32_t width = luaL_checknumber(L, 2);
  int32_t height = luaL_checknumber(L, 3);

  clear_point();
  points = new int[buffer_info.width * buffer_info.height];

  for (int i = 0; i < buffer_info.height; ++i) {
    for (int j = 0; j < buffer_info.width; ++j) {
      if (i < height && j < width)
        points[i * buffer_info.width + j] = !buffer_info.bytes[xytoi(j, i)] && !buffer_info.bytes[xytoi(j, i) + 1] &&
        !buffer_info.bytes[xytoi(j, i) + 2] || (buffer_info.channels == 4 &&
          !buffer_info.bytes[xytoi(j, i) + 3]);
      else
        points[i * buffer_info.width + j] = 1;

    }
  }
  assert(top == lua_gettop(L));
  return 0;
}

static int set_texture(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);
  char * str = (char * ) luaL_checkstring(L, 2);
  int32_t width = luaL_checknumber(L, 3);
  int32_t height = luaL_checknumber(L, 4);
  bool normalize = lua_toboolean(L, 5);
  bool reverse = lua_toboolean(L, 6);

  int k;
  printf("Reverse: %d\n", reverse);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      for (int l = 0; l < buffer_info.channels; ++l) {
        k = i;
        if(reverse)
          k = (height - i - 1);
        buffer_info.bytes[i * buffer_info.width * buffer_info.channels + j * buffer_info.channels + l] =
          str[k * width * buffer_info.channels + j * buffer_info.channels + l];
      }
    }
  }
  int water_diff, white_diff; //, black_diff;
  int accuracy = 100;
  if (normalize) {
    for (uint32_t i = 0; i < buffer_info.src_size; i += 4) {
      if (buffer_info.bytes[i + 3]) {
        water_diff = abs(buffer_info.bytes[i] - 180) + abs(buffer_info.bytes[i + 1] - 210) + abs(buffer_info.bytes[i + 2] - 236);
        white_diff = abs(buffer_info.bytes[i] - 255) + abs(buffer_info.bytes[i + 1] - 255) + abs(buffer_info.bytes[i + 2] - 255);
        // black_diff = abs(buffer_info.bytes[i] - 0) + abs(buffer_info.bytes[i + 1] - 0) + abs(buffer_info.bytes[i + 2] - 0);
        if(water_diff < accuracy)
        {
          buffer_info.bytes[i] = 180;
          buffer_info.bytes[i + 1] = 210;
          buffer_info.bytes[i + 2] = 236;
        }
        if(white_diff < accuracy)
        {
          buffer_info.bytes[i] = 255;
          buffer_info.bytes[i + 1] = 255;
          buffer_info.bytes[i + 2] = 255;
        }
        if(!(water_diff < accuracy || white_diff < accuracy))
        {
          buffer_info.bytes[i] = 0;
          buffer_info.bytes[i + 1] = 0;
          buffer_info.bytes[i + 2] = 0;
        }
      }
    }
  }

  assert(top == lua_gettop(L));
  return 0;
}

static int get_image_data(lua_State * L) {
  int top = lua_gettop(L) + 5;

  read_and_validate_buffer_info(L, 1);

  lua_pushlstring(L, (const char*) buffer_info.bytes, buffer_info.src_size);
  // assert(top == lua_gettop(L));
  return 1;
}

static int find_element(dmArray < Color > & colors, Color c) {
  for (uint32_t i = 0; i < colors.Size(); ++i)
    if (colors[i].r == c.r && colors[i].g == c.g && colors[i].b == c.b)
      return i;
  return -1;
}

static void erase_system_colors(dmArray < Color > & colors) {
  for (int32_t i = colors.Size() - 1; i >= 0; --i)
    if (colors[i].r == 180 && colors[i].g == 210 && colors[i].b == 236 ||
      colors[i].r == 255 && colors[i].g == 255 && colors[i].b == 255 ||
      colors[i].r == 0 && colors[i].g == 0 && colors[i].b == 0)
      colors.EraseSwap(i);
}

template < class T >
  const T & clamp(const T & x,
    const T & upper,
      const T & lower) {
    return min(upper, max(x, lower));
  }

char * concat(const char * s1, const char * s2) {
  size_t len1 = strlen(s1);
  size_t len2 = strlen(s2);

  char * result = (char * ) malloc(len1 + len2 + 1);

  if (!result) {
    fprintf(stderr, "malloc() failed: insufficient memory!\n");
    return NULL;
  }

  memcpy(result, s1, len1);
  memcpy(result + len1, s2, len2 + 1);

  return result;
}

static void replace_color(Color c1, Color c2)
{
  for (int32_t i = 0; i < buffer_info.src_size; i += 4) {
    if (buffer_info.bytes[i] == c1.r && buffer_info.bytes[i + 1] == c1.g &&
      buffer_info.bytes[i + 2] == c1.b) {
        buffer_info.bytes[i] = c2.r;
        buffer_info.bytes[i + 1] = c2.g;
        buffer_info.bytes[i + 2] = c2.b;
    }
  }
}

// 124 instead 128 because there is extrude borders
static int used_124, used_252, used_508, used_1020, used_2044;
static int max_124 = 1024, max_252 = 256, max_508 = 64, max_1020 = 16, max_2044 = 4;

static int get_size_for_texture(int size_x, int size_y) {
  if (size_x <= 124 && size_y <= 124 && used_124 < max_124)
    return 124;
  if (size_x <= 252 && size_y <= 252 && used_252 < max_252)
    return 252;
  if (size_x <= 508 && size_y <= 508 && used_508 < max_508)
    return 508;
  if (size_x <= 1020 && size_y <= 1020 && used_1020 < max_1020)
    return 1020;
  if (size_x <= 2044 && size_y <= 2044 && used_2044 < max_2044)
    return 2044;
  printf("ERROR! No texture found for the province! Try to reduce the number of provinces or do not create textures larger than 2044x2044\n");
  printf("Province data: %dx%d\n", size_x, size_y);
  printf("Textures used:\n124x124:   %d\n252x252:   %d\n508x508:   %d\n1020x1020: %d\n2044x2044: %d\n", used_124, used_252,
    used_508, used_1020, used_2044);
  fflush(stdout);
  return 0;
}

static uint8_t * blur(uint8_t * bytes, int min_x, int max_x, int min_y, int max_y) {
  uint8_t * blurred_image = new uint8_t[buffer_info.width * buffer_info.height];
  for (int i = 0; i < buffer_info.width * buffer_info.height; ++i) {
    blurred_image[i] = 0;
  }

  // Blurring
  double average = 0;
  int count_number = 0;
  int box = 1; // set 1 to have a 3x3 box

  for (int i = min_y; i <= max_y; ++i) {
    for (int j = min_x; j <= max_x; ++j) {
      average = 0;
      count_number = 0;

      for (int li = i - box; li <= i + box; ++li) {
        if (li < 0) {
          li = 0;
        }
        if (li == buffer_info.height) {
          break;
        }

        for (int col = j - box; col <= j + box; col++) {
          if (col == buffer_info.width) break;
          if (col < 0) {
            col = 0;
          }

          count_number++;

          average += bytes[li * buffer_info.width + col];
        }

      }

      blurred_image[i * buffer_info.width + j] = (int) round(average / count_number);
    }
  }
  return blurred_image;
}

static void export_to_ppm(char * file_path, int n, uint8_t* bytes, int min_x, int max_x, int min_y, int max_y)
{
  FILE * fp;

  n = n + 1;

  std::string num_string = std::to_string(n);
  const char* num = num_string.c_str();

  char* ppm_path = concat(file_path, "exported_map/ppm_");
  char* ppm_name = concat(ppm_path, num);

  if ((fp = fopen(ppm_name, "w")) == NULL) {
    printf("File open error");
    free(ppm_path);
    free(ppm_name);
    return;
  }

  fprintf(fp, "P3 %d %d 255\n", max_x - min_x + 1, max_y - min_y + 1);

  for (int i = max_y; i >= min_y; --i) {
    for (int j = min_x; j <= max_x; ++j)
      fprintf(fp, "%d %d %d\n", bytes[i * buffer_info.width + j], bytes[i * buffer_info.width + j], bytes[i * buffer_info.width + j]);
  }

  printf("Export to PPM: %d\n", n);

  fclose(fp);
  free(ppm_path);
  free(ppm_name);
}

static int export_province(Color & color, int n, char * file_path, bool generate_adjacency, dmArray < Color > & colors,
  int* adjacency_list, int num_of_provinces) {
  uint8_t* bytes = new uint8_t[buffer_info.width * buffer_info.height];

  for (int i = 0; i < buffer_info.width * buffer_info.height; ++i) {
    bytes[i] = 0;
  }

  for (int32_t i = 0; i < buffer_info.src_size; i += 4) {
    if (buffer_info.bytes[i] == color.r && buffer_info.bytes[i + 1] == color.g &&
      buffer_info.bytes[i + 2] == color.b) {
      bytes[i / 4] = 255;
    }
  }

  int min_x = buffer_info.width, max_x = 0, min_y = buffer_info.height, max_y = 0;
  // To optimize blur
  for (int i = 0; i < buffer_info.height; ++i) {
    for (int j = 0; j < buffer_info.width; ++j) {
      if (bytes[i * buffer_info.width + j]) {
        if (i < min_y)
          min_y = i;
        if (i > max_y)
          max_y = i;
        if (j < min_x)
          min_x = j;
        if (j > max_x)
          max_x = j;
      }
    }
  }

  uint8_t * blurred_image = blur(bytes, min_x, max_x, min_y, max_y);
  // To get the borders of the image
  for (int i = 0; i < buffer_info.height; ++i) {
    for (int j = 0; j < buffer_info.width; ++j) {
      if (blurred_image[i * buffer_info.width + j]) {
        if (i < min_y)
          min_y = i;
        if (i > max_y)
          max_y = i;
        if (j < min_x)
          min_x = j;
        if (j > max_x)
          max_x = j;
      }
    }
  }

  // size of provinces must be a multiple of two
  if ((max_x - min_x + 1) % 2)
    if (min_x > 0)
      min_x = min_x - 1;
    else
      max_x = max_x + 1;

  if ((max_y - min_y + 1) % 2)
    if (min_y > 0)
      min_y = min_y - 1;
    else
      max_y = max_y + 1;

  if(EXPORT_TO_PPM)
    export_to_ppm(file_path, n, blurred_image, min_x, max_x, min_y, max_y);
  // For debug. UPD: it does not work
  // for (int i = 0; i < buffer_info.height; i++)
  // {
  // for (int j = 0; i < buffer_info.width; i++)
  // {
  // buffer_info.bytes[i*buffer_info.width*4 + j*4] = bytes[i*buffer_info.width + j];
  // }
  // }

  FILE * fp;

  // Provinces should be counted from 1
  n = n + 1;
  // Convert province id to string
  std::string num_string = std::to_string(n);
  const char* num = num_string.c_str();

  // int output_width = max_x - min_x + 1;
  // int output_height = max_y - min_y + 1;

  int texture_size = get_size_for_texture(max_x - min_x + 1, max_y - min_y + 1);
  if (texture_size == 124)
    used_124++;
  if (texture_size == 252)
    used_252++;
  if (texture_size == 508)
    used_508++;
  if (texture_size == 1020)
    used_1020++;
  if (texture_size == 2044)
    used_2044++;
  if (texture_size == 0)
  {
  	Color white_color;
  	white_color.r = 255;
  	white_color.g = 255;
  	white_color.b = 255;
  	replace_color(color, white_color);
  	delete[] bytes;
    delete[] blurred_image;
  	return -1;
  }

  uint8_t * output_generated_data = new uint8_t[texture_size * texture_size];
  uint8_t * output_blurred_data = new uint8_t[texture_size * texture_size];

  for (int i = 0; i < texture_size * texture_size; ++i) {
    output_generated_data[i] = 0;
  }

  for (int i = 0; i < texture_size * texture_size; ++i) {
    output_blurred_data[i] = 0;
  }

  int ki = (texture_size - max_y + min_y - 1) / 2;
  int kj = (texture_size - max_x + min_x - 1) / 2;

  for (int i = min_y; i <= max_y; ++i) {
    for (int j = min_x; j <= max_x; ++j) {
      output_generated_data[ki * texture_size + kj] = bytes[i * buffer_info.width + j];
      ++kj;
    }
    ++ki;
    kj = (texture_size - max_x + min_x - 1) / 2;
  }
  ki = (texture_size - max_y + min_y - 1) / 2;
  kj = (texture_size - max_x + min_x - 1) / 2;
  // Min x, min y: 99, 316, 398, 589, 500, 141, 154
  printf("Min x, min y: %d, %d, %d, %d, %d, %d, %d ", min_x, max_x, min_y, max_y, texture_size, ki, kj);
  for (int i = min_y; i <= max_y; ++i) {
    for (int j = min_x; j <= max_x; ++j) {
      output_blurred_data[ki * texture_size + kj] = blurred_image[i * buffer_info.width + j];
      ++kj;
    }
    ++ki;
    kj = (texture_size - max_x + min_x - 1) / 2;
  }

  // Export data for perfect click tracking
  char* generated_data_file_path = concat(file_path, "exported_map/generated_data/");
  char* generated_data_file_name = concat(generated_data_file_path, num);

  char* file_mode = "wb";
  if(EXPORT_TO_PPM)
  	file_mode = "w";

  if ((fp = fopen(generated_data_file_name, file_mode)) == NULL) {
    printf("File open error");
    free(generated_data_file_path);
    free(generated_data_file_name);
    delete[] bytes;
    delete[] blurred_image;
    delete[] output_generated_data;
    delete[] output_blurred_data;
    return 0;
  }

  if(EXPORT_TO_PPM)
  {
  	fprintf(fp, "1");
  	for (int i = max_y; i >= min_y; --i)
  	{
  	  for (int j = min_x; j <= max_x; ++j)
      {
        fprintf(fp, "%d", bytes[i * buffer_info.width + j] == 255 ? 1 : 0);
      }
  	}
  }
  else
  	fwrite(output_generated_data, 1, texture_size * texture_size, fp);

  fclose(fp);

  // Export blurred image to set texture
  char* blurred_image_file_path = concat(file_path, "exported_map/blurred_data/");
  char* blurred_image_file_name = concat(blurred_image_file_path, num);

  if ((fp = fopen(blurred_image_file_name, "wb")) == NULL) {
    printf("File open error");
    free(blurred_image_file_path);
    free(blurred_image_file_name);
    free(generated_data_file_path);
    free(generated_data_file_name);
    delete[] bytes;
    delete[] blurred_image;
    delete[] output_generated_data;
    delete[] output_blurred_data;
    return 0;
  }

  fwrite(output_blurred_data, 1, texture_size * texture_size, fp);

  fclose(fp);

  char* description_file_path = concat(file_path, "exported_map/description/");
  char* description_file_name = concat(description_file_path, num);

  if ((fp = fopen(description_file_name, "wb")) == NULL) {
    printf("File open error");
    free(blurred_image_file_path);
    free(blurred_image_file_name);
    free(generated_data_file_path);
    free(generated_data_file_name);
    free(description_file_path);
    free(description_file_name);
    delete[] bytes;
    delete[] blurred_image;
    delete[] output_generated_data;
    delete[] output_blurred_data;
    return 0;
  }

  bool is_water = color.b >= 225;

  fprintf(fp, "{\"size\":[%d,%d],\"position\":[%d,%d],\"water\":%s}", texture_size, texture_size,
    min_x + (max_x - min_x + 1) / 2, min_y + (max_y - min_y + 1) / 2, is_water ? "true" : "false");
  printf("Set position: %d, %d, %d, %d\n", min_x, max_x, min_x + (max_x - min_x + 1) / 2, min_y + (max_y - min_y + 1) / 2);
  fclose(fp);

  printf("Export province size: %d, %d, %d\n", max_x - min_x + 1, max_y - min_y + 1, texture_size);

  if (generate_adjacency) {
    uint8_t * outline_bytes = new uint8_t[buffer_info.width * buffer_info.height];

    for (int i = 0; i < buffer_info.width * buffer_info.height; ++i) {
      outline_bytes[i] = 0;
    }
    for (int i = min_y + 1; i < max_y - 1; ++i) {
      for (int j = min_x + 1; j < max_x - 1; ++j) {
        // If near different color
        if (bytes[i * buffer_info.width + j] && (
            !bytes[i * buffer_info.width + j + 1] ||
            !bytes[i * buffer_info.width + j - 1] ||
            !bytes[(i + 1) * buffer_info.width + j] ||
            !bytes[(i - 1) * buffer_info.width + j]
          )) {
          outline_bytes[i * buffer_info.width + j] = 255;
        }
      }
    }
    // For test outline
    // for (int i = min_y*buffer_info.width + min_x; i < buffer_info.width * buffer_info.height; i++)
    // {
    //  buffer_info.bytes[i*4 + 3] = outline_bytes[i];
    // }

    dmArray < Color > colors_found;
    colors_found.SetCapacity(num_of_provinces);

    int r = 5; // look for a neighbor in 25x25 area around the pixel
    for (int i = min_y + r; i <= max_y - r; ++i) {
      for (int j = min_x + r; j <= max_x - r; ++j) {
        if (outline_bytes[i * buffer_info.width + j]) {
          for (int li = i - r; li <= i + r; ++li) {
            for (int lj = j - r; lj <= j + r; ++lj) {
              Color c;
              c.r = buffer_info.bytes[xytoi(lj, li)];
              c.g = buffer_info.bytes[xytoi(lj, li) + 1];
              c.b = buffer_info.bytes[xytoi(lj, li) + 2];
              if (
                !(c.r == color.r && c.g == color.g && c.b == color.b) &&
                !(c.r == 255 && c.g == 255 && c.b == 255) &&
                !(c.r == 0 && c.g == 0 && c.b == 0) &&
                !(c.r == 180 && c.g == 210 && c.b == 236) &&
                find_element(colors_found, c) == -1
              ) {
                // printf("Found new neighbor %d: %d, %d, %d for color %d, %d, %d", n, c.r, c.g, c.b, color.r, color.g, color.b);
                // + 1 because counting provinces with 1
                adjacency_list[num_of_provinces * (n - 1) + colors_found.Size()] = find_element(colors, c) + 1;
                colors_found.Push(c);
              }
            }
          }
        }
      }
    }

    delete[] outline_bytes;
  }

  free(blurred_image_file_path);
  free(blurred_image_file_name);
  free(generated_data_file_path);
  free(generated_data_file_name);
  free(description_file_path);
  free(description_file_name);
  delete[] bytes;
  delete[] blurred_image;
  delete[] output_generated_data;
  delete[] output_blurred_data;
  return is_water;
}

static void save_adjacency(int * adjacency_list, int size, char * file_path) {
  FILE * fp;

  char * file_name = concat(file_path, "exported_map/adjacency.dat");

  if ((fp = fopen(file_name, "w")) == NULL) {
    printf("File open error");
    free(file_name);
    return;
  }

  for (int i = 0; i < size; ++i) {
    fprintf(fp, "%d ", i + 1);
    for (int j = 0; j < size; ++j) {
      if (adjacency_list[size * i + j])
        fprintf(fp, "%d ", adjacency_list[size * i + j]);
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
  free(file_name);
}

struct ExportData {
  char* file_path;
  bool generate_adjacency;
  dmArray<Color> colors;
  int* adjacency_list;
  int num_of_provinces;
} export_data;

Timer timer;

static int handle_image(lua_State * L) {
  int top = lua_gettop(L) + 3;

  read_and_validate_buffer_info(L, 1);

  int32_t n = 0; // Num of provinces

  export_data.colors.SetCapacity(4096); // Max count of provinces

  timer.reset();

  for (uint32_t i = 0; i < buffer_info.src_size; i += 4) {
    if (buffer_info.bytes[i + 3]) {
      Color cur_color;
      cur_color.r = buffer_info.bytes[i];
      cur_color.g = buffer_info.bytes[i + 1];
      cur_color.b = buffer_info.bytes[i + 2];
      if (find_element(export_data.colors, cur_color) == -1) {
        // printf("New color: %d, %d, %d\n", buffer_info.bytes[i], buffer_info.bytes[i + 1], buffer_info.bytes[i + 2]);
        export_data.colors.Push(cur_color);
      }
    }
  }

  erase_system_colors(export_data.colors);

  export_data.file_path = (char * ) luaL_checkstring(L, 2);

  int i = 0;

  export_data.generate_adjacency = lua_toboolean(L, 3);

  if (export_data.generate_adjacency) {
    export_data.adjacency_list = new int[export_data.colors.Size() * export_data.colors.Size()];
    for (int i = 0; i < export_data.colors.Size() * export_data.colors.Size(); ++i) {
      export_data.adjacency_list[i] = 0;
    }
  }

  used_124 = used_252 = used_508 = used_1020 = used_2044 = 0;
  export_data.num_of_provinces = export_data.colors.Size();

  lua_pushnumber(L, export_data.colors.Size());
  return 1;
}

static int export_image(lua_State * L) {
  int i = luaL_checknumber(L, 1);
  char* water_provinces = (char*)luaL_checkstring(L, 2);

  printf("Export: %d", i);

  printf("Export provinces: %.2f%%. Generate adjacency: %d\n", (double) i / export_data.num_of_provinces * 100, export_data.generate_adjacency);
  fflush(stdout);
  InvokeCallback(g_MyCallbackInfo,  (double) i / export_data.num_of_provinces * 100);
  water_provinces[i] = export_province(export_data.colors[i], i, export_data.file_path, 
    export_data.generate_adjacency, export_data.colors, export_data.adjacency_list,
    export_data.num_of_provinces);
  if(water_provinces[i] == -1) 
  	lua_pushnumber(L, -1);
  else
  	lua_pushnumber(L, 0);
  return 1;
}

static int finish_export(lua_State * L) {
  printf("Textures used:\n124x124:   %d\n252x252:   %d\n508x508:   %d\n1020x1020: %d\n2044x2044: %d\n", used_124, used_252,
    used_508, used_1020, used_2044);

 // assert(top == lua_gettop(L));
  if (export_data.generate_adjacency) {
    // For debug
    // for (int i = 0; i < colors.Size(); ++i)
    // {
    //  printf("From province %d you can go to: ", i + 1);
    //  for (int j = 0; j < colors.Size(); ++j)
    //  {
    //    if(adjacency_list[colors.Size() * i + j])
    //      printf("%d ", adjacency_list[colors.Size() * i + j]);
    //  }
    //  printf("\n");
    // }
    save_adjacency(export_data.adjacency_list, export_data.colors.Size(), export_data.file_path);
    delete[] export_data.adjacency_list;
  }

  printf("Done! Elapsed: %f\n", timer.elapsed());
  InvokeCallback(g_MyCallbackInfo, 100);

  if (g_MyCallbackInfo)
    dmScript::DestroyCallback(g_MyCallbackInfo);
    g_MyCallbackInfo = 0;

  return 0;
}

static int get_file_data(lua_State * L) {
  int top = lua_gettop(L) + 5;

  dmScript::LuaHBuffer * lua_buffer = dmScript::CheckBuffer(L, 1);
  dmBuffer::HBuffer buffer = lua_buffer -> m_Buffer;
  if (!dmBuffer::IsBufferValid(buffer)) {
    luaL_error(L, "Buffer is invalid");
  }

  uint8_t * buffer_bytes;
  uint32_t buffer_size;

  dmBuffer::Result r_get_in_bytes = dmBuffer::GetBytes(buffer, (void ** ) & buffer_bytes, & buffer_size);
  if (r_get_in_bytes != dmBuffer::RESULT_OK) {
    luaL_error(L, "Buffer is invalid");
  }

  char * file_name = (char * ) luaL_checkstring(L, 2);
  int width = luaL_checknumber(L, 3);
  int height = luaL_checknumber(L, 4);
  int texture_width = luaL_checknumber(L, 5);
  int id = luaL_checknumber(L, 6);

  uint8_t * bytes = new uint8_t[width * height];

  FILE * fp;

  if ((fp = fopen(file_name, "rb")) == NULL) {
    printf("File open error");
    // assert(top == lua_gettop(L));
    delete[] bytes;
    return 0;
  }

  int y_offset = (id - 1) / (4096 / (texture_width + 4)) * (texture_width + 4);
  int x_offset = (id - 1) % (4096 / (texture_width + 4));
  x_offset *= (texture_width + 4);
  // extrude borders
  y_offset += 2;
  x_offset += 2;

  // printf("y and x offsets: %d, %d, %d\n", y_offset, x_offset, id);

  int c = fread(bytes, 1, width * height, fp);
  // printf("Loaded bytes: %d, %d\n", width * height, c);


  // printf("Size: %d, %d, %d", width, height, texture_width);

  // printf("Draw: %d, %d", y_offset, y_offset + texture_width);
  for (int i = y_offset; i < y_offset + texture_width; ++i) {
    for (int j = x_offset; j < x_offset + texture_width; ++j) {
      buffer_bytes[i * 4096 + j] = bytes[(i - y_offset) * width + (j - x_offset)];

      // For debug
      // if ((i == y_offset || j == x_offset || i == y_offset + texture_width - 1 ||
      //  j == x_offset + texture_width - 1))
      //  buffer_bytes[i * 4096 + j] = 255;
    }
  }
  // printf("Province data: %d, %d, %d, %d\n", width, height, texture_width, buffer_size);

  fclose(fp);

  // assert(top == lua_gettop(L));
  delete[] bytes;
  return 0;
}

// Functions exposed to Lua
static
const luaL_reg Module_methods[] = {
  {
    "start_fill",
    start_record_points_lua
  },
  {
    "end_fill",
    stop_record_points_lua
  },
  {
    "fill_area",
    fill_area_lua
  },
  // {"triangle", draw_triangle_lua},
  {
    "line",
    draw_line_lua
  },
  {
    "gradient_line",
    draw_gradient_line_lua
  },
  {
    "arc",
    draw_arc_lua
  },
  {
    "filled_arc",
    draw_filled_arc_lua
  },
  {
    "gradient_arc",
    draw_gradient_arc_lua
  },
  {
    "circle",
    draw_circle_lua
  },
  {
    "filled_circle",
    draw_filled_circle_lua
  },
  {
    "fill",
    fill_texture_lua
  },
  {
    "rect",
    draw_rect_lua
  },
  {
    "filled_rect",
    draw_filled_rect_lua
  },
  {
    "pixel",
    draw_pixel_lua
  },
  {
    "color",
    read_color_lua
  },
  {
    "bezier",
    draw_bezier_lua
  },
  {
    "set_pixel",
    set_pixel
  },
  {
    "get_pixel",
    get_pixel
  },
  {
    "prepare_fill",
    prepare_points
  },
  {
    "set_texture",
    set_texture
  },
  {
    "get_image_data",
    get_image_data
  },
  {
    "handle_image",
    handle_image
  },
  {
    "export_image",
    export_image
  },
  {
    "finish_export",
    finish_export
  },
  {
    "get_file_data",
    get_file_data
  },
  {
    "register_progress_callback",
    register_progress_callback
  },

  {
    0,
    0
  }
};

static void LuaInit(lua_State * L) {
  int top = lua_gettop(L);
  luaL_register(L, MODULE_NAME, Module_methods);
  lua_pop(L, 1);
  assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDrawPixelsExtension(dmExtension::AppParams * params) {
  return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDrawPixelsExtension(dmExtension::Params * params) {
  LuaInit(params -> m_L);
  printf("Registered %s Extension\n", MODULE_NAME);
  return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDrawPixelsExtension(dmExtension::AppParams * params) {
  return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDrawPixelsExtension(dmExtension::Params * params) {
  return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DrawPixels, LIB_NAME, AppInitializeDrawPixelsExtension, AppFinalizeDrawPixelsExtension, InitializeDrawPixelsExtension, 0, 0, FinalizeDrawPixelsExtension)