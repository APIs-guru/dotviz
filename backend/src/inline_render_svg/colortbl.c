#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "color.h"
#include "agxbuf.h"
#include "util/unreachable.h"

typedef struct rgba_t {
  // FIXME: remove alpha that almost always is 255
  unsigned char r, g, b, a;
} rgba_t;

typedef struct color_t {
  char const *name;
  rgba_t value;
} color_t;

typedef struct colorscheme_t {
  const char *name;
  const color_t *colors;
  size_t num_colors;
} colorscheme_t;

static const color_t accent3_colors[] = {
    {"1", {127, 201, 127, 255}},
    {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}},
};
static const colorscheme_t accent3 = {"accent3", accent3_colors,
                                      sizeof(accent3_colors) /
                                          sizeof(accent3_colors[0])};

static const color_t accent4_colors[] = {
    {"1", {127, 201, 127, 255}},
    {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}},
    {"4", {255, 255, 153, 255}},
};
static const colorscheme_t accent4 = {"accent4", accent4_colors,
                                      sizeof(accent4_colors) /
                                          sizeof(accent4_colors[0])};

static const color_t accent5_colors[] = {
    {"1", {127, 201, 127, 255}}, {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}}, {"4", {255, 255, 153, 255}},
    {"5", {56, 108, 176, 255}},
};
static const colorscheme_t accent5 = {"accent5", accent5_colors,
                                      sizeof(accent5_colors) /
                                          sizeof(accent5_colors[0])};

static const color_t accent6_colors[] = {
    {"1", {127, 201, 127, 255}}, {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}}, {"4", {255, 255, 153, 255}},
    {"5", {56, 108, 176, 255}},  {"6", {240, 2, 127, 255}},
};
static const colorscheme_t accent6 = {"accent6", accent6_colors,
                                      sizeof(accent6_colors) /
                                          sizeof(accent6_colors[0])};

static const color_t accent7_colors[] = {
    {"1", {127, 201, 127, 255}}, {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}}, {"4", {255, 255, 153, 255}},
    {"5", {56, 108, 176, 255}},  {"6", {240, 2, 127, 255}},
    {"7", {191, 91, 23, 255}},
};
static const colorscheme_t accent7 = {"accent7", accent7_colors,
                                      sizeof(accent7_colors) /
                                          sizeof(accent7_colors[0])};

static const color_t accent8_colors[] = {
    {"1", {127, 201, 127, 255}}, {"2", {190, 174, 212, 255}},
    {"3", {253, 192, 134, 255}}, {"4", {255, 255, 153, 255}},
    {"5", {56, 108, 176, 255}},  {"6", {240, 2, 127, 255}},
    {"7", {191, 91, 23, 255}},   {"8", {102, 102, 102, 255}},
};
static const colorscheme_t accent8 = {"accent8", accent8_colors,
                                      sizeof(accent8_colors) /
                                          sizeof(accent8_colors[0])};

static const color_t blues3_colors[] = {
    {"1", {222, 235, 247, 255}},
    {"2", {158, 202, 225, 255}},
    {"3", {49, 130, 189, 255}},
};
static const colorscheme_t blues3 = {
    "blues3", blues3_colors, sizeof(blues3_colors) / sizeof(blues3_colors[0])};

static const color_t blues4_colors[] = {
    {"1", {239, 243, 255, 255}},
    {"2", {189, 215, 231, 255}},
    {"3", {107, 174, 214, 255}},
    {"4", {33, 113, 181, 255}},
};
static const colorscheme_t blues4 = {
    "blues4", blues4_colors, sizeof(blues4_colors) / sizeof(blues4_colors[0])};

static const color_t blues5_colors[] = {
    {"1", {239, 243, 255, 255}}, {"2", {189, 215, 231, 255}},
    {"3", {107, 174, 214, 255}}, {"4", {49, 130, 189, 255}},
    {"5", {8, 81, 156, 255}},
};
static const colorscheme_t blues5 = {
    "blues5", blues5_colors, sizeof(blues5_colors) / sizeof(blues5_colors[0])};

static const color_t blues6_colors[] = {
    {"1", {239, 243, 255, 255}}, {"2", {198, 219, 239, 255}},
    {"3", {158, 202, 225, 255}}, {"4", {107, 174, 214, 255}},
    {"5", {49, 130, 189, 255}},  {"6", {8, 81, 156, 255}},
};
static const colorscheme_t blues6 = {
    "blues6", blues6_colors, sizeof(blues6_colors) / sizeof(blues6_colors[0])};

static const color_t blues7_colors[] = {
    {"1", {239, 243, 255, 255}}, {"2", {198, 219, 239, 255}},
    {"3", {158, 202, 225, 255}}, {"4", {107, 174, 214, 255}},
    {"5", {66, 146, 198, 255}},  {"6", {33, 113, 181, 255}},
    {"7", {8, 69, 148, 255}},
};
static const colorscheme_t blues7 = {
    "blues7", blues7_colors, sizeof(blues7_colors) / sizeof(blues7_colors[0])};

static const color_t blues8_colors[] = {
    {"1", {247, 251, 255, 255}}, {"2", {222, 235, 247, 255}},
    {"3", {198, 219, 239, 255}}, {"4", {158, 202, 225, 255}},
    {"5", {107, 174, 214, 255}}, {"6", {66, 146, 198, 255}},
    {"7", {33, 113, 181, 255}},  {"8", {8, 69, 148, 255}},
};
static const colorscheme_t blues8 = {
    "blues8", blues8_colors, sizeof(blues8_colors) / sizeof(blues8_colors[0])};

static const color_t blues9_colors[] = {
    {"1", {247, 251, 255, 255}}, {"2", {222, 235, 247, 255}},
    {"3", {198, 219, 239, 255}}, {"4", {158, 202, 225, 255}},
    {"5", {107, 174, 214, 255}}, {"6", {66, 146, 198, 255}},
    {"7", {33, 113, 181, 255}},  {"8", {8, 81, 156, 255}},
    {"9", {8, 48, 107, 255}},
};
static const colorscheme_t blues9 = {
    "blues9", blues9_colors, sizeof(blues9_colors) / sizeof(blues9_colors[0])};

static const color_t brbg10_colors[] = {
    {"1", {84, 48, 5, 255}},     {"10", {0, 60, 48, 255}},
    {"2", {140, 81, 10, 255}},   {"3", {191, 129, 45, 255}},
    {"4", {223, 194, 125, 255}}, {"5", {246, 232, 195, 255}},
    {"6", {199, 234, 229, 255}}, {"7", {128, 205, 193, 255}},
    {"8", {53, 151, 143, 255}},  {"9", {1, 102, 94, 255}},
};
static const colorscheme_t brbg10 = {
    "brbg10", brbg10_colors, sizeof(brbg10_colors) / sizeof(brbg10_colors[0])};

static const color_t brbg11_colors[] = {
    {"1", {84, 48, 5, 255}},     {"10", {1, 102, 94, 255}},
    {"11", {0, 60, 48, 255}},    {"2", {140, 81, 10, 255}},
    {"3", {191, 129, 45, 255}},  {"4", {223, 194, 125, 255}},
    {"5", {246, 232, 195, 255}}, {"6", {245, 245, 245, 255}},
    {"7", {199, 234, 229, 255}}, {"8", {128, 205, 193, 255}},
    {"9", {53, 151, 143, 255}},
};
static const colorscheme_t brbg11 = {
    "brbg11", brbg11_colors, sizeof(brbg11_colors) / sizeof(brbg11_colors[0])};

static const color_t brbg3_colors[] = {
    {"1", {216, 179, 101, 255}},
    {"2", {245, 245, 245, 255}},
    {"3", {90, 180, 172, 255}},
};
static const colorscheme_t brbg3 = {
    "brbg3", brbg3_colors, sizeof(brbg3_colors) / sizeof(brbg3_colors[0])};

static const color_t brbg4_colors[] = {
    {"1", {166, 97, 26, 255}},
    {"2", {223, 194, 125, 255}},
    {"3", {128, 205, 193, 255}},
    {"4", {1, 133, 113, 255}},
};
static const colorscheme_t brbg4 = {
    "brbg4", brbg4_colors, sizeof(brbg4_colors) / sizeof(brbg4_colors[0])};

static const color_t brbg5_colors[] = {
    {"1", {166, 97, 26, 255}},   {"2", {223, 194, 125, 255}},
    {"3", {245, 245, 245, 255}}, {"4", {128, 205, 193, 255}},
    {"5", {1, 133, 113, 255}},
};
static const colorscheme_t brbg5 = {
    "brbg5", brbg5_colors, sizeof(brbg5_colors) / sizeof(brbg5_colors[0])};

static const color_t brbg6_colors[] = {
    {"1", {140, 81, 10, 255}},   {"2", {216, 179, 101, 255}},
    {"3", {246, 232, 195, 255}}, {"4", {199, 234, 229, 255}},
    {"5", {90, 180, 172, 255}},  {"6", {1, 102, 94, 255}},
};
static const colorscheme_t brbg6 = {
    "brbg6", brbg6_colors, sizeof(brbg6_colors) / sizeof(brbg6_colors[0])};

static const color_t brbg7_colors[] = {
    {"1", {140, 81, 10, 255}},   {"2", {216, 179, 101, 255}},
    {"3", {246, 232, 195, 255}}, {"4", {245, 245, 245, 255}},
    {"5", {199, 234, 229, 255}}, {"6", {90, 180, 172, 255}},
    {"7", {1, 102, 94, 255}},
};
static const colorscheme_t brbg7 = {
    "brbg7", brbg7_colors, sizeof(brbg7_colors) / sizeof(brbg7_colors[0])};

static const color_t brbg8_colors[] = {
    {"1", {140, 81, 10, 255}},   {"2", {191, 129, 45, 255}},
    {"3", {223, 194, 125, 255}}, {"4", {246, 232, 195, 255}},
    {"5", {199, 234, 229, 255}}, {"6", {128, 205, 193, 255}},
    {"7", {53, 151, 143, 255}},  {"8", {1, 102, 94, 255}},
};
static const colorscheme_t brbg8 = {
    "brbg8", brbg8_colors, sizeof(brbg8_colors) / sizeof(brbg8_colors[0])};

static const color_t brbg9_colors[] = {
    {"1", {140, 81, 10, 255}},   {"2", {191, 129, 45, 255}},
    {"3", {223, 194, 125, 255}}, {"4", {246, 232, 195, 255}},
    {"5", {245, 245, 245, 255}}, {"6", {199, 234, 229, 255}},
    {"7", {128, 205, 193, 255}}, {"8", {53, 151, 143, 255}},
    {"9", {1, 102, 94, 255}},
};
static const colorscheme_t brbg9 = {
    "brbg9", brbg9_colors, sizeof(brbg9_colors) / sizeof(brbg9_colors[0])};

static const color_t bugn3_colors[] = {
    {"1", {229, 245, 249, 255}},
    {"2", {153, 216, 201, 255}},
    {"3", {44, 162, 95, 255}},
};
static const colorscheme_t bugn3 = {
    "bugn3", bugn3_colors, sizeof(bugn3_colors) / sizeof(bugn3_colors[0])};

static const color_t bugn4_colors[] = {
    {"1", {237, 248, 251, 255}},
    {"2", {178, 226, 226, 255}},
    {"3", {102, 194, 164, 255}},
    {"4", {35, 139, 69, 255}},
};
static const colorscheme_t bugn4 = {
    "bugn4", bugn4_colors, sizeof(bugn4_colors) / sizeof(bugn4_colors[0])};

static const color_t bugn5_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {178, 226, 226, 255}},
    {"3", {102, 194, 164, 255}}, {"4", {44, 162, 95, 255}},
    {"5", {0, 109, 44, 255}},
};
static const colorscheme_t bugn5 = {
    "bugn5", bugn5_colors, sizeof(bugn5_colors) / sizeof(bugn5_colors[0])};

static const color_t bugn6_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {204, 236, 230, 255}},
    {"3", {153, 216, 201, 255}}, {"4", {102, 194, 164, 255}},
    {"5", {44, 162, 95, 255}},   {"6", {0, 109, 44, 255}},
};
static const colorscheme_t bugn6 = {
    "bugn6", bugn6_colors, sizeof(bugn6_colors) / sizeof(bugn6_colors[0])};

static const color_t bugn7_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {204, 236, 230, 255}},
    {"3", {153, 216, 201, 255}}, {"4", {102, 194, 164, 255}},
    {"5", {65, 174, 118, 255}},  {"6", {35, 139, 69, 255}},
    {"7", {0, 88, 36, 255}},
};
static const colorscheme_t bugn7 = {
    "bugn7", bugn7_colors, sizeof(bugn7_colors) / sizeof(bugn7_colors[0])};

static const color_t bugn8_colors[] = {
    {"1", {247, 252, 253, 255}}, {"2", {229, 245, 249, 255}},
    {"3", {204, 236, 230, 255}}, {"4", {153, 216, 201, 255}},
    {"5", {102, 194, 164, 255}}, {"6", {65, 174, 118, 255}},
    {"7", {35, 139, 69, 255}},   {"8", {0, 88, 36, 255}},
};
static const colorscheme_t bugn8 = {
    "bugn8", bugn8_colors, sizeof(bugn8_colors) / sizeof(bugn8_colors[0])};

static const color_t bugn9_colors[] = {
    {"1", {247, 252, 253, 255}}, {"2", {229, 245, 249, 255}},
    {"3", {204, 236, 230, 255}}, {"4", {153, 216, 201, 255}},
    {"5", {102, 194, 164, 255}}, {"6", {65, 174, 118, 255}},
    {"7", {35, 139, 69, 255}},   {"8", {0, 109, 44, 255}},
    {"9", {0, 68, 27, 255}},
};
static const colorscheme_t bugn9 = {
    "bugn9", bugn9_colors, sizeof(bugn9_colors) / sizeof(bugn9_colors[0])};

static const color_t bupu3_colors[] = {
    {"1", {224, 236, 244, 255}},
    {"2", {158, 188, 218, 255}},
    {"3", {136, 86, 167, 255}},
};
static const colorscheme_t bupu3 = {
    "bupu3", bupu3_colors, sizeof(bupu3_colors) / sizeof(bupu3_colors[0])};

static const color_t bupu4_colors[] = {
    {"1", {237, 248, 251, 255}},
    {"2", {179, 205, 227, 255}},
    {"3", {140, 150, 198, 255}},
    {"4", {136, 65, 157, 255}},
};
static const colorscheme_t bupu4 = {
    "bupu4", bupu4_colors, sizeof(bupu4_colors) / sizeof(bupu4_colors[0])};

static const color_t bupu5_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {140, 150, 198, 255}}, {"4", {136, 86, 167, 255}},
    {"5", {129, 15, 124, 255}},
};
static const colorscheme_t bupu5 = {
    "bupu5", bupu5_colors, sizeof(bupu5_colors) / sizeof(bupu5_colors[0])};

static const color_t bupu6_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {191, 211, 230, 255}},
    {"3", {158, 188, 218, 255}}, {"4", {140, 150, 198, 255}},
    {"5", {136, 86, 167, 255}},  {"6", {129, 15, 124, 255}},
};
static const colorscheme_t bupu6 = {
    "bupu6", bupu6_colors, sizeof(bupu6_colors) / sizeof(bupu6_colors[0])};

static const color_t bupu7_colors[] = {
    {"1", {237, 248, 251, 255}}, {"2", {191, 211, 230, 255}},
    {"3", {158, 188, 218, 255}}, {"4", {140, 150, 198, 255}},
    {"5", {140, 107, 177, 255}}, {"6", {136, 65, 157, 255}},
    {"7", {110, 1, 107, 255}},
};
static const colorscheme_t bupu7 = {
    "bupu7", bupu7_colors, sizeof(bupu7_colors) / sizeof(bupu7_colors[0])};

static const color_t bupu8_colors[] = {
    {"1", {247, 252, 253, 255}}, {"2", {224, 236, 244, 255}},
    {"3", {191, 211, 230, 255}}, {"4", {158, 188, 218, 255}},
    {"5", {140, 150, 198, 255}}, {"6", {140, 107, 177, 255}},
    {"7", {136, 65, 157, 255}},  {"8", {110, 1, 107, 255}},
};
static const colorscheme_t bupu8 = {
    "bupu8", bupu8_colors, sizeof(bupu8_colors) / sizeof(bupu8_colors[0])};

static const color_t bupu9_colors[] = {
    {"1", {247, 252, 253, 255}}, {"2", {224, 236, 244, 255}},
    {"3", {191, 211, 230, 255}}, {"4", {158, 188, 218, 255}},
    {"5", {140, 150, 198, 255}}, {"6", {140, 107, 177, 255}},
    {"7", {136, 65, 157, 255}},  {"8", {129, 15, 124, 255}},
    {"9", {77, 0, 75, 255}},
};
static const colorscheme_t bupu9 = {
    "bupu9", bupu9_colors, sizeof(bupu9_colors) / sizeof(bupu9_colors[0])};

static const color_t dark23_colors[] = {
    {"1", {27, 158, 119, 255}},
    {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}},
};
static const colorscheme_t dark23 = {
    "dark23", dark23_colors, sizeof(dark23_colors) / sizeof(dark23_colors[0])};

static const color_t dark24_colors[] = {
    {"1", {27, 158, 119, 255}},
    {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}},
    {"4", {231, 41, 138, 255}},
};
static const colorscheme_t dark24 = {
    "dark24", dark24_colors, sizeof(dark24_colors) / sizeof(dark24_colors[0])};

static const color_t dark25_colors[] = {
    {"1", {27, 158, 119, 255}},  {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}}, {"4", {231, 41, 138, 255}},
    {"5", {102, 166, 30, 255}},
};
static const colorscheme_t dark25 = {
    "dark25", dark25_colors, sizeof(dark25_colors) / sizeof(dark25_colors[0])};

static const color_t dark26_colors[] = {
    {"1", {27, 158, 119, 255}},  {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}}, {"4", {231, 41, 138, 255}},
    {"5", {102, 166, 30, 255}},  {"6", {230, 171, 2, 255}},
};
static const colorscheme_t dark26 = {
    "dark26", dark26_colors, sizeof(dark26_colors) / sizeof(dark26_colors[0])};

static const color_t dark27_colors[] = {
    {"1", {27, 158, 119, 255}},  {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}}, {"4", {231, 41, 138, 255}},
    {"5", {102, 166, 30, 255}},  {"6", {230, 171, 2, 255}},
    {"7", {166, 118, 29, 255}},
};
static const colorscheme_t dark27 = {
    "dark27", dark27_colors, sizeof(dark27_colors) / sizeof(dark27_colors[0])};

static const color_t dark28_colors[] = {
    {"1", {27, 158, 119, 255}},  {"2", {217, 95, 2, 255}},
    {"3", {117, 112, 179, 255}}, {"4", {231, 41, 138, 255}},
    {"5", {102, 166, 30, 255}},  {"6", {230, 171, 2, 255}},
    {"7", {166, 118, 29, 255}},  {"8", {102, 102, 102, 255}},
};
static const colorscheme_t dark28 = {
    "dark28", dark28_colors, sizeof(dark28_colors) / sizeof(dark28_colors[0])};

static const color_t gnbu3_colors[] = {
    {"1", {224, 243, 219, 255}},
    {"2", {168, 221, 181, 255}},
    {"3", {67, 162, 202, 255}},
};
static const colorscheme_t gnbu3 = {
    "gnbu3", gnbu3_colors, sizeof(gnbu3_colors) / sizeof(gnbu3_colors[0])};

static const color_t gnbu4_colors[] = {
    {"1", {240, 249, 232, 255}},
    {"2", {186, 228, 188, 255}},
    {"3", {123, 204, 196, 255}},
    {"4", {43, 140, 190, 255}},
};
static const colorscheme_t gnbu4 = {
    "gnbu4", gnbu4_colors, sizeof(gnbu4_colors) / sizeof(gnbu4_colors[0])};

static const color_t gnbu5_colors[] = {
    {"1", {240, 249, 232, 255}}, {"2", {186, 228, 188, 255}},
    {"3", {123, 204, 196, 255}}, {"4", {67, 162, 202, 255}},
    {"5", {8, 104, 172, 255}},
};
static const colorscheme_t gnbu5 = {
    "gnbu5", gnbu5_colors, sizeof(gnbu5_colors) / sizeof(gnbu5_colors[0])};

static const color_t gnbu6_colors[] = {
    {"1", {240, 249, 232, 255}}, {"2", {204, 235, 197, 255}},
    {"3", {168, 221, 181, 255}}, {"4", {123, 204, 196, 255}},
    {"5", {67, 162, 202, 255}},  {"6", {8, 104, 172, 255}},
};
static const colorscheme_t gnbu6 = {
    "gnbu6", gnbu6_colors, sizeof(gnbu6_colors) / sizeof(gnbu6_colors[0])};

static const color_t gnbu7_colors[] = {
    {"1", {240, 249, 232, 255}}, {"2", {204, 235, 197, 255}},
    {"3", {168, 221, 181, 255}}, {"4", {123, 204, 196, 255}},
    {"5", {78, 179, 211, 255}},  {"6", {43, 140, 190, 255}},
    {"7", {8, 88, 158, 255}},
};
static const colorscheme_t gnbu7 = {
    "gnbu7", gnbu7_colors, sizeof(gnbu7_colors) / sizeof(gnbu7_colors[0])};

static const color_t gnbu8_colors[] = {
    {"1", {247, 252, 240, 255}}, {"2", {224, 243, 219, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {168, 221, 181, 255}},
    {"5", {123, 204, 196, 255}}, {"6", {78, 179, 211, 255}},
    {"7", {43, 140, 190, 255}},  {"8", {8, 88, 158, 255}},
};
static const colorscheme_t gnbu8 = {
    "gnbu8", gnbu8_colors, sizeof(gnbu8_colors) / sizeof(gnbu8_colors[0])};

static const color_t gnbu9_colors[] = {
    {"1", {247, 252, 240, 255}}, {"2", {224, 243, 219, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {168, 221, 181, 255}},
    {"5", {123, 204, 196, 255}}, {"6", {78, 179, 211, 255}},
    {"7", {43, 140, 190, 255}},  {"8", {8, 104, 172, 255}},
    {"9", {8, 64, 129, 255}},
};
static const colorscheme_t gnbu9 = {
    "gnbu9", gnbu9_colors, sizeof(gnbu9_colors) / sizeof(gnbu9_colors[0])};

static const color_t greens3_colors[] = {
    {"1", {229, 245, 224, 255}},
    {"2", {161, 217, 155, 255}},
    {"3", {49, 163, 84, 255}},
};
static const colorscheme_t greens3 = {"greens3", greens3_colors,
                                      sizeof(greens3_colors) /
                                          sizeof(greens3_colors[0])};

static const color_t greens4_colors[] = {
    {"1", {237, 248, 233, 255}},
    {"2", {186, 228, 179, 255}},
    {"3", {116, 196, 118, 255}},
    {"4", {35, 139, 69, 255}},
};
static const colorscheme_t greens4 = {"greens4", greens4_colors,
                                      sizeof(greens4_colors) /
                                          sizeof(greens4_colors[0])};

static const color_t greens5_colors[] = {
    {"1", {237, 248, 233, 255}}, {"2", {186, 228, 179, 255}},
    {"3", {116, 196, 118, 255}}, {"4", {49, 163, 84, 255}},
    {"5", {0, 109, 44, 255}},
};
static const colorscheme_t greens5 = {"greens5", greens5_colors,
                                      sizeof(greens5_colors) /
                                          sizeof(greens5_colors[0])};

static const color_t greens6_colors[] = {
    {"1", {237, 248, 233, 255}}, {"2", {199, 233, 192, 255}},
    {"3", {161, 217, 155, 255}}, {"4", {116, 196, 118, 255}},
    {"5", {49, 163, 84, 255}},   {"6", {0, 109, 44, 255}},
};
static const colorscheme_t greens6 = {"greens6", greens6_colors,
                                      sizeof(greens6_colors) /
                                          sizeof(greens6_colors[0])};

static const color_t greens7_colors[] = {
    {"1", {237, 248, 233, 255}}, {"2", {199, 233, 192, 255}},
    {"3", {161, 217, 155, 255}}, {"4", {116, 196, 118, 255}},
    {"5", {65, 171, 93, 255}},   {"6", {35, 139, 69, 255}},
    {"7", {0, 90, 50, 255}},
};
static const colorscheme_t greens7 = {"greens7", greens7_colors,
                                      sizeof(greens7_colors) /
                                          sizeof(greens7_colors[0])};

static const color_t greens8_colors[] = {
    {"1", {247, 252, 245, 255}}, {"2", {229, 245, 224, 255}},
    {"3", {199, 233, 192, 255}}, {"4", {161, 217, 155, 255}},
    {"5", {116, 196, 118, 255}}, {"6", {65, 171, 93, 255}},
    {"7", {35, 139, 69, 255}},   {"8", {0, 90, 50, 255}},
};
static const colorscheme_t greens8 = {"greens8", greens8_colors,
                                      sizeof(greens8_colors) /
                                          sizeof(greens8_colors[0])};

static const color_t greens9_colors[] = {
    {"1", {247, 252, 245, 255}}, {"2", {229, 245, 224, 255}},
    {"3", {199, 233, 192, 255}}, {"4", {161, 217, 155, 255}},
    {"5", {116, 196, 118, 255}}, {"6", {65, 171, 93, 255}},
    {"7", {35, 139, 69, 255}},   {"8", {0, 109, 44, 255}},
    {"9", {0, 68, 27, 255}},
};
static const colorscheme_t greens9 = {"greens9", greens9_colors,
                                      sizeof(greens9_colors) /
                                          sizeof(greens9_colors[0])};

static const color_t greys3_colors[] = {
    {"1", {240, 240, 240, 255}},
    {"2", {189, 189, 189, 255}},
    {"3", {99, 99, 99, 255}},
};
static const colorscheme_t greys3 = {
    "greys3", greys3_colors, sizeof(greys3_colors) / sizeof(greys3_colors[0])};

static const color_t greys4_colors[] = {
    {"1", {247, 247, 247, 255}},
    {"2", {204, 204, 204, 255}},
    {"3", {150, 150, 150, 255}},
    {"4", {82, 82, 82, 255}},
};
static const colorscheme_t greys4 = {
    "greys4", greys4_colors, sizeof(greys4_colors) / sizeof(greys4_colors[0])};

static const color_t greys5_colors[] = {
    {"1", {247, 247, 247, 255}}, {"2", {204, 204, 204, 255}},
    {"3", {150, 150, 150, 255}}, {"4", {99, 99, 99, 255}},
    {"5", {37, 37, 37, 255}},
};
static const colorscheme_t greys5 = {
    "greys5", greys5_colors, sizeof(greys5_colors) / sizeof(greys5_colors[0])};

static const color_t greys6_colors[] = {
    {"1", {247, 247, 247, 255}}, {"2", {217, 217, 217, 255}},
    {"3", {189, 189, 189, 255}}, {"4", {150, 150, 150, 255}},
    {"5", {99, 99, 99, 255}},    {"6", {37, 37, 37, 255}},
};
static const colorscheme_t greys6 = {
    "greys6", greys6_colors, sizeof(greys6_colors) / sizeof(greys6_colors[0])};

static const color_t greys7_colors[] = {
    {"1", {247, 247, 247, 255}}, {"2", {217, 217, 217, 255}},
    {"3", {189, 189, 189, 255}}, {"4", {150, 150, 150, 255}},
    {"5", {115, 115, 115, 255}}, {"6", {82, 82, 82, 255}},
    {"7", {37, 37, 37, 255}},
};
static const colorscheme_t greys7 = {
    "greys7", greys7_colors, sizeof(greys7_colors) / sizeof(greys7_colors[0])};

static const color_t greys8_colors[] = {
    {"1", {255, 255, 255, 255}}, {"2", {240, 240, 240, 255}},
    {"3", {217, 217, 217, 255}}, {"4", {189, 189, 189, 255}},
    {"5", {150, 150, 150, 255}}, {"6", {115, 115, 115, 255}},
    {"7", {82, 82, 82, 255}},    {"8", {37, 37, 37, 255}},
};
static const colorscheme_t greys8 = {
    "greys8", greys8_colors, sizeof(greys8_colors) / sizeof(greys8_colors[0])};

static const color_t greys9_colors[] = {
    {"1", {255, 255, 255, 255}}, {"2", {240, 240, 240, 255}},
    {"3", {217, 217, 217, 255}}, {"4", {189, 189, 189, 255}},
    {"5", {150, 150, 150, 255}}, {"6", {115, 115, 115, 255}},
    {"7", {82, 82, 82, 255}},    {"8", {37, 37, 37, 255}},
    {"9", {0, 0, 0, 255}},
};
static const colorscheme_t greys9 = {
    "greys9", greys9_colors, sizeof(greys9_colors) / sizeof(greys9_colors[0])};

static const color_t oranges3_colors[] = {
    {"1", {254, 230, 206, 255}},
    {"2", {253, 174, 107, 255}},
    {"3", {230, 85, 13, 255}},
};
static const colorscheme_t oranges3 = {"oranges3", oranges3_colors,
                                       sizeof(oranges3_colors) /
                                           sizeof(oranges3_colors[0])};

static const color_t oranges4_colors[] = {
    {"1", {254, 237, 222, 255}},
    {"2", {253, 190, 133, 255}},
    {"3", {253, 141, 60, 255}},
    {"4", {217, 71, 1, 255}},
};
static const colorscheme_t oranges4 = {"oranges4", oranges4_colors,
                                       sizeof(oranges4_colors) /
                                           sizeof(oranges4_colors[0])};

static const color_t oranges5_colors[] = {
    {"1", {254, 237, 222, 255}}, {"2", {253, 190, 133, 255}},
    {"3", {253, 141, 60, 255}},  {"4", {230, 85, 13, 255}},
    {"5", {166, 54, 3, 255}},
};
static const colorscheme_t oranges5 = {"oranges5", oranges5_colors,
                                       sizeof(oranges5_colors) /
                                           sizeof(oranges5_colors[0])};

static const color_t oranges6_colors[] = {
    {"1", {254, 237, 222, 255}}, {"2", {253, 208, 162, 255}},
    {"3", {253, 174, 107, 255}}, {"4", {253, 141, 60, 255}},
    {"5", {230, 85, 13, 255}},   {"6", {166, 54, 3, 255}},
};
static const colorscheme_t oranges6 = {"oranges6", oranges6_colors,
                                       sizeof(oranges6_colors) /
                                           sizeof(oranges6_colors[0])};

static const color_t oranges7_colors[] = {
    {"1", {254, 237, 222, 255}}, {"2", {253, 208, 162, 255}},
    {"3", {253, 174, 107, 255}}, {"4", {253, 141, 60, 255}},
    {"5", {241, 105, 19, 255}},  {"6", {217, 72, 1, 255}},
    {"7", {140, 45, 4, 255}},
};
static const colorscheme_t oranges7 = {"oranges7", oranges7_colors,
                                       sizeof(oranges7_colors) /
                                           sizeof(oranges7_colors[0])};

static const color_t oranges8_colors[] = {
    {"1", {255, 245, 235, 255}}, {"2", {254, 230, 206, 255}},
    {"3", {253, 208, 162, 255}}, {"4", {253, 174, 107, 255}},
    {"5", {253, 141, 60, 255}},  {"6", {241, 105, 19, 255}},
    {"7", {217, 72, 1, 255}},    {"8", {140, 45, 4, 255}},
};
static const colorscheme_t oranges8 = {"oranges8", oranges8_colors,
                                       sizeof(oranges8_colors) /
                                           sizeof(oranges8_colors[0])};

static const color_t oranges9_colors[] = {
    {"1", {255, 245, 235, 255}}, {"2", {254, 230, 206, 255}},
    {"3", {253, 208, 162, 255}}, {"4", {253, 174, 107, 255}},
    {"5", {253, 141, 60, 255}},  {"6", {241, 105, 19, 255}},
    {"7", {217, 72, 1, 255}},    {"8", {166, 54, 3, 255}},
    {"9", {127, 39, 4, 255}},
};
static const colorscheme_t oranges9 = {"oranges9", oranges9_colors,
                                       sizeof(oranges9_colors) /
                                           sizeof(oranges9_colors[0])};

static const color_t orrd3_colors[] = {
    {"1", {254, 232, 200, 255}},
    {"2", {253, 187, 132, 255}},
    {"3", {227, 74, 51, 255}},
};
static const colorscheme_t orrd3 = {
    "orrd3", orrd3_colors, sizeof(orrd3_colors) / sizeof(orrd3_colors[0])};

static const color_t orrd4_colors[] = {
    {"1", {254, 240, 217, 255}},
    {"2", {253, 204, 138, 255}},
    {"3", {252, 141, 89, 255}},
    {"4", {215, 48, 31, 255}},
};
static const colorscheme_t orrd4 = {
    "orrd4", orrd4_colors, sizeof(orrd4_colors) / sizeof(orrd4_colors[0])};

static const color_t orrd5_colors[] = {
    {"1", {254, 240, 217, 255}}, {"2", {253, 204, 138, 255}},
    {"3", {252, 141, 89, 255}},  {"4", {227, 74, 51, 255}},
    {"5", {179, 0, 0, 255}},
};
static const colorscheme_t orrd5 = {
    "orrd5", orrd5_colors, sizeof(orrd5_colors) / sizeof(orrd5_colors[0])};

static const color_t orrd6_colors[] = {
    {"1", {254, 240, 217, 255}}, {"2", {253, 212, 158, 255}},
    {"3", {253, 187, 132, 255}}, {"4", {252, 141, 89, 255}},
    {"5", {227, 74, 51, 255}},   {"6", {179, 0, 0, 255}},
};
static const colorscheme_t orrd6 = {
    "orrd6", orrd6_colors, sizeof(orrd6_colors) / sizeof(orrd6_colors[0])};

static const color_t orrd7_colors[] = {
    {"1", {254, 240, 217, 255}}, {"2", {253, 212, 158, 255}},
    {"3", {253, 187, 132, 255}}, {"4", {252, 141, 89, 255}},
    {"5", {239, 101, 72, 255}},  {"6", {215, 48, 31, 255}},
    {"7", {153, 0, 0, 255}},
};
static const colorscheme_t orrd7 = {
    "orrd7", orrd7_colors, sizeof(orrd7_colors) / sizeof(orrd7_colors[0])};

static const color_t orrd8_colors[] = {
    {"1", {255, 247, 236, 255}}, {"2", {254, 232, 200, 255}},
    {"3", {253, 212, 158, 255}}, {"4", {253, 187, 132, 255}},
    {"5", {252, 141, 89, 255}},  {"6", {239, 101, 72, 255}},
    {"7", {215, 48, 31, 255}},   {"8", {153, 0, 0, 255}},
};
static const colorscheme_t orrd8 = {
    "orrd8", orrd8_colors, sizeof(orrd8_colors) / sizeof(orrd8_colors[0])};

static const color_t orrd9_colors[] = {
    {"1", {255, 247, 236, 255}}, {"2", {254, 232, 200, 255}},
    {"3", {253, 212, 158, 255}}, {"4", {253, 187, 132, 255}},
    {"5", {252, 141, 89, 255}},  {"6", {239, 101, 72, 255}},
    {"7", {215, 48, 31, 255}},   {"8", {179, 0, 0, 255}},
    {"9", {127, 0, 0, 255}},
};
static const colorscheme_t orrd9 = {
    "orrd9", orrd9_colors, sizeof(orrd9_colors) / sizeof(orrd9_colors[0])};

static const color_t paired10_colors[] = {
    {"1", {166, 206, 227, 255}}, {"10", {106, 61, 154, 255}},
    {"2", {31, 120, 180, 255}},  {"3", {178, 223, 138, 255}},
    {"4", {51, 160, 44, 255}},   {"5", {251, 154, 153, 255}},
    {"6", {227, 26, 28, 255}},   {"7", {253, 191, 111, 255}},
    {"8", {255, 127, 0, 255}},   {"9", {202, 178, 214, 255}},
};
static const colorscheme_t paired10 = {"paired10", paired10_colors,
                                       sizeof(paired10_colors) /
                                           sizeof(paired10_colors[0])};

static const color_t paired11_colors[] = {
    {"1", {166, 206, 227, 255}},  {"10", {106, 61, 154, 255}},
    {"11", {255, 255, 153, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}},  {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}},  {"6", {227, 26, 28, 255}},
    {"7", {253, 191, 111, 255}},  {"8", {255, 127, 0, 255}},
    {"9", {202, 178, 214, 255}},
};
static const colorscheme_t paired11 = {"paired11", paired11_colors,
                                       sizeof(paired11_colors) /
                                           sizeof(paired11_colors[0])};

static const color_t paired12_colors[] = {
    {"1", {166, 206, 227, 255}},  {"10", {106, 61, 154, 255}},
    {"11", {255, 255, 153, 255}}, {"12", {177, 89, 40, 255}},
    {"2", {31, 120, 180, 255}},   {"3", {178, 223, 138, 255}},
    {"4", {51, 160, 44, 255}},    {"5", {251, 154, 153, 255}},
    {"6", {227, 26, 28, 255}},    {"7", {253, 191, 111, 255}},
    {"8", {255, 127, 0, 255}},    {"9", {202, 178, 214, 255}},
};
static const colorscheme_t paired12 = {"paired12", paired12_colors,
                                       sizeof(paired12_colors) /
                                           sizeof(paired12_colors[0])};

static const color_t paired3_colors[] = {
    {"1", {166, 206, 227, 255}},
    {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}},
};
static const colorscheme_t paired3 = {"paired3", paired3_colors,
                                      sizeof(paired3_colors) /
                                          sizeof(paired3_colors[0])};

static const color_t paired4_colors[] = {
    {"1", {166, 206, 227, 255}},
    {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}},
    {"4", {51, 160, 44, 255}},
};
static const colorscheme_t paired4 = {"paired4", paired4_colors,
                                      sizeof(paired4_colors) /
                                          sizeof(paired4_colors[0])};

static const color_t paired5_colors[] = {
    {"1", {166, 206, 227, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}}, {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}},
};
static const colorscheme_t paired5 = {"paired5", paired5_colors,
                                      sizeof(paired5_colors) /
                                          sizeof(paired5_colors[0])};

static const color_t paired6_colors[] = {
    {"1", {166, 206, 227, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}}, {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}}, {"6", {227, 26, 28, 255}},
};
static const colorscheme_t paired6 = {"paired6", paired6_colors,
                                      sizeof(paired6_colors) /
                                          sizeof(paired6_colors[0])};

static const color_t paired7_colors[] = {
    {"1", {166, 206, 227, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}}, {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}}, {"6", {227, 26, 28, 255}},
    {"7", {253, 191, 111, 255}},
};
static const colorscheme_t paired7 = {"paired7", paired7_colors,
                                      sizeof(paired7_colors) /
                                          sizeof(paired7_colors[0])};

static const color_t paired8_colors[] = {
    {"1", {166, 206, 227, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}}, {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}}, {"6", {227, 26, 28, 255}},
    {"7", {253, 191, 111, 255}}, {"8", {255, 127, 0, 255}},
};
static const colorscheme_t paired8 = {"paired8", paired8_colors,
                                      sizeof(paired8_colors) /
                                          sizeof(paired8_colors[0])};

static const color_t paired9_colors[] = {
    {"1", {166, 206, 227, 255}}, {"2", {31, 120, 180, 255}},
    {"3", {178, 223, 138, 255}}, {"4", {51, 160, 44, 255}},
    {"5", {251, 154, 153, 255}}, {"6", {227, 26, 28, 255}},
    {"7", {253, 191, 111, 255}}, {"8", {255, 127, 0, 255}},
    {"9", {202, 178, 214, 255}},
};
static const colorscheme_t paired9 = {"paired9", paired9_colors,
                                      sizeof(paired9_colors) /
                                          sizeof(paired9_colors[0])};

static const color_t pastel13_colors[] = {
    {"1", {251, 180, 174, 255}},
    {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}},
};
static const colorscheme_t pastel13 = {"pastel13", pastel13_colors,
                                       sizeof(pastel13_colors) /
                                           sizeof(pastel13_colors[0])};

static const color_t pastel14_colors[] = {
    {"1", {251, 180, 174, 255}},
    {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}},
    {"4", {222, 203, 228, 255}},
};
static const colorscheme_t pastel14 = {"pastel14", pastel14_colors,
                                       sizeof(pastel14_colors) /
                                           sizeof(pastel14_colors[0])};

static const color_t pastel15_colors[] = {
    {"1", {251, 180, 174, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {222, 203, 228, 255}},
    {"5", {254, 217, 166, 255}},
};
static const colorscheme_t pastel15 = {"pastel15", pastel15_colors,
                                       sizeof(pastel15_colors) /
                                           sizeof(pastel15_colors[0])};

static const color_t pastel16_colors[] = {
    {"1", {251, 180, 174, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {222, 203, 228, 255}},
    {"5", {254, 217, 166, 255}}, {"6", {255, 255, 204, 255}},
};
static const colorscheme_t pastel16 = {"pastel16", pastel16_colors,
                                       sizeof(pastel16_colors) /
                                           sizeof(pastel16_colors[0])};

static const color_t pastel17_colors[] = {
    {"1", {251, 180, 174, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {222, 203, 228, 255}},
    {"5", {254, 217, 166, 255}}, {"6", {255, 255, 204, 255}},
    {"7", {229, 216, 189, 255}},
};
static const colorscheme_t pastel17 = {"pastel17", pastel17_colors,
                                       sizeof(pastel17_colors) /
                                           sizeof(pastel17_colors[0])};

static const color_t pastel18_colors[] = {
    {"1", {251, 180, 174, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {222, 203, 228, 255}},
    {"5", {254, 217, 166, 255}}, {"6", {255, 255, 204, 255}},
    {"7", {229, 216, 189, 255}}, {"8", {253, 218, 236, 255}},
};
static const colorscheme_t pastel18 = {"pastel18", pastel18_colors,
                                       sizeof(pastel18_colors) /
                                           sizeof(pastel18_colors[0])};

static const color_t pastel19_colors[] = {
    {"1", {251, 180, 174, 255}}, {"2", {179, 205, 227, 255}},
    {"3", {204, 235, 197, 255}}, {"4", {222, 203, 228, 255}},
    {"5", {254, 217, 166, 255}}, {"6", {255, 255, 204, 255}},
    {"7", {229, 216, 189, 255}}, {"8", {253, 218, 236, 255}},
    {"9", {242, 242, 242, 255}},
};
static const colorscheme_t pastel19 = {"pastel19", pastel19_colors,
                                       sizeof(pastel19_colors) /
                                           sizeof(pastel19_colors[0])};

static const color_t pastel23_colors[] = {
    {"1", {179, 226, 205, 255}},
    {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}},
};
static const colorscheme_t pastel23 = {"pastel23", pastel23_colors,
                                       sizeof(pastel23_colors) /
                                           sizeof(pastel23_colors[0])};

static const color_t pastel24_colors[] = {
    {"1", {179, 226, 205, 255}},
    {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}},
    {"4", {244, 202, 228, 255}},
};
static const colorscheme_t pastel24 = {"pastel24", pastel24_colors,
                                       sizeof(pastel24_colors) /
                                           sizeof(pastel24_colors[0])};

static const color_t pastel25_colors[] = {
    {"1", {179, 226, 205, 255}}, {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}}, {"4", {244, 202, 228, 255}},
    {"5", {230, 245, 201, 255}},
};
static const colorscheme_t pastel25 = {"pastel25", pastel25_colors,
                                       sizeof(pastel25_colors) /
                                           sizeof(pastel25_colors[0])};

static const color_t pastel26_colors[] = {
    {"1", {179, 226, 205, 255}}, {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}}, {"4", {244, 202, 228, 255}},
    {"5", {230, 245, 201, 255}}, {"6", {255, 242, 174, 255}},
};
static const colorscheme_t pastel26 = {"pastel26", pastel26_colors,
                                       sizeof(pastel26_colors) /
                                           sizeof(pastel26_colors[0])};

static const color_t pastel27_colors[] = {
    {"1", {179, 226, 205, 255}}, {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}}, {"4", {244, 202, 228, 255}},
    {"5", {230, 245, 201, 255}}, {"6", {255, 242, 174, 255}},
    {"7", {241, 226, 204, 255}},
};
static const colorscheme_t pastel27 = {"pastel27", pastel27_colors,
                                       sizeof(pastel27_colors) /
                                           sizeof(pastel27_colors[0])};

static const color_t pastel28_colors[] = {
    {"1", {179, 226, 205, 255}}, {"2", {253, 205, 172, 255}},
    {"3", {203, 213, 232, 255}}, {"4", {244, 202, 228, 255}},
    {"5", {230, 245, 201, 255}}, {"6", {255, 242, 174, 255}},
    {"7", {241, 226, 204, 255}}, {"8", {204, 204, 204, 255}},
};
static const colorscheme_t pastel28 = {"pastel28", pastel28_colors,
                                       sizeof(pastel28_colors) /
                                           sizeof(pastel28_colors[0])};

static const color_t piyg10_colors[] = {
    {"1", {142, 1, 82, 255}},    {"10", {39, 100, 25, 255}},
    {"2", {197, 27, 125, 255}},  {"3", {222, 119, 174, 255}},
    {"4", {241, 182, 218, 255}}, {"5", {253, 224, 239, 255}},
    {"6", {230, 245, 208, 255}}, {"7", {184, 225, 134, 255}},
    {"8", {127, 188, 65, 255}},  {"9", {77, 146, 33, 255}},
};
static const colorscheme_t piyg10 = {
    "piyg10", piyg10_colors, sizeof(piyg10_colors) / sizeof(piyg10_colors[0])};

static const color_t piyg11_colors[] = {
    {"1", {142, 1, 82, 255}},    {"10", {77, 146, 33, 255}},
    {"11", {39, 100, 25, 255}},  {"2", {197, 27, 125, 255}},
    {"3", {222, 119, 174, 255}}, {"4", {241, 182, 218, 255}},
    {"5", {253, 224, 239, 255}}, {"6", {247, 247, 247, 255}},
    {"7", {230, 245, 208, 255}}, {"8", {184, 225, 134, 255}},
    {"9", {127, 188, 65, 255}},
};
static const colorscheme_t piyg11 = {
    "piyg11", piyg11_colors, sizeof(piyg11_colors) / sizeof(piyg11_colors[0])};

static const color_t piyg3_colors[] = {
    {"1", {233, 163, 201, 255}},
    {"2", {247, 247, 247, 255}},
    {"3", {161, 215, 106, 255}},
};
static const colorscheme_t piyg3 = {
    "piyg3", piyg3_colors, sizeof(piyg3_colors) / sizeof(piyg3_colors[0])};

static const color_t piyg4_colors[] = {
    {"1", {208, 28, 139, 255}},
    {"2", {241, 182, 218, 255}},
    {"3", {184, 225, 134, 255}},
    {"4", {77, 172, 38, 255}},
};
static const colorscheme_t piyg4 = {
    "piyg4", piyg4_colors, sizeof(piyg4_colors) / sizeof(piyg4_colors[0])};

static const color_t piyg5_colors[] = {
    {"1", {208, 28, 139, 255}},  {"2", {241, 182, 218, 255}},
    {"3", {247, 247, 247, 255}}, {"4", {184, 225, 134, 255}},
    {"5", {77, 172, 38, 255}},
};
static const colorscheme_t piyg5 = {
    "piyg5", piyg5_colors, sizeof(piyg5_colors) / sizeof(piyg5_colors[0])};

static const color_t piyg6_colors[] = {
    {"1", {197, 27, 125, 255}},  {"2", {233, 163, 201, 255}},
    {"3", {253, 224, 239, 255}}, {"4", {230, 245, 208, 255}},
    {"5", {161, 215, 106, 255}}, {"6", {77, 146, 33, 255}},
};
static const colorscheme_t piyg6 = {
    "piyg6", piyg6_colors, sizeof(piyg6_colors) / sizeof(piyg6_colors[0])};

static const color_t piyg7_colors[] = {
    {"1", {197, 27, 125, 255}},  {"2", {233, 163, 201, 255}},
    {"3", {253, 224, 239, 255}}, {"4", {247, 247, 247, 255}},
    {"5", {230, 245, 208, 255}}, {"6", {161, 215, 106, 255}},
    {"7", {77, 146, 33, 255}},
};
static const colorscheme_t piyg7 = {
    "piyg7", piyg7_colors, sizeof(piyg7_colors) / sizeof(piyg7_colors[0])};

static const color_t piyg8_colors[] = {
    {"1", {197, 27, 125, 255}},  {"2", {222, 119, 174, 255}},
    {"3", {241, 182, 218, 255}}, {"4", {253, 224, 239, 255}},
    {"5", {230, 245, 208, 255}}, {"6", {184, 225, 134, 255}},
    {"7", {127, 188, 65, 255}},  {"8", {77, 146, 33, 255}},
};
static const colorscheme_t piyg8 = {
    "piyg8", piyg8_colors, sizeof(piyg8_colors) / sizeof(piyg8_colors[0])};

static const color_t piyg9_colors[] = {
    {"1", {197, 27, 125, 255}},  {"2", {222, 119, 174, 255}},
    {"3", {241, 182, 218, 255}}, {"4", {253, 224, 239, 255}},
    {"5", {247, 247, 247, 255}}, {"6", {230, 245, 208, 255}},
    {"7", {184, 225, 134, 255}}, {"8", {127, 188, 65, 255}},
    {"9", {77, 146, 33, 255}},
};
static const colorscheme_t piyg9 = {
    "piyg9", piyg9_colors, sizeof(piyg9_colors) / sizeof(piyg9_colors[0])};

static const color_t prgn10_colors[] = {
    {"1", {64, 0, 75, 255}},     {"10", {0, 68, 27, 255}},
    {"2", {118, 42, 131, 255}},  {"3", {153, 112, 171, 255}},
    {"4", {194, 165, 207, 255}}, {"5", {231, 212, 232, 255}},
    {"6", {217, 240, 211, 255}}, {"7", {166, 219, 160, 255}},
    {"8", {90, 174, 97, 255}},   {"9", {27, 120, 55, 255}},
};
static const colorscheme_t prgn10 = {
    "prgn10", prgn10_colors, sizeof(prgn10_colors) / sizeof(prgn10_colors[0])};

static const color_t prgn11_colors[] = {
    {"1", {64, 0, 75, 255}},     {"10", {27, 120, 55, 255}},
    {"11", {0, 68, 27, 255}},    {"2", {118, 42, 131, 255}},
    {"3", {153, 112, 171, 255}}, {"4", {194, 165, 207, 255}},
    {"5", {231, 212, 232, 255}}, {"6", {247, 247, 247, 255}},
    {"7", {217, 240, 211, 255}}, {"8", {166, 219, 160, 255}},
    {"9", {90, 174, 97, 255}},
};
static const colorscheme_t prgn11 = {
    "prgn11", prgn11_colors, sizeof(prgn11_colors) / sizeof(prgn11_colors[0])};

static const color_t prgn3_colors[] = {
    {"1", {175, 141, 195, 255}},
    {"2", {247, 247, 247, 255}},
    {"3", {127, 191, 123, 255}},
};
static const colorscheme_t prgn3 = {
    "prgn3", prgn3_colors, sizeof(prgn3_colors) / sizeof(prgn3_colors[0])};

static const color_t prgn4_colors[] = {
    {"1", {123, 50, 148, 255}},
    {"2", {194, 165, 207, 255}},
    {"3", {166, 219, 160, 255}},
    {"4", {0, 136, 55, 255}},
};
static const colorscheme_t prgn4 = {
    "prgn4", prgn4_colors, sizeof(prgn4_colors) / sizeof(prgn4_colors[0])};

static const color_t prgn5_colors[] = {
    {"1", {123, 50, 148, 255}},  {"2", {194, 165, 207, 255}},
    {"3", {247, 247, 247, 255}}, {"4", {166, 219, 160, 255}},
    {"5", {0, 136, 55, 255}},
};
static const colorscheme_t prgn5 = {
    "prgn5", prgn5_colors, sizeof(prgn5_colors) / sizeof(prgn5_colors[0])};

static const color_t prgn6_colors[] = {
    {"1", {118, 42, 131, 255}},  {"2", {175, 141, 195, 255}},
    {"3", {231, 212, 232, 255}}, {"4", {217, 240, 211, 255}},
    {"5", {127, 191, 123, 255}}, {"6", {27, 120, 55, 255}},
};
static const colorscheme_t prgn6 = {
    "prgn6", prgn6_colors, sizeof(prgn6_colors) / sizeof(prgn6_colors[0])};

static const color_t prgn7_colors[] = {
    {"1", {118, 42, 131, 255}},  {"2", {175, 141, 195, 255}},
    {"3", {231, 212, 232, 255}}, {"4", {247, 247, 247, 255}},
    {"5", {217, 240, 211, 255}}, {"6", {127, 191, 123, 255}},
    {"7", {27, 120, 55, 255}},
};
static const colorscheme_t prgn7 = {
    "prgn7", prgn7_colors, sizeof(prgn7_colors) / sizeof(prgn7_colors[0])};

static const color_t prgn8_colors[] = {
    {"1", {118, 42, 131, 255}},  {"2", {153, 112, 171, 255}},
    {"3", {194, 165, 207, 255}}, {"4", {231, 212, 232, 255}},
    {"5", {217, 240, 211, 255}}, {"6", {166, 219, 160, 255}},
    {"7", {90, 174, 97, 255}},   {"8", {27, 120, 55, 255}},
};
static const colorscheme_t prgn8 = {
    "prgn8", prgn8_colors, sizeof(prgn8_colors) / sizeof(prgn8_colors[0])};

static const color_t prgn9_colors[] = {
    {"1", {118, 42, 131, 255}},  {"2", {153, 112, 171, 255}},
    {"3", {194, 165, 207, 255}}, {"4", {231, 212, 232, 255}},
    {"5", {247, 247, 247, 255}}, {"6", {217, 240, 211, 255}},
    {"7", {166, 219, 160, 255}}, {"8", {90, 174, 97, 255}},
    {"9", {27, 120, 55, 255}},
};
static const colorscheme_t prgn9 = {
    "prgn9", prgn9_colors, sizeof(prgn9_colors) / sizeof(prgn9_colors[0])};

static const color_t pubu3_colors[] = {
    {"1", {236, 231, 242, 255}},
    {"2", {166, 189, 219, 255}},
    {"3", {43, 140, 190, 255}},
};
static const colorscheme_t pubu3 = {
    "pubu3", pubu3_colors, sizeof(pubu3_colors) / sizeof(pubu3_colors[0])};

static const color_t pubu4_colors[] = {
    {"1", {241, 238, 246, 255}},
    {"2", {189, 201, 225, 255}},
    {"3", {116, 169, 207, 255}},
    {"4", {5, 112, 176, 255}},
};
static const colorscheme_t pubu4 = {
    "pubu4", pubu4_colors, sizeof(pubu4_colors) / sizeof(pubu4_colors[0])};

static const color_t pubu5_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {189, 201, 225, 255}},
    {"3", {116, 169, 207, 255}}, {"4", {43, 140, 190, 255}},
    {"5", {4, 90, 141, 255}},
};
static const colorscheme_t pubu5 = {
    "pubu5", pubu5_colors, sizeof(pubu5_colors) / sizeof(pubu5_colors[0])};

static const color_t pubu6_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {208, 209, 230, 255}},
    {"3", {166, 189, 219, 255}}, {"4", {116, 169, 207, 255}},
    {"5", {43, 140, 190, 255}},  {"6", {4, 90, 141, 255}},
};
static const colorscheme_t pubu6 = {
    "pubu6", pubu6_colors, sizeof(pubu6_colors) / sizeof(pubu6_colors[0])};

static const color_t pubu7_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {208, 209, 230, 255}},
    {"3", {166, 189, 219, 255}}, {"4", {116, 169, 207, 255}},
    {"5", {54, 144, 192, 255}},  {"6", {5, 112, 176, 255}},
    {"7", {3, 78, 123, 255}},
};
static const colorscheme_t pubu7 = {
    "pubu7", pubu7_colors, sizeof(pubu7_colors) / sizeof(pubu7_colors[0])};

static const color_t pubu8_colors[] = {
    {"1", {255, 247, 251, 255}}, {"2", {236, 231, 242, 255}},
    {"3", {208, 209, 230, 255}}, {"4", {166, 189, 219, 255}},
    {"5", {116, 169, 207, 255}}, {"6", {54, 144, 192, 255}},
    {"7", {5, 112, 176, 255}},   {"8", {3, 78, 123, 255}},
};
static const colorscheme_t pubu8 = {
    "pubu8", pubu8_colors, sizeof(pubu8_colors) / sizeof(pubu8_colors[0])};

static const color_t pubu9_colors[] = {
    {"1", {255, 247, 251, 255}}, {"2", {236, 231, 242, 255}},
    {"3", {208, 209, 230, 255}}, {"4", {166, 189, 219, 255}},
    {"5", {116, 169, 207, 255}}, {"6", {54, 144, 192, 255}},
    {"7", {5, 112, 176, 255}},   {"8", {4, 90, 141, 255}},
    {"9", {2, 56, 88, 255}},
};
static const colorscheme_t pubu9 = {
    "pubu9", pubu9_colors, sizeof(pubu9_colors) / sizeof(pubu9_colors[0])};

static const color_t pubugn3_colors[] = {
    {"1", {236, 226, 240, 255}},
    {"2", {166, 189, 219, 255}},
    {"3", {28, 144, 153, 255}},
};
static const colorscheme_t pubugn3 = {"pubugn3", pubugn3_colors,
                                      sizeof(pubugn3_colors) /
                                          sizeof(pubugn3_colors[0])};

static const color_t pubugn4_colors[] = {
    {"1", {246, 239, 247, 255}},
    {"2", {189, 201, 225, 255}},
    {"3", {103, 169, 207, 255}},
    {"4", {2, 129, 138, 255}},
};
static const colorscheme_t pubugn4 = {"pubugn4", pubugn4_colors,
                                      sizeof(pubugn4_colors) /
                                          sizeof(pubugn4_colors[0])};

static const color_t pubugn5_colors[] = {
    {"1", {246, 239, 247, 255}}, {"2", {189, 201, 225, 255}},
    {"3", {103, 169, 207, 255}}, {"4", {28, 144, 153, 255}},
    {"5", {1, 108, 89, 255}},
};
static const colorscheme_t pubugn5 = {"pubugn5", pubugn5_colors,
                                      sizeof(pubugn5_colors) /
                                          sizeof(pubugn5_colors[0])};

static const color_t pubugn6_colors[] = {
    {"1", {246, 239, 247, 255}}, {"2", {208, 209, 230, 255}},
    {"3", {166, 189, 219, 255}}, {"4", {103, 169, 207, 255}},
    {"5", {28, 144, 153, 255}},  {"6", {1, 108, 89, 255}},
};
static const colorscheme_t pubugn6 = {"pubugn6", pubugn6_colors,
                                      sizeof(pubugn6_colors) /
                                          sizeof(pubugn6_colors[0])};

static const color_t pubugn7_colors[] = {
    {"1", {246, 239, 247, 255}}, {"2", {208, 209, 230, 255}},
    {"3", {166, 189, 219, 255}}, {"4", {103, 169, 207, 255}},
    {"5", {54, 144, 192, 255}},  {"6", {2, 129, 138, 255}},
    {"7", {1, 100, 80, 255}},
};
static const colorscheme_t pubugn7 = {"pubugn7", pubugn7_colors,
                                      sizeof(pubugn7_colors) /
                                          sizeof(pubugn7_colors[0])};

static const color_t pubugn8_colors[] = {
    {"1", {255, 247, 251, 255}}, {"2", {236, 226, 240, 255}},
    {"3", {208, 209, 230, 255}}, {"4", {166, 189, 219, 255}},
    {"5", {103, 169, 207, 255}}, {"6", {54, 144, 192, 255}},
    {"7", {2, 129, 138, 255}},   {"8", {1, 100, 80, 255}},
};
static const colorscheme_t pubugn8 = {"pubugn8", pubugn8_colors,
                                      sizeof(pubugn8_colors) /
                                          sizeof(pubugn8_colors[0])};

static const color_t pubugn9_colors[] = {
    {"1", {255, 247, 251, 255}}, {"2", {236, 226, 240, 255}},
    {"3", {208, 209, 230, 255}}, {"4", {166, 189, 219, 255}},
    {"5", {103, 169, 207, 255}}, {"6", {54, 144, 192, 255}},
    {"7", {2, 129, 138, 255}},   {"8", {1, 108, 89, 255}},
    {"9", {1, 70, 54, 255}},
};
static const colorscheme_t pubugn9 = {"pubugn9", pubugn9_colors,
                                      sizeof(pubugn9_colors) /
                                          sizeof(pubugn9_colors[0])};

static const color_t puor10_colors[] = {
    {"1", {127, 59, 8, 255}},    {"10", {45, 0, 75, 255}},
    {"2", {179, 88, 6, 255}},    {"3", {224, 130, 20, 255}},
    {"4", {253, 184, 99, 255}},  {"5", {254, 224, 182, 255}},
    {"6", {216, 218, 235, 255}}, {"7", {178, 171, 210, 255}},
    {"8", {128, 115, 172, 255}}, {"9", {84, 39, 136, 255}},
};
static const colorscheme_t puor10 = {
    "puor10", puor10_colors, sizeof(puor10_colors) / sizeof(puor10_colors[0])};

static const color_t puor11_colors[] = {
    {"1", {127, 59, 8, 255}},    {"10", {84, 39, 136, 255}},
    {"11", {45, 0, 75, 255}},    {"2", {179, 88, 6, 255}},
    {"3", {224, 130, 20, 255}},  {"4", {253, 184, 99, 255}},
    {"5", {254, 224, 182, 255}}, {"6", {247, 247, 247, 255}},
    {"7", {216, 218, 235, 255}}, {"8", {178, 171, 210, 255}},
    {"9", {128, 115, 172, 255}},
};
static const colorscheme_t puor11 = {
    "puor11", puor11_colors, sizeof(puor11_colors) / sizeof(puor11_colors[0])};

static const color_t puor3_colors[] = {
    {"1", {241, 163, 64, 255}},
    {"2", {247, 247, 247, 255}},
    {"3", {153, 142, 195, 255}},
};
static const colorscheme_t puor3 = {
    "puor3", puor3_colors, sizeof(puor3_colors) / sizeof(puor3_colors[0])};

static const color_t puor4_colors[] = {
    {"1", {230, 97, 1, 255}},
    {"2", {253, 184, 99, 255}},
    {"3", {178, 171, 210, 255}},
    {"4", {94, 60, 153, 255}},
};
static const colorscheme_t puor4 = {
    "puor4", puor4_colors, sizeof(puor4_colors) / sizeof(puor4_colors[0])};

static const color_t puor5_colors[] = {
    {"1", {230, 97, 1, 255}},    {"2", {253, 184, 99, 255}},
    {"3", {247, 247, 247, 255}}, {"4", {178, 171, 210, 255}},
    {"5", {94, 60, 153, 255}},
};
static const colorscheme_t puor5 = {
    "puor5", puor5_colors, sizeof(puor5_colors) / sizeof(puor5_colors[0])};

static const color_t puor6_colors[] = {
    {"1", {179, 88, 6, 255}},    {"2", {241, 163, 64, 255}},
    {"3", {254, 224, 182, 255}}, {"4", {216, 218, 235, 255}},
    {"5", {153, 142, 195, 255}}, {"6", {84, 39, 136, 255}},
};
static const colorscheme_t puor6 = {
    "puor6", puor6_colors, sizeof(puor6_colors) / sizeof(puor6_colors[0])};

static const color_t puor7_colors[] = {
    {"1", {179, 88, 6, 255}},    {"2", {241, 163, 64, 255}},
    {"3", {254, 224, 182, 255}}, {"4", {247, 247, 247, 255}},
    {"5", {216, 218, 235, 255}}, {"6", {153, 142, 195, 255}},
    {"7", {84, 39, 136, 255}},
};
static const colorscheme_t puor7 = {
    "puor7", puor7_colors, sizeof(puor7_colors) / sizeof(puor7_colors[0])};

static const color_t puor8_colors[] = {
    {"1", {179, 88, 6, 255}},    {"2", {224, 130, 20, 255}},
    {"3", {253, 184, 99, 255}},  {"4", {254, 224, 182, 255}},
    {"5", {216, 218, 235, 255}}, {"6", {178, 171, 210, 255}},
    {"7", {128, 115, 172, 255}}, {"8", {84, 39, 136, 255}},
};
static const colorscheme_t puor8 = {
    "puor8", puor8_colors, sizeof(puor8_colors) / sizeof(puor8_colors[0])};

static const color_t puor9_colors[] = {
    {"1", {179, 88, 6, 255}},    {"2", {224, 130, 20, 255}},
    {"3", {253, 184, 99, 255}},  {"4", {254, 224, 182, 255}},
    {"5", {247, 247, 247, 255}}, {"6", {216, 218, 235, 255}},
    {"7", {178, 171, 210, 255}}, {"8", {128, 115, 172, 255}},
    {"9", {84, 39, 136, 255}},
};
static const colorscheme_t puor9 = {
    "puor9", puor9_colors, sizeof(puor9_colors) / sizeof(puor9_colors[0])};

static const color_t purd3_colors[] = {
    {"1", {231, 225, 239, 255}},
    {"2", {201, 148, 199, 255}},
    {"3", {221, 28, 119, 255}},
};
static const colorscheme_t purd3 = {
    "purd3", purd3_colors, sizeof(purd3_colors) / sizeof(purd3_colors[0])};

static const color_t purd4_colors[] = {
    {"1", {241, 238, 246, 255}},
    {"2", {215, 181, 216, 255}},
    {"3", {223, 101, 176, 255}},
    {"4", {206, 18, 86, 255}},
};
static const colorscheme_t purd4 = {
    "purd4", purd4_colors, sizeof(purd4_colors) / sizeof(purd4_colors[0])};

static const color_t purd5_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {215, 181, 216, 255}},
    {"3", {223, 101, 176, 255}}, {"4", {221, 28, 119, 255}},
    {"5", {152, 0, 67, 255}},
};
static const colorscheme_t purd5 = {
    "purd5", purd5_colors, sizeof(purd5_colors) / sizeof(purd5_colors[0])};

static const color_t purd6_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {212, 185, 218, 255}},
    {"3", {201, 148, 199, 255}}, {"4", {223, 101, 176, 255}},
    {"5", {221, 28, 119, 255}},  {"6", {152, 0, 67, 255}},
};
static const colorscheme_t purd6 = {
    "purd6", purd6_colors, sizeof(purd6_colors) / sizeof(purd6_colors[0])};

static const color_t purd7_colors[] = {
    {"1", {241, 238, 246, 255}}, {"2", {212, 185, 218, 255}},
    {"3", {201, 148, 199, 255}}, {"4", {223, 101, 176, 255}},
    {"5", {231, 41, 138, 255}},  {"6", {206, 18, 86, 255}},
    {"7", {145, 0, 63, 255}},
};
static const colorscheme_t purd7 = {
    "purd7", purd7_colors, sizeof(purd7_colors) / sizeof(purd7_colors[0])};

static const color_t purd8_colors[] = {
    {"1", {247, 244, 249, 255}}, {"2", {231, 225, 239, 255}},
    {"3", {212, 185, 218, 255}}, {"4", {201, 148, 199, 255}},
    {"5", {223, 101, 176, 255}}, {"6", {231, 41, 138, 255}},
    {"7", {206, 18, 86, 255}},   {"8", {145, 0, 63, 255}},
};
static const colorscheme_t purd8 = {
    "purd8", purd8_colors, sizeof(purd8_colors) / sizeof(purd8_colors[0])};

static const color_t purd9_colors[] = {
    {"1", {247, 244, 249, 255}}, {"2", {231, 225, 239, 255}},
    {"3", {212, 185, 218, 255}}, {"4", {201, 148, 199, 255}},
    {"5", {223, 101, 176, 255}}, {"6", {231, 41, 138, 255}},
    {"7", {206, 18, 86, 255}},   {"8", {152, 0, 67, 255}},
    {"9", {103, 0, 31, 255}},
};
static const colorscheme_t purd9 = {
    "purd9", purd9_colors, sizeof(purd9_colors) / sizeof(purd9_colors[0])};

static const color_t purples3_colors[] = {
    {"1", {239, 237, 245, 255}},
    {"2", {188, 189, 220, 255}},
    {"3", {117, 107, 177, 255}},
};
static const colorscheme_t purples3 = {"purples3", purples3_colors,
                                       sizeof(purples3_colors) /
                                           sizeof(purples3_colors[0])};

static const color_t purples4_colors[] = {
    {"1", {242, 240, 247, 255}},
    {"2", {203, 201, 226, 255}},
    {"3", {158, 154, 200, 255}},
    {"4", {106, 81, 163, 255}},
};
static const colorscheme_t purples4 = {"purples4", purples4_colors,
                                       sizeof(purples4_colors) /
                                           sizeof(purples4_colors[0])};

static const color_t purples5_colors[] = {
    {"1", {242, 240, 247, 255}}, {"2", {203, 201, 226, 255}},
    {"3", {158, 154, 200, 255}}, {"4", {117, 107, 177, 255}},
    {"5", {84, 39, 143, 255}},
};
static const colorscheme_t purples5 = {"purples5", purples5_colors,
                                       sizeof(purples5_colors) /
                                           sizeof(purples5_colors[0])};

static const color_t purples6_colors[] = {
    {"1", {242, 240, 247, 255}}, {"2", {218, 218, 235, 255}},
    {"3", {188, 189, 220, 255}}, {"4", {158, 154, 200, 255}},
    {"5", {117, 107, 177, 255}}, {"6", {84, 39, 143, 255}},
};
static const colorscheme_t purples6 = {"purples6", purples6_colors,
                                       sizeof(purples6_colors) /
                                           sizeof(purples6_colors[0])};

static const color_t purples7_colors[] = {
    {"1", {242, 240, 247, 255}}, {"2", {218, 218, 235, 255}},
    {"3", {188, 189, 220, 255}}, {"4", {158, 154, 200, 255}},
    {"5", {128, 125, 186, 255}}, {"6", {106, 81, 163, 255}},
    {"7", {74, 20, 134, 255}},
};
static const colorscheme_t purples7 = {"purples7", purples7_colors,
                                       sizeof(purples7_colors) /
                                           sizeof(purples7_colors[0])};

static const color_t purples8_colors[] = {
    {"1", {252, 251, 253, 255}}, {"2", {239, 237, 245, 255}},
    {"3", {218, 218, 235, 255}}, {"4", {188, 189, 220, 255}},
    {"5", {158, 154, 200, 255}}, {"6", {128, 125, 186, 255}},
    {"7", {106, 81, 163, 255}},  {"8", {74, 20, 134, 255}},
};
static const colorscheme_t purples8 = {"purples8", purples8_colors,
                                       sizeof(purples8_colors) /
                                           sizeof(purples8_colors[0])};

static const color_t purples9_colors[] = {
    {"1", {252, 251, 253, 255}}, {"2", {239, 237, 245, 255}},
    {"3", {218, 218, 235, 255}}, {"4", {188, 189, 220, 255}},
    {"5", {158, 154, 200, 255}}, {"6", {128, 125, 186, 255}},
    {"7", {106, 81, 163, 255}},  {"8", {84, 39, 143, 255}},
    {"9", {63, 0, 125, 255}},
};
static const colorscheme_t purples9 = {"purples9", purples9_colors,
                                       sizeof(purples9_colors) /
                                           sizeof(purples9_colors[0])};

static const color_t rdbu10_colors[] = {
    {"1", {103, 0, 31, 255}},    {"10", {5, 48, 97, 255}},
    {"2", {178, 24, 43, 255}},   {"3", {214, 96, 77, 255}},
    {"4", {244, 165, 130, 255}}, {"5", {253, 219, 199, 255}},
    {"6", {209, 229, 240, 255}}, {"7", {146, 197, 222, 255}},
    {"8", {67, 147, 195, 255}},  {"9", {33, 102, 172, 255}},
};
static const colorscheme_t rdbu10 = {
    "rdbu10", rdbu10_colors, sizeof(rdbu10_colors) / sizeof(rdbu10_colors[0])};

static const color_t rdbu11_colors[] = {
    {"1", {103, 0, 31, 255}},    {"10", {33, 102, 172, 255}},
    {"11", {5, 48, 97, 255}},    {"2", {178, 24, 43, 255}},
    {"3", {214, 96, 77, 255}},   {"4", {244, 165, 130, 255}},
    {"5", {253, 219, 199, 255}}, {"6", {247, 247, 247, 255}},
    {"7", {209, 229, 240, 255}}, {"8", {146, 197, 222, 255}},
    {"9", {67, 147, 195, 255}},
};
static const colorscheme_t rdbu11 = {
    "rdbu11", rdbu11_colors, sizeof(rdbu11_colors) / sizeof(rdbu11_colors[0])};

static const color_t rdbu3_colors[] = {
    {"1", {239, 138, 98, 255}},
    {"2", {247, 247, 247, 255}},
    {"3", {103, 169, 207, 255}},
};
static const colorscheme_t rdbu3 = {
    "rdbu3", rdbu3_colors, sizeof(rdbu3_colors) / sizeof(rdbu3_colors[0])};

static const color_t rdbu4_colors[] = {
    {"1", {202, 0, 32, 255}},
    {"2", {244, 165, 130, 255}},
    {"3", {146, 197, 222, 255}},
    {"4", {5, 113, 176, 255}},
};
static const colorscheme_t rdbu4 = {
    "rdbu4", rdbu4_colors, sizeof(rdbu4_colors) / sizeof(rdbu4_colors[0])};

static const color_t rdbu5_colors[] = {
    {"1", {202, 0, 32, 255}},    {"2", {244, 165, 130, 255}},
    {"3", {247, 247, 247, 255}}, {"4", {146, 197, 222, 255}},
    {"5", {5, 113, 176, 255}},
};
static const colorscheme_t rdbu5 = {
    "rdbu5", rdbu5_colors, sizeof(rdbu5_colors) / sizeof(rdbu5_colors[0])};

static const color_t rdbu6_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {239, 138, 98, 255}},
    {"3", {253, 219, 199, 255}}, {"4", {209, 229, 240, 255}},
    {"5", {103, 169, 207, 255}}, {"6", {33, 102, 172, 255}},
};
static const colorscheme_t rdbu6 = {
    "rdbu6", rdbu6_colors, sizeof(rdbu6_colors) / sizeof(rdbu6_colors[0])};

static const color_t rdbu7_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {239, 138, 98, 255}},
    {"3", {253, 219, 199, 255}}, {"4", {247, 247, 247, 255}},
    {"5", {209, 229, 240, 255}}, {"6", {103, 169, 207, 255}},
    {"7", {33, 102, 172, 255}},
};
static const colorscheme_t rdbu7 = {
    "rdbu7", rdbu7_colors, sizeof(rdbu7_colors) / sizeof(rdbu7_colors[0])};

static const color_t rdbu8_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {214, 96, 77, 255}},
    {"3", {244, 165, 130, 255}}, {"4", {253, 219, 199, 255}},
    {"5", {209, 229, 240, 255}}, {"6", {146, 197, 222, 255}},
    {"7", {67, 147, 195, 255}},  {"8", {33, 102, 172, 255}},
};
static const colorscheme_t rdbu8 = {
    "rdbu8", rdbu8_colors, sizeof(rdbu8_colors) / sizeof(rdbu8_colors[0])};

static const color_t rdbu9_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {214, 96, 77, 255}},
    {"3", {244, 165, 130, 255}}, {"4", {253, 219, 199, 255}},
    {"5", {247, 247, 247, 255}}, {"6", {209, 229, 240, 255}},
    {"7", {146, 197, 222, 255}}, {"8", {67, 147, 195, 255}},
    {"9", {33, 102, 172, 255}},
};
static const colorscheme_t rdbu9 = {
    "rdbu9", rdbu9_colors, sizeof(rdbu9_colors) / sizeof(rdbu9_colors[0])};

static const color_t rdgy10_colors[] = {
    {"1", {103, 0, 31, 255}},    {"10", {26, 26, 26, 255}},
    {"2", {178, 24, 43, 255}},   {"3", {214, 96, 77, 255}},
    {"4", {244, 165, 130, 255}}, {"5", {253, 219, 199, 255}},
    {"6", {224, 224, 224, 255}}, {"7", {186, 186, 186, 255}},
    {"8", {135, 135, 135, 255}}, {"9", {77, 77, 77, 255}},
};
static const colorscheme_t rdgy10 = {
    "rdgy10", rdgy10_colors, sizeof(rdgy10_colors) / sizeof(rdgy10_colors[0])};

static const color_t rdgy11_colors[] = {
    {"1", {103, 0, 31, 255}},    {"10", {77, 77, 77, 255}},
    {"11", {26, 26, 26, 255}},   {"2", {178, 24, 43, 255}},
    {"3", {214, 96, 77, 255}},   {"4", {244, 165, 130, 255}},
    {"5", {253, 219, 199, 255}}, {"6", {255, 255, 255, 255}},
    {"7", {224, 224, 224, 255}}, {"8", {186, 186, 186, 255}},
    {"9", {135, 135, 135, 255}},
};
static const colorscheme_t rdgy11 = {
    "rdgy11", rdgy11_colors, sizeof(rdgy11_colors) / sizeof(rdgy11_colors[0])};

static const color_t rdgy3_colors[] = {
    {"1", {239, 138, 98, 255}},
    {"2", {255, 255, 255, 255}},
    {"3", {153, 153, 153, 255}},
};
static const colorscheme_t rdgy3 = {
    "rdgy3", rdgy3_colors, sizeof(rdgy3_colors) / sizeof(rdgy3_colors[0])};

static const color_t rdgy4_colors[] = {
    {"1", {202, 0, 32, 255}},
    {"2", {244, 165, 130, 255}},
    {"3", {186, 186, 186, 255}},
    {"4", {64, 64, 64, 255}},
};
static const colorscheme_t rdgy4 = {
    "rdgy4", rdgy4_colors, sizeof(rdgy4_colors) / sizeof(rdgy4_colors[0])};

static const color_t rdgy5_colors[] = {
    {"1", {202, 0, 32, 255}},    {"2", {244, 165, 130, 255}},
    {"3", {255, 255, 255, 255}}, {"4", {186, 186, 186, 255}},
    {"5", {64, 64, 64, 255}},
};
static const colorscheme_t rdgy5 = {
    "rdgy5", rdgy5_colors, sizeof(rdgy5_colors) / sizeof(rdgy5_colors[0])};

static const color_t rdgy6_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {239, 138, 98, 255}},
    {"3", {253, 219, 199, 255}}, {"4", {224, 224, 224, 255}},
    {"5", {153, 153, 153, 255}}, {"6", {77, 77, 77, 255}},
};
static const colorscheme_t rdgy6 = {
    "rdgy6", rdgy6_colors, sizeof(rdgy6_colors) / sizeof(rdgy6_colors[0])};

static const color_t rdgy7_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {239, 138, 98, 255}},
    {"3", {253, 219, 199, 255}}, {"4", {255, 255, 255, 255}},
    {"5", {224, 224, 224, 255}}, {"6", {153, 153, 153, 255}},
    {"7", {77, 77, 77, 255}},
};
static const colorscheme_t rdgy7 = {
    "rdgy7", rdgy7_colors, sizeof(rdgy7_colors) / sizeof(rdgy7_colors[0])};

static const color_t rdgy8_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {214, 96, 77, 255}},
    {"3", {244, 165, 130, 255}}, {"4", {253, 219, 199, 255}},
    {"5", {224, 224, 224, 255}}, {"6", {186, 186, 186, 255}},
    {"7", {135, 135, 135, 255}}, {"8", {77, 77, 77, 255}},
};
static const colorscheme_t rdgy8 = {
    "rdgy8", rdgy8_colors, sizeof(rdgy8_colors) / sizeof(rdgy8_colors[0])};

static const color_t rdgy9_colors[] = {
    {"1", {178, 24, 43, 255}},   {"2", {214, 96, 77, 255}},
    {"3", {244, 165, 130, 255}}, {"4", {253, 219, 199, 255}},
    {"5", {255, 255, 255, 255}}, {"6", {224, 224, 224, 255}},
    {"7", {186, 186, 186, 255}}, {"8", {135, 135, 135, 255}},
    {"9", {77, 77, 77, 255}},
};
static const colorscheme_t rdgy9 = {
    "rdgy9", rdgy9_colors, sizeof(rdgy9_colors) / sizeof(rdgy9_colors[0])};

static const color_t rdpu3_colors[] = {
    {"1", {253, 224, 221, 255}},
    {"2", {250, 159, 181, 255}},
    {"3", {197, 27, 138, 255}},
};
static const colorscheme_t rdpu3 = {
    "rdpu3", rdpu3_colors, sizeof(rdpu3_colors) / sizeof(rdpu3_colors[0])};

static const color_t rdpu4_colors[] = {
    {"1", {254, 235, 226, 255}},
    {"2", {251, 180, 185, 255}},
    {"3", {247, 104, 161, 255}},
    {"4", {174, 1, 126, 255}},
};
static const colorscheme_t rdpu4 = {
    "rdpu4", rdpu4_colors, sizeof(rdpu4_colors) / sizeof(rdpu4_colors[0])};

static const color_t rdpu5_colors[] = {
    {"1", {254, 235, 226, 255}}, {"2", {251, 180, 185, 255}},
    {"3", {247, 104, 161, 255}}, {"4", {197, 27, 138, 255}},
    {"5", {122, 1, 119, 255}},
};
static const colorscheme_t rdpu5 = {
    "rdpu5", rdpu5_colors, sizeof(rdpu5_colors) / sizeof(rdpu5_colors[0])};

static const color_t rdpu6_colors[] = {
    {"1", {254, 235, 226, 255}}, {"2", {252, 197, 192, 255}},
    {"3", {250, 159, 181, 255}}, {"4", {247, 104, 161, 255}},
    {"5", {197, 27, 138, 255}},  {"6", {122, 1, 119, 255}},
};
static const colorscheme_t rdpu6 = {
    "rdpu6", rdpu6_colors, sizeof(rdpu6_colors) / sizeof(rdpu6_colors[0])};

static const color_t rdpu7_colors[] = {
    {"1", {254, 235, 226, 255}}, {"2", {252, 197, 192, 255}},
    {"3", {250, 159, 181, 255}}, {"4", {247, 104, 161, 255}},
    {"5", {221, 52, 151, 255}},  {"6", {174, 1, 126, 255}},
    {"7", {122, 1, 119, 255}},
};
static const colorscheme_t rdpu7 = {
    "rdpu7", rdpu7_colors, sizeof(rdpu7_colors) / sizeof(rdpu7_colors[0])};

static const color_t rdpu8_colors[] = {
    {"1", {255, 247, 243, 255}}, {"2", {253, 224, 221, 255}},
    {"3", {252, 197, 192, 255}}, {"4", {250, 159, 181, 255}},
    {"5", {247, 104, 161, 255}}, {"6", {221, 52, 151, 255}},
    {"7", {174, 1, 126, 255}},   {"8", {122, 1, 119, 255}},
};
static const colorscheme_t rdpu8 = {
    "rdpu8", rdpu8_colors, sizeof(rdpu8_colors) / sizeof(rdpu8_colors[0])};

static const color_t rdpu9_colors[] = {
    {"1", {255, 247, 243, 255}}, {"2", {253, 224, 221, 255}},
    {"3", {252, 197, 192, 255}}, {"4", {250, 159, 181, 255}},
    {"5", {247, 104, 161, 255}}, {"6", {221, 52, 151, 255}},
    {"7", {174, 1, 126, 255}},   {"8", {122, 1, 119, 255}},
    {"9", {73, 0, 106, 255}},
};
static const colorscheme_t rdpu9 = {
    "rdpu9", rdpu9_colors, sizeof(rdpu9_colors) / sizeof(rdpu9_colors[0])};

static const color_t rdylbu10_colors[] = {
    {"1", {165, 0, 38, 255}},    {"10", {49, 54, 149, 255}},
    {"2", {215, 48, 39, 255}},   {"3", {244, 109, 67, 255}},
    {"4", {253, 174, 97, 255}},  {"5", {254, 224, 144, 255}},
    {"6", {224, 243, 248, 255}}, {"7", {171, 217, 233, 255}},
    {"8", {116, 173, 209, 255}}, {"9", {69, 117, 180, 255}},
};
static const colorscheme_t rdylbu10 = {"rdylbu10", rdylbu10_colors,
                                       sizeof(rdylbu10_colors) /
                                           sizeof(rdylbu10_colors[0])};

static const color_t rdylbu11_colors[] = {
    {"1", {165, 0, 38, 255}},    {"10", {69, 117, 180, 255}},
    {"11", {49, 54, 149, 255}},  {"2", {215, 48, 39, 255}},
    {"3", {244, 109, 67, 255}},  {"4", {253, 174, 97, 255}},
    {"5", {254, 224, 144, 255}}, {"6", {255, 255, 191, 255}},
    {"7", {224, 243, 248, 255}}, {"8", {171, 217, 233, 255}},
    {"9", {116, 173, 209, 255}},
};
static const colorscheme_t rdylbu11 = {"rdylbu11", rdylbu11_colors,
                                       sizeof(rdylbu11_colors) /
                                           sizeof(rdylbu11_colors[0])};

static const color_t rdylbu3_colors[] = {
    {"1", {252, 141, 89, 255}},
    {"2", {255, 255, 191, 255}},
    {"3", {145, 191, 219, 255}},
};
static const colorscheme_t rdylbu3 = {"rdylbu3", rdylbu3_colors,
                                      sizeof(rdylbu3_colors) /
                                          sizeof(rdylbu3_colors[0])};

static const color_t rdylbu4_colors[] = {
    {"1", {215, 25, 28, 255}},
    {"2", {253, 174, 97, 255}},
    {"3", {171, 217, 233, 255}},
    {"4", {44, 123, 182, 255}},
};
static const colorscheme_t rdylbu4 = {"rdylbu4", rdylbu4_colors,
                                      sizeof(rdylbu4_colors) /
                                          sizeof(rdylbu4_colors[0])};

static const color_t rdylbu5_colors[] = {
    {"1", {215, 25, 28, 255}},   {"2", {253, 174, 97, 255}},
    {"3", {255, 255, 191, 255}}, {"4", {171, 217, 233, 255}},
    {"5", {44, 123, 182, 255}},
};
static const colorscheme_t rdylbu5 = {"rdylbu5", rdylbu5_colors,
                                      sizeof(rdylbu5_colors) /
                                          sizeof(rdylbu5_colors[0])};

static const color_t rdylbu6_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 144, 255}}, {"4", {224, 243, 248, 255}},
    {"5", {145, 191, 219, 255}}, {"6", {69, 117, 180, 255}},
};
static const colorscheme_t rdylbu6 = {"rdylbu6", rdylbu6_colors,
                                      sizeof(rdylbu6_colors) /
                                          sizeof(rdylbu6_colors[0])};

static const color_t rdylbu7_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 144, 255}}, {"4", {255, 255, 191, 255}},
    {"5", {224, 243, 248, 255}}, {"6", {145, 191, 219, 255}},
    {"7", {69, 117, 180, 255}},
};
static const colorscheme_t rdylbu7 = {"rdylbu7", rdylbu7_colors,
                                      sizeof(rdylbu7_colors) /
                                          sizeof(rdylbu7_colors[0])};

static const color_t rdylbu8_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 144, 255}},
    {"5", {224, 243, 248, 255}}, {"6", {171, 217, 233, 255}},
    {"7", {116, 173, 209, 255}}, {"8", {69, 117, 180, 255}},
};
static const colorscheme_t rdylbu8 = {"rdylbu8", rdylbu8_colors,
                                      sizeof(rdylbu8_colors) /
                                          sizeof(rdylbu8_colors[0])};

static const color_t rdylbu9_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 144, 255}},
    {"5", {255, 255, 191, 255}}, {"6", {224, 243, 248, 255}},
    {"7", {171, 217, 233, 255}}, {"8", {116, 173, 209, 255}},
    {"9", {69, 117, 180, 255}},
};
static const colorscheme_t rdylbu9 = {"rdylbu9", rdylbu9_colors,
                                      sizeof(rdylbu9_colors) /
                                          sizeof(rdylbu9_colors[0])};

static const color_t rdylgn10_colors[] = {
    {"1", {165, 0, 38, 255}},    {"10", {0, 104, 55, 255}},
    {"2", {215, 48, 39, 255}},   {"3", {244, 109, 67, 255}},
    {"4", {253, 174, 97, 255}},  {"5", {254, 224, 139, 255}},
    {"6", {217, 239, 139, 255}}, {"7", {166, 217, 106, 255}},
    {"8", {102, 189, 99, 255}},  {"9", {26, 152, 80, 255}},
};
static const colorscheme_t rdylgn10 = {"rdylgn10", rdylgn10_colors,
                                       sizeof(rdylgn10_colors) /
                                           sizeof(rdylgn10_colors[0])};

static const color_t rdylgn11_colors[] = {
    {"1", {165, 0, 38, 255}},    {"10", {26, 152, 80, 255}},
    {"11", {0, 104, 55, 255}},   {"2", {215, 48, 39, 255}},
    {"3", {244, 109, 67, 255}},  {"4", {253, 174, 97, 255}},
    {"5", {254, 224, 139, 255}}, {"6", {255, 255, 191, 255}},
    {"7", {217, 239, 139, 255}}, {"8", {166, 217, 106, 255}},
    {"9", {102, 189, 99, 255}},
};
static const colorscheme_t rdylgn11 = {"rdylgn11", rdylgn11_colors,
                                       sizeof(rdylgn11_colors) /
                                           sizeof(rdylgn11_colors[0])};

static const color_t rdylgn3_colors[] = {
    {"1", {252, 141, 89, 255}},
    {"2", {255, 255, 191, 255}},
    {"3", {145, 207, 96, 255}},
};
static const colorscheme_t rdylgn3 = {"rdylgn3", rdylgn3_colors,
                                      sizeof(rdylgn3_colors) /
                                          sizeof(rdylgn3_colors[0])};

static const color_t rdylgn4_colors[] = {
    {"1", {215, 25, 28, 255}},
    {"2", {253, 174, 97, 255}},
    {"3", {166, 217, 106, 255}},
    {"4", {26, 150, 65, 255}},
};
static const colorscheme_t rdylgn4 = {"rdylgn4", rdylgn4_colors,
                                      sizeof(rdylgn4_colors) /
                                          sizeof(rdylgn4_colors[0])};

static const color_t rdylgn5_colors[] = {
    {"1", {215, 25, 28, 255}},   {"2", {253, 174, 97, 255}},
    {"3", {255, 255, 191, 255}}, {"4", {166, 217, 106, 255}},
    {"5", {26, 150, 65, 255}},
};
static const colorscheme_t rdylgn5 = {"rdylgn5", rdylgn5_colors,
                                      sizeof(rdylgn5_colors) /
                                          sizeof(rdylgn5_colors[0])};

static const color_t rdylgn6_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 139, 255}}, {"4", {217, 239, 139, 255}},
    {"5", {145, 207, 96, 255}},  {"6", {26, 152, 80, 255}},
};
static const colorscheme_t rdylgn6 = {"rdylgn6", rdylgn6_colors,
                                      sizeof(rdylgn6_colors) /
                                          sizeof(rdylgn6_colors[0])};

static const color_t rdylgn7_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 139, 255}}, {"4", {255, 255, 191, 255}},
    {"5", {217, 239, 139, 255}}, {"6", {145, 207, 96, 255}},
    {"7", {26, 152, 80, 255}},
};
static const colorscheme_t rdylgn7 = {"rdylgn7", rdylgn7_colors,
                                      sizeof(rdylgn7_colors) /
                                          sizeof(rdylgn7_colors[0])};

static const color_t rdylgn8_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 139, 255}},
    {"5", {217, 239, 139, 255}}, {"6", {166, 217, 106, 255}},
    {"7", {102, 189, 99, 255}},  {"8", {26, 152, 80, 255}},
};
static const colorscheme_t rdylgn8 = {"rdylgn8", rdylgn8_colors,
                                      sizeof(rdylgn8_colors) /
                                          sizeof(rdylgn8_colors[0])};

static const color_t rdylgn9_colors[] = {
    {"1", {215, 48, 39, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 139, 255}},
    {"5", {255, 255, 191, 255}}, {"6", {217, 239, 139, 255}},
    {"7", {166, 217, 106, 255}}, {"8", {102, 189, 99, 255}},
    {"9", {26, 152, 80, 255}},
};
static const colorscheme_t rdylgn9 = {"rdylgn9", rdylgn9_colors,
                                      sizeof(rdylgn9_colors) /
                                          sizeof(rdylgn9_colors[0])};

static const color_t reds3_colors[] = {
    {"1", {254, 224, 210, 255}},
    {"2", {252, 146, 114, 255}},
    {"3", {222, 45, 38, 255}},
};
static const colorscheme_t reds3 = {
    "reds3", reds3_colors, sizeof(reds3_colors) / sizeof(reds3_colors[0])};

static const color_t reds4_colors[] = {
    {"1", {254, 229, 217, 255}},
    {"2", {252, 174, 145, 255}},
    {"3", {251, 106, 74, 255}},
    {"4", {203, 24, 29, 255}},
};
static const colorscheme_t reds4 = {
    "reds4", reds4_colors, sizeof(reds4_colors) / sizeof(reds4_colors[0])};

static const color_t reds5_colors[] = {
    {"1", {254, 229, 217, 255}}, {"2", {252, 174, 145, 255}},
    {"3", {251, 106, 74, 255}},  {"4", {222, 45, 38, 255}},
    {"5", {165, 15, 21, 255}},
};
static const colorscheme_t reds5 = {
    "reds5", reds5_colors, sizeof(reds5_colors) / sizeof(reds5_colors[0])};

static const color_t reds6_colors[] = {
    {"1", {254, 229, 217, 255}}, {"2", {252, 187, 161, 255}},
    {"3", {252, 146, 114, 255}}, {"4", {251, 106, 74, 255}},
    {"5", {222, 45, 38, 255}},   {"6", {165, 15, 21, 255}},
};
static const colorscheme_t reds6 = {
    "reds6", reds6_colors, sizeof(reds6_colors) / sizeof(reds6_colors[0])};

static const color_t reds7_colors[] = {
    {"1", {254, 229, 217, 255}}, {"2", {252, 187, 161, 255}},
    {"3", {252, 146, 114, 255}}, {"4", {251, 106, 74, 255}},
    {"5", {239, 59, 44, 255}},   {"6", {203, 24, 29, 255}},
    {"7", {153, 0, 13, 255}},
};
static const colorscheme_t reds7 = {
    "reds7", reds7_colors, sizeof(reds7_colors) / sizeof(reds7_colors[0])};

static const color_t reds8_colors[] = {
    {"1", {255, 245, 240, 255}}, {"2", {254, 224, 210, 255}},
    {"3", {252, 187, 161, 255}}, {"4", {252, 146, 114, 255}},
    {"5", {251, 106, 74, 255}},  {"6", {239, 59, 44, 255}},
    {"7", {203, 24, 29, 255}},   {"8", {153, 0, 13, 255}},
};
static const colorscheme_t reds8 = {
    "reds8", reds8_colors, sizeof(reds8_colors) / sizeof(reds8_colors[0])};

static const color_t reds9_colors[] = {
    {"1", {255, 245, 240, 255}}, {"2", {254, 224, 210, 255}},
    {"3", {252, 187, 161, 255}}, {"4", {252, 146, 114, 255}},
    {"5", {251, 106, 74, 255}},  {"6", {239, 59, 44, 255}},
    {"7", {203, 24, 29, 255}},   {"8", {165, 15, 21, 255}},
    {"9", {103, 0, 13, 255}},
};
static const colorscheme_t reds9 = {
    "reds9", reds9_colors, sizeof(reds9_colors) / sizeof(reds9_colors[0])};

static const color_t set13_colors[] = {
    {"1", {228, 26, 28, 255}},
    {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}},
};
static const colorscheme_t set13 = {
    "set13", set13_colors, sizeof(set13_colors) / sizeof(set13_colors[0])};

static const color_t set14_colors[] = {
    {"1", {228, 26, 28, 255}},
    {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}},
    {"4", {152, 78, 163, 255}},
};
static const colorscheme_t set14 = {
    "set14", set14_colors, sizeof(set14_colors) / sizeof(set14_colors[0])};

static const color_t set15_colors[] = {
    {"1", {228, 26, 28, 255}}, {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}}, {"4", {152, 78, 163, 255}},
    {"5", {255, 127, 0, 255}},
};
static const colorscheme_t set15 = {
    "set15", set15_colors, sizeof(set15_colors) / sizeof(set15_colors[0])};

static const color_t set16_colors[] = {
    {"1", {228, 26, 28, 255}}, {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}}, {"4", {152, 78, 163, 255}},
    {"5", {255, 127, 0, 255}}, {"6", {255, 255, 51, 255}},
};
static const colorscheme_t set16 = {
    "set16", set16_colors, sizeof(set16_colors) / sizeof(set16_colors[0])};

static const color_t set17_colors[] = {
    {"1", {228, 26, 28, 255}}, {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}}, {"4", {152, 78, 163, 255}},
    {"5", {255, 127, 0, 255}}, {"6", {255, 255, 51, 255}},
    {"7", {166, 86, 40, 255}},
};
static const colorscheme_t set17 = {
    "set17", set17_colors, sizeof(set17_colors) / sizeof(set17_colors[0])};

static const color_t set18_colors[] = {
    {"1", {228, 26, 28, 255}}, {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}}, {"4", {152, 78, 163, 255}},
    {"5", {255, 127, 0, 255}}, {"6", {255, 255, 51, 255}},
    {"7", {166, 86, 40, 255}}, {"8", {247, 129, 191, 255}},
};
static const colorscheme_t set18 = {
    "set18", set18_colors, sizeof(set18_colors) / sizeof(set18_colors[0])};

static const color_t set19_colors[] = {
    {"1", {228, 26, 28, 255}},   {"2", {55, 126, 184, 255}},
    {"3", {77, 175, 74, 255}},   {"4", {152, 78, 163, 255}},
    {"5", {255, 127, 0, 255}},   {"6", {255, 255, 51, 255}},
    {"7", {166, 86, 40, 255}},   {"8", {247, 129, 191, 255}},
    {"9", {153, 153, 153, 255}},
};
static const colorscheme_t set19 = {
    "set19", set19_colors, sizeof(set19_colors) / sizeof(set19_colors[0])};

static const color_t set23_colors[] = {
    {"1", {102, 194, 165, 255}},
    {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}},
};
static const colorscheme_t set23 = {
    "set23", set23_colors, sizeof(set23_colors) / sizeof(set23_colors[0])};

static const color_t set24_colors[] = {
    {"1", {102, 194, 165, 255}},
    {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}},
    {"4", {231, 138, 195, 255}},
};
static const colorscheme_t set24 = {
    "set24", set24_colors, sizeof(set24_colors) / sizeof(set24_colors[0])};

static const color_t set25_colors[] = {
    {"1", {102, 194, 165, 255}}, {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}}, {"4", {231, 138, 195, 255}},
    {"5", {166, 216, 84, 255}},
};
static const colorscheme_t set25 = {
    "set25", set25_colors, sizeof(set25_colors) / sizeof(set25_colors[0])};

static const color_t set26_colors[] = {
    {"1", {102, 194, 165, 255}}, {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}}, {"4", {231, 138, 195, 255}},
    {"5", {166, 216, 84, 255}},  {"6", {255, 217, 47, 255}},
};
static const colorscheme_t set26 = {
    "set26", set26_colors, sizeof(set26_colors) / sizeof(set26_colors[0])};

static const color_t set27_colors[] = {
    {"1", {102, 194, 165, 255}}, {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}}, {"4", {231, 138, 195, 255}},
    {"5", {166, 216, 84, 255}},  {"6", {255, 217, 47, 255}},
    {"7", {229, 196, 148, 255}},
};
static const colorscheme_t set27 = {
    "set27", set27_colors, sizeof(set27_colors) / sizeof(set27_colors[0])};

static const color_t set28_colors[] = {
    {"1", {102, 194, 165, 255}}, {"2", {252, 141, 98, 255}},
    {"3", {141, 160, 203, 255}}, {"4", {231, 138, 195, 255}},
    {"5", {166, 216, 84, 255}},  {"6", {255, 217, 47, 255}},
    {"7", {229, 196, 148, 255}}, {"8", {179, 179, 179, 255}},
};
static const colorscheme_t set28 = {
    "set28", set28_colors, sizeof(set28_colors) / sizeof(set28_colors[0])};

static const color_t set310_colors[] = {
    {"1", {141, 211, 199, 255}}, {"10", {188, 128, 189, 255}},
    {"2", {255, 255, 179, 255}}, {"3", {190, 186, 218, 255}},
    {"4", {251, 128, 114, 255}}, {"5", {128, 177, 211, 255}},
    {"6", {253, 180, 98, 255}},  {"7", {179, 222, 105, 255}},
    {"8", {252, 205, 229, 255}}, {"9", {217, 217, 217, 255}},
};
static const colorscheme_t set310 = {
    "set310", set310_colors, sizeof(set310_colors) / sizeof(set310_colors[0])};

static const color_t set311_colors[] = {
    {"1", {141, 211, 199, 255}},  {"10", {188, 128, 189, 255}},
    {"11", {204, 235, 197, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}},  {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}},  {"6", {253, 180, 98, 255}},
    {"7", {179, 222, 105, 255}},  {"8", {252, 205, 229, 255}},
    {"9", {217, 217, 217, 255}},
};
static const colorscheme_t set311 = {
    "set311", set311_colors, sizeof(set311_colors) / sizeof(set311_colors[0])};

static const color_t set312_colors[] = {
    {"1", {141, 211, 199, 255}},  {"10", {188, 128, 189, 255}},
    {"11", {204, 235, 197, 255}}, {"12", {255, 237, 111, 255}},
    {"2", {255, 255, 179, 255}},  {"3", {190, 186, 218, 255}},
    {"4", {251, 128, 114, 255}},  {"5", {128, 177, 211, 255}},
    {"6", {253, 180, 98, 255}},   {"7", {179, 222, 105, 255}},
    {"8", {252, 205, 229, 255}},  {"9", {217, 217, 217, 255}},
};
static const colorscheme_t set312 = {
    "set312", set312_colors, sizeof(set312_colors) / sizeof(set312_colors[0])};

static const color_t set33_colors[] = {
    {"1", {141, 211, 199, 255}},
    {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}},
};
static const colorscheme_t set33 = {
    "set33", set33_colors, sizeof(set33_colors) / sizeof(set33_colors[0])};

static const color_t set34_colors[] = {
    {"1", {141, 211, 199, 255}},
    {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}},
    {"4", {251, 128, 114, 255}},
};
static const colorscheme_t set34 = {
    "set34", set34_colors, sizeof(set34_colors) / sizeof(set34_colors[0])};

static const color_t set35_colors[] = {
    {"1", {141, 211, 199, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}}, {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}},
};
static const colorscheme_t set35 = {
    "set35", set35_colors, sizeof(set35_colors) / sizeof(set35_colors[0])};

static const color_t set36_colors[] = {
    {"1", {141, 211, 199, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}}, {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}}, {"6", {253, 180, 98, 255}},
};
static const colorscheme_t set36 = {
    "set36", set36_colors, sizeof(set36_colors) / sizeof(set36_colors[0])};

static const color_t set37_colors[] = {
    {"1", {141, 211, 199, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}}, {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}}, {"6", {253, 180, 98, 255}},
    {"7", {179, 222, 105, 255}},
};
static const colorscheme_t set37 = {
    "set37", set37_colors, sizeof(set37_colors) / sizeof(set37_colors[0])};

static const color_t set38_colors[] = {
    {"1", {141, 211, 199, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}}, {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}}, {"6", {253, 180, 98, 255}},
    {"7", {179, 222, 105, 255}}, {"8", {252, 205, 229, 255}},
};
static const colorscheme_t set38 = {
    "set38", set38_colors, sizeof(set38_colors) / sizeof(set38_colors[0])};

static const color_t set39_colors[] = {
    {"1", {141, 211, 199, 255}}, {"2", {255, 255, 179, 255}},
    {"3", {190, 186, 218, 255}}, {"4", {251, 128, 114, 255}},
    {"5", {128, 177, 211, 255}}, {"6", {253, 180, 98, 255}},
    {"7", {179, 222, 105, 255}}, {"8", {252, 205, 229, 255}},
    {"9", {217, 217, 217, 255}},
};
static const colorscheme_t set39 = {
    "set39", set39_colors, sizeof(set39_colors) / sizeof(set39_colors[0])};

static const color_t spectral10_colors[] = {
    {"1", {158, 1, 66, 255}},    {"10", {94, 79, 162, 255}},
    {"2", {213, 62, 79, 255}},   {"3", {244, 109, 67, 255}},
    {"4", {253, 174, 97, 255}},  {"5", {254, 224, 139, 255}},
    {"6", {230, 245, 152, 255}}, {"7", {171, 221, 164, 255}},
    {"8", {102, 194, 165, 255}}, {"9", {50, 136, 189, 255}},
};
static const colorscheme_t spectral10 = {"spectral10", spectral10_colors,
                                         sizeof(spectral10_colors) /
                                             sizeof(spectral10_colors[0])};

static const color_t spectral11_colors[] = {
    {"1", {158, 1, 66, 255}},    {"10", {50, 136, 189, 255}},
    {"11", {94, 79, 162, 255}},  {"2", {213, 62, 79, 255}},
    {"3", {244, 109, 67, 255}},  {"4", {253, 174, 97, 255}},
    {"5", {254, 224, 139, 255}}, {"6", {255, 255, 191, 255}},
    {"7", {230, 245, 152, 255}}, {"8", {171, 221, 164, 255}},
    {"9", {102, 194, 165, 255}},
};
static const colorscheme_t spectral11 = {"spectral11", spectral11_colors,
                                         sizeof(spectral11_colors) /
                                             sizeof(spectral11_colors[0])};

static const color_t spectral3_colors[] = {
    {"1", {252, 141, 89, 255}},
    {"2", {255, 255, 191, 255}},
    {"3", {153, 213, 148, 255}},
};
static const colorscheme_t spectral3 = {"spectral3", spectral3_colors,
                                        sizeof(spectral3_colors) /
                                            sizeof(spectral3_colors[0])};

static const color_t spectral4_colors[] = {
    {"1", {215, 25, 28, 255}},
    {"2", {253, 174, 97, 255}},
    {"3", {171, 221, 164, 255}},
    {"4", {43, 131, 186, 255}},
};
static const colorscheme_t spectral4 = {"spectral4", spectral4_colors,
                                        sizeof(spectral4_colors) /
                                            sizeof(spectral4_colors[0])};

static const color_t spectral5_colors[] = {
    {"1", {215, 25, 28, 255}},   {"2", {253, 174, 97, 255}},
    {"3", {255, 255, 191, 255}}, {"4", {171, 221, 164, 255}},
    {"5", {43, 131, 186, 255}},
};
static const colorscheme_t spectral5 = {"spectral5", spectral5_colors,
                                        sizeof(spectral5_colors) /
                                            sizeof(spectral5_colors[0])};

static const color_t spectral6_colors[] = {
    {"1", {213, 62, 79, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 139, 255}}, {"4", {230, 245, 152, 255}},
    {"5", {153, 213, 148, 255}}, {"6", {50, 136, 189, 255}},
};
static const colorscheme_t spectral6 = {"spectral6", spectral6_colors,
                                        sizeof(spectral6_colors) /
                                            sizeof(spectral6_colors[0])};

static const color_t spectral7_colors[] = {
    {"1", {213, 62, 79, 255}},   {"2", {252, 141, 89, 255}},
    {"3", {254, 224, 139, 255}}, {"4", {255, 255, 191, 255}},
    {"5", {230, 245, 152, 255}}, {"6", {153, 213, 148, 255}},
    {"7", {50, 136, 189, 255}},
};
static const colorscheme_t spectral7 = {"spectral7", spectral7_colors,
                                        sizeof(spectral7_colors) /
                                            sizeof(spectral7_colors[0])};

static const color_t spectral8_colors[] = {
    {"1", {213, 62, 79, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 139, 255}},
    {"5", {230, 245, 152, 255}}, {"6", {171, 221, 164, 255}},
    {"7", {102, 194, 165, 255}}, {"8", {50, 136, 189, 255}},
};
static const colorscheme_t spectral8 = {"spectral8", spectral8_colors,
                                        sizeof(spectral8_colors) /
                                            sizeof(spectral8_colors[0])};

static const color_t spectral9_colors[] = {
    {"1", {213, 62, 79, 255}},   {"2", {244, 109, 67, 255}},
    {"3", {253, 174, 97, 255}},  {"4", {254, 224, 139, 255}},
    {"5", {255, 255, 191, 255}}, {"6", {230, 245, 152, 255}},
    {"7", {171, 221, 164, 255}}, {"8", {102, 194, 165, 255}},
    {"9", {50, 136, 189, 255}},
};
static const colorscheme_t spectral9 = {"spectral9", spectral9_colors,
                                        sizeof(spectral9_colors) /
                                            sizeof(spectral9_colors[0])};

static const color_t svg_colors[] = {
    {"aliceblue", {240, 248, 255, 255}},
    {"antiquewhite", {250, 235, 215, 255}},
    {"aqua", {0, 255, 255, 255}},
    {"aquamarine", {127, 255, 212, 255}},
    {"azure", {240, 255, 255, 255}},
    {"beige", {245, 245, 220, 255}},
    {"bisque", {255, 228, 196, 255}},
    {"black", {0, 0, 0, 255}},
    {"blanchedalmond", {255, 235, 205, 255}},
    {"blue", {0, 0, 255, 255}},
    {"blueviolet", {138, 43, 226, 255}},
    {"brown", {165, 42, 42, 255}},
    {"burlywood", {222, 184, 135, 255}},
    {"cadetblue", {95, 158, 160, 255}},
    {"chartreuse", {127, 255, 0, 255}},
    {"chocolate", {210, 105, 30, 255}},
    {"coral", {255, 127, 80, 255}},
    {"cornflowerblue", {100, 149, 237, 255}},
    {"cornsilk", {255, 248, 220, 255}},
    {"crimson", {220, 20, 60, 255}},
    {"cyan", {0, 255, 255, 255}},
    {"darkblue", {0, 0, 139, 255}},
    {"darkcyan", {0, 139, 139, 255}},
    {"darkgoldenrod", {184, 134, 11, 255}},
    {"darkgray", {169, 169, 169, 255}},
    {"darkgreen", {0, 100, 0, 255}},
    {"darkgrey", {169, 169, 169, 255}},
    {"darkkhaki", {189, 183, 107, 255}},
    {"darkmagenta", {139, 0, 139, 255}},
    {"darkolivegreen", {85, 107, 47, 255}},
    {"darkorange", {255, 140, 0, 255}},
    {"darkorchid", {153, 50, 204, 255}},
    {"darkred", {139, 0, 0, 255}},
    {"darksalmon", {233, 150, 122, 255}},
    {"darkseagreen", {143, 188, 143, 255}},
    {"darkslateblue", {72, 61, 139, 255}},
    {"darkslategray", {47, 79, 79, 255}},
    {"darkslategrey", {47, 79, 79, 255}},
    {"darkturquoise", {0, 206, 209, 255}},
    {"darkviolet", {148, 0, 211, 255}},
    {"deeppink", {255, 20, 147, 255}},
    {"deepskyblue", {0, 191, 255, 255}},
    {"dimgray", {105, 105, 105, 255}},
    {"dimgrey", {105, 105, 105, 255}},
    {"dodgerblue", {30, 144, 255, 255}},
    {"firebrick", {178, 34, 34, 255}},
    {"floralwhite", {255, 250, 240, 255}},
    {"forestgreen", {34, 139, 34, 255}},
    {"fuchsia", {255, 0, 255, 255}},
    {"gainsboro", {220, 220, 220, 255}},
    {"ghostwhite", {248, 248, 255, 255}},
    {"gold", {255, 215, 0, 255}},
    {"goldenrod", {218, 165, 32, 255}},
    {"gray", {128, 128, 128, 255}},
    {"green", {0, 128, 0, 255}},
    {"greenyellow", {173, 255, 47, 255}},
    {"grey", {128, 128, 128, 255}},
    {"honeydew", {240, 255, 240, 255}},
    {"hotpink", {255, 105, 180, 255}},
    {"indianred", {205, 92, 92, 255}},
    {"indigo", {75, 0, 130, 255}},
    {"ivory", {255, 255, 240, 255}},
    {"khaki", {240, 230, 140, 255}},
    {"lavender", {230, 230, 250, 255}},
    {"lavenderblush", {255, 240, 245, 255}},
    {"lawngreen", {124, 252, 0, 255}},
    {"lemonchiffon", {255, 250, 205, 255}},
    {"lightblue", {173, 216, 230, 255}},
    {"lightcoral", {240, 128, 128, 255}},
    {"lightcyan", {224, 255, 255, 255}},
    {"lightgoldenrodyellow", {250, 250, 210, 255}},
    {"lightgray", {211, 211, 211, 255}},
    {"lightgreen", {144, 238, 144, 255}},
    {"lightgrey", {211, 211, 211, 255}},
    {"lightpink", {255, 182, 193, 255}},
    {"lightsalmon", {255, 160, 122, 255}},
    {"lightseagreen", {32, 178, 170, 255}},
    {"lightskyblue", {135, 206, 250, 255}},
    {"lightslategray", {119, 136, 153, 255}},
    {"lightslategrey", {119, 136, 153, 255}},
    {"lightsteelblue", {176, 196, 222, 255}},
    {"lightyellow", {255, 255, 224, 255}},
    {"lime", {0, 255, 0, 255}},
    {"limegreen", {50, 205, 50, 255}},
    {"linen", {250, 240, 230, 255}},
    {"magenta", {255, 0, 255, 255}},
    {"maroon", {128, 0, 0, 255}},
    {"mediumaquamarine", {102, 205, 170, 255}},
    {"mediumblue", {0, 0, 205, 255}},
    {"mediumorchid", {186, 85, 211, 255}},
    {"mediumpurple", {147, 112, 219, 255}},
    {"mediumseagreen", {60, 179, 113, 255}},
    {"mediumslateblue", {123, 104, 238, 255}},
    {"mediumspringgreen", {0, 250, 154, 255}},
    {"mediumturquoise", {72, 209, 204, 255}},
    {"mediumvioletred", {199, 21, 133, 255}},
    {"midnightblue", {25, 25, 112, 255}},
    {"mintcream", {245, 255, 250, 255}},
    {"mistyrose", {255, 228, 225, 255}},
    {"moccasin", {255, 228, 181, 255}},
    {"navajowhite", {255, 222, 173, 255}},
    {"navy", {0, 0, 128, 255}},
    {"oldlace", {253, 245, 230, 255}},
    {"olive", {128, 128, 0, 255}},
    {"olivedrab", {107, 142, 35, 255}},
    {"orange", {255, 165, 0, 255}},
    {"orangered", {255, 69, 0, 255}},
    {"orchid", {218, 112, 214, 255}},
    {"palegoldenrod", {238, 232, 170, 255}},
    {"palegreen", {152, 251, 152, 255}},
    {"paleturquoise", {175, 238, 238, 255}},
    {"palevioletred", {219, 112, 147, 255}},
    {"papayawhip", {255, 239, 213, 255}},
    {"peachpuff", {255, 218, 185, 255}},
    {"peru", {205, 133, 63, 255}},
    {"pink", {255, 192, 203, 255}},
    {"plum", {221, 160, 221, 255}},
    {"powderblue", {176, 224, 230, 255}},
    {"purple", {128, 0, 128, 255}},
    {"red", {255, 0, 0, 255}},
    {"rosybrown", {188, 143, 143, 255}},
    {"royalblue", {65, 105, 225, 255}},
    {"saddlebrown", {139, 69, 19, 255}},
    {"salmon", {250, 128, 114, 255}},
    {"sandybrown", {244, 164, 96, 255}},
    {"seagreen", {46, 139, 87, 255}},
    {"seashell", {255, 245, 238, 255}},
    {"sienna", {160, 82, 45, 255}},
    {"silver", {192, 192, 192, 255}},
    {"skyblue", {135, 206, 235, 255}},
    {"slateblue", {106, 90, 205, 255}},
    {"slategray", {112, 128, 144, 255}},
    {"slategrey", {112, 128, 144, 255}},
    {"snow", {255, 250, 250, 255}},
    {"springgreen", {0, 255, 127, 255}},
    {"steelblue", {70, 130, 180, 255}},
    {"tan", {210, 180, 140, 255}},
    {"teal", {0, 128, 128, 255}},
    {"thistle", {216, 191, 216, 255}},
    {"tomato", {255, 99, 71, 255}},
    {"turquoise", {64, 224, 208, 255}},
    {"violet", {238, 130, 238, 255}},
    {"wheat", {245, 222, 179, 255}},
    {"white", {255, 255, 255, 255}},
    {"whitesmoke", {245, 245, 245, 255}},
    {"yellow", {255, 255, 0, 255}},
    {"yellowgreen", {154, 205, 50, 255}},
};
static const colorscheme_t svg = {"svg", svg_colors,
                                  sizeof(svg_colors) / sizeof(svg_colors[0])};

static const color_t ylgn3_colors[] = {
    {"1", {247, 252, 185, 255}},
    {"2", {173, 221, 142, 255}},
    {"3", {49, 163, 84, 255}},
};
static const colorscheme_t ylgn3 = {
    "ylgn3", ylgn3_colors, sizeof(ylgn3_colors) / sizeof(ylgn3_colors[0])};

static const color_t ylgn4_colors[] = {
    {"1", {255, 255, 204, 255}},
    {"2", {194, 230, 153, 255}},
    {"3", {120, 198, 121, 255}},
    {"4", {35, 132, 67, 255}},
};
static const colorscheme_t ylgn4 = {
    "ylgn4", ylgn4_colors, sizeof(ylgn4_colors) / sizeof(ylgn4_colors[0])};

static const color_t ylgn5_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {194, 230, 153, 255}},
    {"3", {120, 198, 121, 255}}, {"4", {49, 163, 84, 255}},
    {"5", {0, 104, 55, 255}},
};
static const colorscheme_t ylgn5 = {
    "ylgn5", ylgn5_colors, sizeof(ylgn5_colors) / sizeof(ylgn5_colors[0])};

static const color_t ylgn6_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {217, 240, 163, 255}},
    {"3", {173, 221, 142, 255}}, {"4", {120, 198, 121, 255}},
    {"5", {49, 163, 84, 255}},   {"6", {0, 104, 55, 255}},
};
static const colorscheme_t ylgn6 = {
    "ylgn6", ylgn6_colors, sizeof(ylgn6_colors) / sizeof(ylgn6_colors[0])};

static const color_t ylgn7_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {217, 240, 163, 255}},
    {"3", {173, 221, 142, 255}}, {"4", {120, 198, 121, 255}},
    {"5", {65, 171, 93, 255}},   {"6", {35, 132, 67, 255}},
    {"7", {0, 90, 50, 255}},
};
static const colorscheme_t ylgn7 = {
    "ylgn7", ylgn7_colors, sizeof(ylgn7_colors) / sizeof(ylgn7_colors[0])};

static const color_t ylgn8_colors[] = {
    {"1", {255, 255, 229, 255}}, {"2", {247, 252, 185, 255}},
    {"3", {217, 240, 163, 255}}, {"4", {173, 221, 142, 255}},
    {"5", {120, 198, 121, 255}}, {"6", {65, 171, 93, 255}},
    {"7", {35, 132, 67, 255}},   {"8", {0, 90, 50, 255}},
};
static const colorscheme_t ylgn8 = {
    "ylgn8", ylgn8_colors, sizeof(ylgn8_colors) / sizeof(ylgn8_colors[0])};

static const color_t ylgn9_colors[] = {
    {"1", {255, 255, 229, 255}}, {"2", {247, 252, 185, 255}},
    {"3", {217, 240, 163, 255}}, {"4", {173, 221, 142, 255}},
    {"5", {120, 198, 121, 255}}, {"6", {65, 171, 93, 255}},
    {"7", {35, 132, 67, 255}},   {"8", {0, 104, 55, 255}},
    {"9", {0, 69, 41, 255}},
};
static const colorscheme_t ylgn9 = {
    "ylgn9", ylgn9_colors, sizeof(ylgn9_colors) / sizeof(ylgn9_colors[0])};

static const color_t ylgnbu3_colors[] = {
    {"1", {237, 248, 177, 255}},
    {"2", {127, 205, 187, 255}},
    {"3", {44, 127, 184, 255}},
};
static const colorscheme_t ylgnbu3 = {"ylgnbu3", ylgnbu3_colors,
                                      sizeof(ylgnbu3_colors) /
                                          sizeof(ylgnbu3_colors[0])};

static const color_t ylgnbu4_colors[] = {
    {"1", {255, 255, 204, 255}},
    {"2", {161, 218, 180, 255}},
    {"3", {65, 182, 196, 255}},
    {"4", {34, 94, 168, 255}},
};
static const colorscheme_t ylgnbu4 = {"ylgnbu4", ylgnbu4_colors,
                                      sizeof(ylgnbu4_colors) /
                                          sizeof(ylgnbu4_colors[0])};

static const color_t ylgnbu5_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {161, 218, 180, 255}},
    {"3", {65, 182, 196, 255}},  {"4", {44, 127, 184, 255}},
    {"5", {37, 52, 148, 255}},
};
static const colorscheme_t ylgnbu5 = {"ylgnbu5", ylgnbu5_colors,
                                      sizeof(ylgnbu5_colors) /
                                          sizeof(ylgnbu5_colors[0])};

static const color_t ylgnbu6_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {199, 233, 180, 255}},
    {"3", {127, 205, 187, 255}}, {"4", {65, 182, 196, 255}},
    {"5", {44, 127, 184, 255}},  {"6", {37, 52, 148, 255}},
};
static const colorscheme_t ylgnbu6 = {"ylgnbu6", ylgnbu6_colors,
                                      sizeof(ylgnbu6_colors) /
                                          sizeof(ylgnbu6_colors[0])};

static const color_t ylgnbu7_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {199, 233, 180, 255}},
    {"3", {127, 205, 187, 255}}, {"4", {65, 182, 196, 255}},
    {"5", {29, 145, 192, 255}},  {"6", {34, 94, 168, 255}},
    {"7", {12, 44, 132, 255}},
};
static const colorscheme_t ylgnbu7 = {"ylgnbu7", ylgnbu7_colors,
                                      sizeof(ylgnbu7_colors) /
                                          sizeof(ylgnbu7_colors[0])};

static const color_t ylgnbu8_colors[] = {
    {"1", {255, 255, 217, 255}}, {"2", {237, 248, 177, 255}},
    {"3", {199, 233, 180, 255}}, {"4", {127, 205, 187, 255}},
    {"5", {65, 182, 196, 255}},  {"6", {29, 145, 192, 255}},
    {"7", {34, 94, 168, 255}},   {"8", {12, 44, 132, 255}},
};
static const colorscheme_t ylgnbu8 = {"ylgnbu8", ylgnbu8_colors,
                                      sizeof(ylgnbu8_colors) /
                                          sizeof(ylgnbu8_colors[0])};

static const color_t ylgnbu9_colors[] = {
    {"1", {255, 255, 217, 255}}, {"2", {237, 248, 177, 255}},
    {"3", {199, 233, 180, 255}}, {"4", {127, 205, 187, 255}},
    {"5", {65, 182, 196, 255}},  {"6", {29, 145, 192, 255}},
    {"7", {34, 94, 168, 255}},   {"8", {37, 52, 148, 255}},
    {"9", {8, 29, 88, 255}},
};
static const colorscheme_t ylgnbu9 = {"ylgnbu9", ylgnbu9_colors,
                                      sizeof(ylgnbu9_colors) /
                                          sizeof(ylgnbu9_colors[0])};

static const color_t ylorbr3_colors[] = {
    {"1", {255, 247, 188, 255}},
    {"2", {254, 196, 79, 255}},
    {"3", {217, 95, 14, 255}},
};
static const colorscheme_t ylorbr3 = {"ylorbr3", ylorbr3_colors,
                                      sizeof(ylorbr3_colors) /
                                          sizeof(ylorbr3_colors[0])};

static const color_t ylorbr4_colors[] = {
    {"1", {255, 255, 212, 255}},
    {"2", {254, 217, 142, 255}},
    {"3", {254, 153, 41, 255}},
    {"4", {204, 76, 2, 255}},
};
static const colorscheme_t ylorbr4 = {"ylorbr4", ylorbr4_colors,
                                      sizeof(ylorbr4_colors) /
                                          sizeof(ylorbr4_colors[0])};

static const color_t ylorbr5_colors[] = {
    {"1", {255, 255, 212, 255}}, {"2", {254, 217, 142, 255}},
    {"3", {254, 153, 41, 255}},  {"4", {217, 95, 14, 255}},
    {"5", {153, 52, 4, 255}},
};
static const colorscheme_t ylorbr5 = {"ylorbr5", ylorbr5_colors,
                                      sizeof(ylorbr5_colors) /
                                          sizeof(ylorbr5_colors[0])};

static const color_t ylorbr6_colors[] = {
    {"1", {255, 255, 212, 255}}, {"2", {254, 227, 145, 255}},
    {"3", {254, 196, 79, 255}},  {"4", {254, 153, 41, 255}},
    {"5", {217, 95, 14, 255}},   {"6", {153, 52, 4, 255}},
};
static const colorscheme_t ylorbr6 = {"ylorbr6", ylorbr6_colors,
                                      sizeof(ylorbr6_colors) /
                                          sizeof(ylorbr6_colors[0])};

static const color_t ylorbr7_colors[] = {
    {"1", {255, 255, 212, 255}}, {"2", {254, 227, 145, 255}},
    {"3", {254, 196, 79, 255}},  {"4", {254, 153, 41, 255}},
    {"5", {236, 112, 20, 255}},  {"6", {204, 76, 2, 255}},
    {"7", {140, 45, 4, 255}},
};
static const colorscheme_t ylorbr7 = {"ylorbr7", ylorbr7_colors,
                                      sizeof(ylorbr7_colors) /
                                          sizeof(ylorbr7_colors[0])};

static const color_t ylorbr8_colors[] = {
    {"1", {255, 255, 229, 255}}, {"2", {255, 247, 188, 255}},
    {"3", {254, 227, 145, 255}}, {"4", {254, 196, 79, 255}},
    {"5", {254, 153, 41, 255}},  {"6", {236, 112, 20, 255}},
    {"7", {204, 76, 2, 255}},    {"8", {140, 45, 4, 255}},
};
static const colorscheme_t ylorbr8 = {"ylorbr8", ylorbr8_colors,
                                      sizeof(ylorbr8_colors) /
                                          sizeof(ylorbr8_colors[0])};

static const color_t ylorbr9_colors[] = {
    {"1", {255, 255, 229, 255}}, {"2", {255, 247, 188, 255}},
    {"3", {254, 227, 145, 255}}, {"4", {254, 196, 79, 255}},
    {"5", {254, 153, 41, 255}},  {"6", {236, 112, 20, 255}},
    {"7", {204, 76, 2, 255}},    {"8", {153, 52, 4, 255}},
    {"9", {102, 37, 6, 255}},
};
static const colorscheme_t ylorbr9 = {"ylorbr9", ylorbr9_colors,
                                      sizeof(ylorbr9_colors) /
                                          sizeof(ylorbr9_colors[0])};

static const color_t ylorrd3_colors[] = {
    {"1", {255, 237, 160, 255}},
    {"2", {254, 178, 76, 255}},
    {"3", {240, 59, 32, 255}},
};
static const colorscheme_t ylorrd3 = {"ylorrd3", ylorrd3_colors,
                                      sizeof(ylorrd3_colors) /
                                          sizeof(ylorrd3_colors[0])};

static const color_t ylorrd4_colors[] = {
    {"1", {255, 255, 178, 255}},
    {"2", {254, 204, 92, 255}},
    {"3", {253, 141, 60, 255}},
    {"4", {227, 26, 28, 255}},
};
static const colorscheme_t ylorrd4 = {"ylorrd4", ylorrd4_colors,
                                      sizeof(ylorrd4_colors) /
                                          sizeof(ylorrd4_colors[0])};

static const color_t ylorrd5_colors[] = {
    {"1", {255, 255, 178, 255}}, {"2", {254, 204, 92, 255}},
    {"3", {253, 141, 60, 255}},  {"4", {240, 59, 32, 255}},
    {"5", {189, 0, 38, 255}},
};
static const colorscheme_t ylorrd5 = {"ylorrd5", ylorrd5_colors,
                                      sizeof(ylorrd5_colors) /
                                          sizeof(ylorrd5_colors[0])};

static const color_t ylorrd6_colors[] = {
    {"1", {255, 255, 178, 255}}, {"2", {254, 217, 118, 255}},
    {"3", {254, 178, 76, 255}},  {"4", {253, 141, 60, 255}},
    {"5", {240, 59, 32, 255}},   {"6", {189, 0, 38, 255}},
};
static const colorscheme_t ylorrd6 = {"ylorrd6", ylorrd6_colors,
                                      sizeof(ylorrd6_colors) /
                                          sizeof(ylorrd6_colors[0])};

static const color_t ylorrd7_colors[] = {
    {"1", {255, 255, 178, 255}}, {"2", {254, 217, 118, 255}},
    {"3", {254, 178, 76, 255}},  {"4", {253, 141, 60, 255}},
    {"5", {252, 78, 42, 255}},   {"6", {227, 26, 28, 255}},
    {"7", {177, 0, 38, 255}},
};
static const colorscheme_t ylorrd7 = {"ylorrd7", ylorrd7_colors,
                                      sizeof(ylorrd7_colors) /
                                          sizeof(ylorrd7_colors[0])};

static const color_t ylorrd8_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {255, 237, 160, 255}},
    {"3", {254, 217, 118, 255}}, {"4", {254, 178, 76, 255}},
    {"5", {253, 141, 60, 255}},  {"6", {252, 78, 42, 255}},
    {"7", {227, 26, 28, 255}},   {"8", {177, 0, 38, 255}},
};
static const colorscheme_t ylorrd8 = {"ylorrd8", ylorrd8_colors,
                                      sizeof(ylorrd8_colors) /
                                          sizeof(ylorrd8_colors[0])};

static const color_t ylorrd9_colors[] = {
    {"1", {255, 255, 204, 255}}, {"2", {255, 237, 160, 255}},
    {"3", {254, 217, 118, 255}}, {"4", {254, 178, 76, 255}},
    {"5", {253, 141, 60, 255}},  {"6", {252, 78, 42, 255}},
    {"7", {227, 26, 28, 255}},   {"8", {189, 0, 38, 255}},
    {"9", {128, 0, 38, 255}},
};
static const colorscheme_t ylorrd9 = {"ylorrd9", ylorrd9_colors,
                                      sizeof(ylorrd9_colors) /
                                          sizeof(ylorrd9_colors[0])};

static const color_t X11_colors[] = {
    {"aliceblue", {240, 248, 255, 255}},
    {"antiquewhite", {250, 235, 215, 255}},
    {"antiquewhite1", {255, 239, 219, 255}},
    {"antiquewhite2", {238, 223, 204, 255}},
    {"antiquewhite3", {205, 192, 176, 255}},
    {"antiquewhite4", {139, 131, 120, 255}},
    {"aqua", {0, 255, 255, 255}},
    {"aquamarine", {127, 255, 212, 255}},
    {"aquamarine1", {127, 255, 212, 255}},
    {"aquamarine2", {118, 238, 198, 255}},
    {"aquamarine3", {102, 205, 170, 255}},
    {"aquamarine4", {69, 139, 116, 255}},
    {"azure", {240, 255, 255, 255}},
    {"azure1", {240, 255, 255, 255}},
    {"azure2", {224, 238, 238, 255}},
    {"azure3", {193, 205, 205, 255}},
    {"azure4", {131, 139, 139, 255}},
    {"beige", {245, 245, 220, 255}},
    {"bisque", {255, 228, 196, 255}},
    {"bisque1", {255, 228, 196, 255}},
    {"bisque2", {238, 213, 183, 255}},
    {"bisque3", {205, 183, 158, 255}},
    {"bisque4", {139, 125, 107, 255}},
    {"black", {0, 0, 0, 255}},
    {"blanchedalmond", {255, 235, 205, 255}},
    {"blue", {0, 0, 255, 255}},
    {"blue1", {0, 0, 255, 255}},
    {"blue2", {0, 0, 238, 255}},
    {"blue3", {0, 0, 205, 255}},
    {"blue4", {0, 0, 139, 255}},
    {"blueviolet", {138, 43, 226, 255}},
    {"brown", {165, 42, 42, 255}},
    {"brown1", {255, 64, 64, 255}},
    {"brown2", {238, 59, 59, 255}},
    {"brown3", {205, 51, 51, 255}},
    {"brown4", {139, 35, 35, 255}},
    {"burlywood", {222, 184, 135, 255}},
    {"burlywood1", {255, 211, 155, 255}},
    {"burlywood2", {238, 197, 145, 255}},
    {"burlywood3", {205, 170, 125, 255}},
    {"burlywood4", {139, 115, 85, 255}},
    {"cadetblue", {95, 158, 160, 255}},
    {"cadetblue1", {152, 245, 255, 255}},
    {"cadetblue2", {142, 229, 238, 255}},
    {"cadetblue3", {122, 197, 205, 255}},
    {"cadetblue4", {83, 134, 139, 255}},
    {"chartreuse", {127, 255, 0, 255}},
    {"chartreuse1", {127, 255, 0, 255}},
    {"chartreuse2", {118, 238, 0, 255}},
    {"chartreuse3", {102, 205, 0, 255}},
    {"chartreuse4", {69, 139, 0, 255}},
    {"chocolate", {210, 105, 30, 255}},
    {"chocolate1", {255, 127, 36, 255}},
    {"chocolate2", {238, 118, 33, 255}},
    {"chocolate3", {205, 102, 29, 255}},
    {"chocolate4", {139, 69, 19, 255}},
    {"coral", {255, 127, 80, 255}},
    {"coral1", {255, 114, 86, 255}},
    {"coral2", {238, 106, 80, 255}},
    {"coral3", {205, 91, 69, 255}},
    {"coral4", {139, 62, 47, 255}},
    {"cornflowerblue", {100, 149, 237, 255}},
    {"cornsilk", {255, 248, 220, 255}},
    {"cornsilk1", {255, 248, 220, 255}},
    {"cornsilk2", {238, 232, 205, 255}},
    {"cornsilk3", {205, 200, 177, 255}},
    {"cornsilk4", {139, 136, 120, 255}},
    {"crimson", {220, 20, 60, 255}},
    {"cyan", {0, 255, 255, 255}},
    {"cyan1", {0, 255, 255, 255}},
    {"cyan2", {0, 238, 238, 255}},
    {"cyan3", {0, 205, 205, 255}},
    {"cyan4", {0, 139, 139, 255}},
    {"darkblue", {0, 0, 139, 255}},
    {"darkcyan", {0, 139, 139, 255}},
    {"darkgoldenrod", {184, 134, 11, 255}},
    {"darkgoldenrod1", {255, 185, 15, 255}},
    {"darkgoldenrod2", {238, 173, 14, 255}},
    {"darkgoldenrod3", {205, 149, 12, 255}},
    {"darkgoldenrod4", {139, 101, 8, 255}},
    {"darkgray", {169, 169, 169, 255}},
    {"darkgreen", {0, 100, 0, 255}},
    {"darkgrey", {169, 169, 169, 255}},
    {"darkkhaki", {189, 183, 107, 255}},
    {"darkmagenta", {139, 0, 139, 255}},
    {"darkolivegreen", {85, 107, 47, 255}},
    {"darkolivegreen1", {202, 255, 112, 255}},
    {"darkolivegreen2", {188, 238, 104, 255}},
    {"darkolivegreen3", {162, 205, 90, 255}},
    {"darkolivegreen4", {110, 139, 61, 255}},
    {"darkorange", {255, 140, 0, 255}},
    {"darkorange1", {255, 127, 0, 255}},
    {"darkorange2", {238, 118, 0, 255}},
    {"darkorange3", {205, 102, 0, 255}},
    {"darkorange4", {139, 69, 0, 255}},
    {"darkorchid", {153, 50, 204, 255}},
    {"darkorchid1", {191, 62, 255, 255}},
    {"darkorchid2", {178, 58, 238, 255}},
    {"darkorchid3", {154, 50, 205, 255}},
    {"darkorchid4", {104, 34, 139, 255}},
    {"darkred", {139, 0, 0, 255}},
    {"darksalmon", {233, 150, 122, 255}},
    {"darkseagreen", {143, 188, 143, 255}},
    {"darkseagreen1", {193, 255, 193, 255}},
    {"darkseagreen2", {180, 238, 180, 255}},
    {"darkseagreen3", {155, 205, 155, 255}},
    {"darkseagreen4", {105, 139, 105, 255}},
    {"darkslateblue", {72, 61, 139, 255}},
    {"darkslategray", {47, 79, 79, 255}},
    {"darkslategray1", {151, 255, 255, 255}},
    {"darkslategray2", {141, 238, 238, 255}},
    {"darkslategray3", {121, 205, 205, 255}},
    {"darkslategray4", {82, 139, 139, 255}},
    {"darkslategrey", {47, 79, 79, 255}},
    {"darkturquoise", {0, 206, 209, 255}},
    {"darkviolet", {148, 0, 211, 255}},
    {"deeppink", {255, 20, 147, 255}},
    {"deeppink1", {255, 20, 147, 255}},
    {"deeppink2", {238, 18, 137, 255}},
    {"deeppink3", {205, 16, 118, 255}},
    {"deeppink4", {139, 10, 80, 255}},
    {"deepskyblue", {0, 191, 255, 255}},
    {"deepskyblue1", {0, 191, 255, 255}},
    {"deepskyblue2", {0, 178, 238, 255}},
    {"deepskyblue3", {0, 154, 205, 255}},
    {"deepskyblue4", {0, 104, 139, 255}},
    {"dimgray", {105, 105, 105, 255}},
    {"dimgrey", {105, 105, 105, 255}},
    {"dodgerblue", {30, 144, 255, 255}},
    {"dodgerblue1", {30, 144, 255, 255}},
    {"dodgerblue2", {28, 134, 238, 255}},
    {"dodgerblue3", {24, 116, 205, 255}},
    {"dodgerblue4", {16, 78, 139, 255}},
    {"firebrick", {178, 34, 34, 255}},
    {"firebrick1", {255, 48, 48, 255}},
    {"firebrick2", {238, 44, 44, 255}},
    {"firebrick3", {205, 38, 38, 255}},
    {"firebrick4", {139, 26, 26, 255}},
    {"floralwhite", {255, 250, 240, 255}},
    {"forestgreen", {34, 139, 34, 255}},
    {"fuchsia", {255, 0, 255, 255}},
    {"gainsboro", {220, 220, 220, 255}},
    {"ghostwhite", {248, 248, 255, 255}},
    {"gold", {255, 215, 0, 255}},
    {"gold1", {255, 215, 0, 255}},
    {"gold2", {238, 201, 0, 255}},
    {"gold3", {205, 173, 0, 255}},
    {"gold4", {139, 117, 0, 255}},
    {"goldenrod", {218, 165, 32, 255}},
    {"goldenrod1", {255, 193, 37, 255}},
    {"goldenrod2", {238, 180, 34, 255}},
    {"goldenrod3", {205, 155, 29, 255}},
    {"goldenrod4", {139, 105, 20, 255}},
    {"gray", {192, 192, 192, 255}},
    {"gray0", {0, 0, 0, 255}},
    {"gray1", {3, 3, 3, 255}},
    {"gray10", {26, 26, 26, 255}},
    {"gray100", {255, 255, 255, 255}},
    {"gray11", {28, 28, 28, 255}},
    {"gray12", {31, 31, 31, 255}},
    {"gray13", {33, 33, 33, 255}},
    {"gray14", {36, 36, 36, 255}},
    {"gray15", {38, 38, 38, 255}},
    {"gray16", {41, 41, 41, 255}},
    {"gray17", {43, 43, 43, 255}},
    {"gray18", {46, 46, 46, 255}},
    {"gray19", {48, 48, 48, 255}},
    {"gray2", {5, 5, 5, 255}},
    {"gray20", {51, 51, 51, 255}},
    {"gray21", {54, 54, 54, 255}},
    {"gray22", {56, 56, 56, 255}},
    {"gray23", {59, 59, 59, 255}},
    {"gray24", {61, 61, 61, 255}},
    {"gray25", {64, 64, 64, 255}},
    {"gray26", {66, 66, 66, 255}},
    {"gray27", {69, 69, 69, 255}},
    {"gray28", {71, 71, 71, 255}},
    {"gray29", {74, 74, 74, 255}},
    {"gray3", {8, 8, 8, 255}},
    {"gray30", {77, 77, 77, 255}},
    {"gray31", {79, 79, 79, 255}},
    {"gray32", {82, 82, 82, 255}},
    {"gray33", {84, 84, 84, 255}},
    {"gray34", {87, 87, 87, 255}},
    {"gray35", {89, 89, 89, 255}},
    {"gray36", {92, 92, 92, 255}},
    {"gray37", {94, 94, 94, 255}},
    {"gray38", {97, 97, 97, 255}},
    {"gray39", {99, 99, 99, 255}},
    {"gray4", {10, 10, 10, 255}},
    {"gray40", {102, 102, 102, 255}},
    {"gray41", {105, 105, 105, 255}},
    {"gray42", {107, 107, 107, 255}},
    {"gray43", {110, 110, 110, 255}},
    {"gray44", {112, 112, 112, 255}},
    {"gray45", {115, 115, 115, 255}},
    {"gray46", {117, 117, 117, 255}},
    {"gray47", {120, 120, 120, 255}},
    {"gray48", {122, 122, 122, 255}},
    {"gray49", {125, 125, 125, 255}},
    {"gray5", {13, 13, 13, 255}},
    {"gray50", {127, 127, 127, 255}},
    {"gray51", {130, 130, 130, 255}},
    {"gray52", {133, 133, 133, 255}},
    {"gray53", {135, 135, 135, 255}},
    {"gray54", {138, 138, 138, 255}},
    {"gray55", {140, 140, 140, 255}},
    {"gray56", {143, 143, 143, 255}},
    {"gray57", {145, 145, 145, 255}},
    {"gray58", {148, 148, 148, 255}},
    {"gray59", {150, 150, 150, 255}},
    {"gray6", {15, 15, 15, 255}},
    {"gray60", {153, 153, 153, 255}},
    {"gray61", {156, 156, 156, 255}},
    {"gray62", {158, 158, 158, 255}},
    {"gray63", {161, 161, 161, 255}},
    {"gray64", {163, 163, 163, 255}},
    {"gray65", {166, 166, 166, 255}},
    {"gray66", {168, 168, 168, 255}},
    {"gray67", {171, 171, 171, 255}},
    {"gray68", {173, 173, 173, 255}},
    {"gray69", {176, 176, 176, 255}},
    {"gray7", {18, 18, 18, 255}},
    {"gray70", {179, 179, 179, 255}},
    {"gray71", {181, 181, 181, 255}},
    {"gray72", {184, 184, 184, 255}},
    {"gray73", {186, 186, 186, 255}},
    {"gray74", {189, 189, 189, 255}},
    {"gray75", {191, 191, 191, 255}},
    {"gray76", {194, 194, 194, 255}},
    {"gray77", {196, 196, 196, 255}},
    {"gray78", {199, 199, 199, 255}},
    {"gray79", {201, 201, 201, 255}},
    {"gray8", {20, 20, 20, 255}},
    {"gray80", {204, 204, 204, 255}},
    {"gray81", {207, 207, 207, 255}},
    {"gray82", {209, 209, 209, 255}},
    {"gray83", {212, 212, 212, 255}},
    {"gray84", {214, 214, 214, 255}},
    {"gray85", {217, 217, 217, 255}},
    {"gray86", {219, 219, 219, 255}},
    {"gray87", {222, 222, 222, 255}},
    {"gray88", {224, 224, 224, 255}},
    {"gray89", {227, 227, 227, 255}},
    {"gray9", {23, 23, 23, 255}},
    {"gray90", {229, 229, 229, 255}},
    {"gray91", {232, 232, 232, 255}},
    {"gray92", {235, 235, 235, 255}},
    {"gray93", {237, 237, 237, 255}},
    {"gray94", {240, 240, 240, 255}},
    {"gray95", {242, 242, 242, 255}},
    {"gray96", {245, 245, 245, 255}},
    {"gray97", {247, 247, 247, 255}},
    {"gray98", {250, 250, 250, 255}},
    {"gray99", {252, 252, 252, 255}},
    {"green", {0, 255, 0, 255}},
    {"green1", {0, 255, 0, 255}},
    {"green2", {0, 238, 0, 255}},
    {"green3", {0, 205, 0, 255}},
    {"green4", {0, 139, 0, 255}},
    {"greenyellow", {173, 255, 47, 255}},
    {"grey", {192, 192, 192, 255}},
    {"grey0", {0, 0, 0, 255}},
    {"grey1", {3, 3, 3, 255}},
    {"grey10", {26, 26, 26, 255}},
    {"grey100", {255, 255, 255, 255}},
    {"grey11", {28, 28, 28, 255}},
    {"grey12", {31, 31, 31, 255}},
    {"grey13", {33, 33, 33, 255}},
    {"grey14", {36, 36, 36, 255}},
    {"grey15", {38, 38, 38, 255}},
    {"grey16", {41, 41, 41, 255}},
    {"grey17", {43, 43, 43, 255}},
    {"grey18", {46, 46, 46, 255}},
    {"grey19", {48, 48, 48, 255}},
    {"grey2", {5, 5, 5, 255}},
    {"grey20", {51, 51, 51, 255}},
    {"grey21", {54, 54, 54, 255}},
    {"grey22", {56, 56, 56, 255}},
    {"grey23", {59, 59, 59, 255}},
    {"grey24", {61, 61, 61, 255}},
    {"grey25", {64, 64, 64, 255}},
    {"grey26", {66, 66, 66, 255}},
    {"grey27", {69, 69, 69, 255}},
    {"grey28", {71, 71, 71, 255}},
    {"grey29", {74, 74, 74, 255}},
    {"grey3", {8, 8, 8, 255}},
    {"grey30", {77, 77, 77, 255}},
    {"grey31", {79, 79, 79, 255}},
    {"grey32", {82, 82, 82, 255}},
    {"grey33", {84, 84, 84, 255}},
    {"grey34", {87, 87, 87, 255}},
    {"grey35", {89, 89, 89, 255}},
    {"grey36", {92, 92, 92, 255}},
    {"grey37", {94, 94, 94, 255}},
    {"grey38", {97, 97, 97, 255}},
    {"grey39", {99, 99, 99, 255}},
    {"grey4", {10, 10, 10, 255}},
    {"grey40", {102, 102, 102, 255}},
    {"grey41", {105, 105, 105, 255}},
    {"grey42", {107, 107, 107, 255}},
    {"grey43", {110, 110, 110, 255}},
    {"grey44", {112, 112, 112, 255}},
    {"grey45", {115, 115, 115, 255}},
    {"grey46", {117, 117, 117, 255}},
    {"grey47", {120, 120, 120, 255}},
    {"grey48", {122, 122, 122, 255}},
    {"grey49", {125, 125, 125, 255}},
    {"grey5", {13, 13, 13, 255}},
    {"grey50", {127, 127, 127, 255}},
    {"grey51", {130, 130, 130, 255}},
    {"grey52", {133, 133, 133, 255}},
    {"grey53", {135, 135, 135, 255}},
    {"grey54", {138, 138, 138, 255}},
    {"grey55", {140, 140, 140, 255}},
    {"grey56", {143, 143, 143, 255}},
    {"grey57", {145, 145, 145, 255}},
    {"grey58", {148, 148, 148, 255}},
    {"grey59", {150, 150, 150, 255}},
    {"grey6", {15, 15, 15, 255}},
    {"grey60", {153, 153, 153, 255}},
    {"grey61", {156, 156, 156, 255}},
    {"grey62", {158, 158, 158, 255}},
    {"grey63", {161, 161, 161, 255}},
    {"grey64", {163, 163, 163, 255}},
    {"grey65", {166, 166, 166, 255}},
    {"grey66", {168, 168, 168, 255}},
    {"grey67", {171, 171, 171, 255}},
    {"grey68", {173, 173, 173, 255}},
    {"grey69", {176, 176, 176, 255}},
    {"grey7", {18, 18, 18, 255}},
    {"grey70", {179, 179, 179, 255}},
    {"grey71", {181, 181, 181, 255}},
    {"grey72", {184, 184, 184, 255}},
    {"grey73", {186, 186, 186, 255}},
    {"grey74", {189, 189, 189, 255}},
    {"grey75", {191, 191, 191, 255}},
    {"grey76", {194, 194, 194, 255}},
    {"grey77", {196, 196, 196, 255}},
    {"grey78", {199, 199, 199, 255}},
    {"grey79", {201, 201, 201, 255}},
    {"grey8", {20, 20, 20, 255}},
    {"grey80", {204, 204, 204, 255}},
    {"grey81", {207, 207, 207, 255}},
    {"grey82", {209, 209, 209, 255}},
    {"grey83", {212, 212, 212, 255}},
    {"grey84", {214, 214, 214, 255}},
    {"grey85", {217, 217, 217, 255}},
    {"grey86", {219, 219, 219, 255}},
    {"grey87", {222, 222, 222, 255}},
    {"grey88", {224, 224, 224, 255}},
    {"grey89", {227, 227, 227, 255}},
    {"grey9", {23, 23, 23, 255}},
    {"grey90", {229, 229, 229, 255}},
    {"grey91", {232, 232, 232, 255}},
    {"grey92", {235, 235, 235, 255}},
    {"grey93", {237, 237, 237, 255}},
    {"grey94", {240, 240, 240, 255}},
    {"grey95", {242, 242, 242, 255}},
    {"grey96", {245, 245, 245, 255}},
    {"grey97", {247, 247, 247, 255}},
    {"grey98", {250, 250, 250, 255}},
    {"grey99", {252, 252, 252, 255}},
    {"honeydew", {240, 255, 240, 255}},
    {"honeydew1", {240, 255, 240, 255}},
    {"honeydew2", {224, 238, 224, 255}},
    {"honeydew3", {193, 205, 193, 255}},
    {"honeydew4", {131, 139, 131, 255}},
    {"hotpink", {255, 105, 180, 255}},
    {"hotpink1", {255, 110, 180, 255}},
    {"hotpink2", {238, 106, 167, 255}},
    {"hotpink3", {205, 96, 144, 255}},
    {"hotpink4", {139, 58, 98, 255}},
    {"indianred", {205, 92, 92, 255}},
    {"indianred1", {255, 106, 106, 255}},
    {"indianred2", {238, 99, 99, 255}},
    {"indianred3", {205, 85, 85, 255}},
    {"indianred4", {139, 58, 58, 255}},
    {"indigo", {75, 0, 130, 255}},
    {"invis", {255, 255, 254, 0}},
    {"ivory", {255, 255, 240, 255}},
    {"ivory1", {255, 255, 240, 255}},
    {"ivory2", {238, 238, 224, 255}},
    {"ivory3", {205, 205, 193, 255}},
    {"ivory4", {139, 139, 131, 255}},
    {"khaki", {240, 230, 140, 255}},
    {"khaki1", {255, 246, 143, 255}},
    {"khaki2", {238, 230, 133, 255}},
    {"khaki3", {205, 198, 115, 255}},
    {"khaki4", {139, 134, 78, 255}},
    {"lavender", {230, 230, 250, 255}},
    {"lavenderblush", {255, 240, 245, 255}},
    {"lavenderblush1", {255, 240, 245, 255}},
    {"lavenderblush2", {238, 224, 229, 255}},
    {"lavenderblush3", {205, 193, 197, 255}},
    {"lavenderblush4", {139, 131, 134, 255}},
    {"lawngreen", {124, 252, 0, 255}},
    {"lemonchiffon", {255, 250, 205, 255}},
    {"lemonchiffon1", {255, 250, 205, 255}},
    {"lemonchiffon2", {238, 233, 191, 255}},
    {"lemonchiffon3", {205, 201, 165, 255}},
    {"lemonchiffon4", {139, 137, 112, 255}},
    {"lightblue", {173, 216, 230, 255}},
    {"lightblue1", {191, 239, 255, 255}},
    {"lightblue2", {178, 223, 238, 255}},
    {"lightblue3", {154, 192, 205, 255}},
    {"lightblue4", {104, 131, 139, 255}},
    {"lightcoral", {240, 128, 128, 255}},
    {"lightcyan", {224, 255, 255, 255}},
    {"lightcyan1", {224, 255, 255, 255}},
    {"lightcyan2", {209, 238, 238, 255}},
    {"lightcyan3", {180, 205, 205, 255}},
    {"lightcyan4", {122, 139, 139, 255}},
    {"lightgoldenrod", {238, 221, 130, 255}},
    {"lightgoldenrod1", {255, 236, 139, 255}},
    {"lightgoldenrod2", {238, 220, 130, 255}},
    {"lightgoldenrod3", {205, 190, 112, 255}},
    {"lightgoldenrod4", {139, 129, 76, 255}},
    {"lightgoldenrodyellow", {250, 250, 210, 255}},
    {"lightgray", {211, 211, 211, 255}},
    {"lightgreen", {144, 238, 144, 255}},
    {"lightgrey", {211, 211, 211, 255}},
    {"lightpink", {255, 182, 193, 255}},
    {"lightpink1", {255, 174, 185, 255}},
    {"lightpink2", {238, 162, 173, 255}},
    {"lightpink3", {205, 140, 149, 255}},
    {"lightpink4", {139, 95, 101, 255}},
    {"lightsalmon", {255, 160, 122, 255}},
    {"lightsalmon1", {255, 160, 122, 255}},
    {"lightsalmon2", {238, 149, 114, 255}},
    {"lightsalmon3", {205, 129, 98, 255}},
    {"lightsalmon4", {139, 87, 66, 255}},
    {"lightseagreen", {32, 178, 170, 255}},
    {"lightskyblue", {135, 206, 250, 255}},
    {"lightskyblue1", {176, 226, 255, 255}},
    {"lightskyblue2", {164, 211, 238, 255}},
    {"lightskyblue3", {141, 182, 205, 255}},
    {"lightskyblue4", {96, 123, 139, 255}},
    {"lightslateblue", {132, 112, 255, 255}},
    {"lightslategray", {119, 136, 153, 255}},
    {"lightslategrey", {119, 136, 153, 255}},
    {"lightsteelblue", {176, 196, 222, 255}},
    {"lightsteelblue1", {202, 225, 255, 255}},
    {"lightsteelblue2", {188, 210, 238, 255}},
    {"lightsteelblue3", {162, 181, 205, 255}},
    {"lightsteelblue4", {110, 123, 139, 255}},
    {"lightyellow", {255, 255, 224, 255}},
    {"lightyellow1", {255, 255, 224, 255}},
    {"lightyellow2", {238, 238, 209, 255}},
    {"lightyellow3", {205, 205, 180, 255}},
    {"lightyellow4", {139, 139, 122, 255}},
    {"lime", {0, 255, 0, 255}},
    {"limegreen", {50, 205, 50, 255}},
    {"linen", {250, 240, 230, 255}},
    {"magenta", {255, 0, 255, 255}},
    {"magenta1", {255, 0, 255, 255}},
    {"magenta2", {238, 0, 238, 255}},
    {"magenta3", {205, 0, 205, 255}},
    {"magenta4", {139, 0, 139, 255}},
    {"maroon", {176, 48, 96, 255}},
    {"maroon1", {255, 52, 179, 255}},
    {"maroon2", {238, 48, 167, 255}},
    {"maroon3", {205, 41, 144, 255}},
    {"maroon4", {139, 28, 98, 255}},
    {"mediumaquamarine", {102, 205, 170, 255}},
    {"mediumblue", {0, 0, 205, 255}},
    {"mediumorchid", {186, 85, 211, 255}},
    {"mediumorchid1", {224, 102, 255, 255}},
    {"mediumorchid2", {209, 95, 238, 255}},
    {"mediumorchid3", {180, 82, 205, 255}},
    {"mediumorchid4", {122, 55, 139, 255}},
    {"mediumpurple", {147, 112, 219, 255}},
    {"mediumpurple1", {171, 130, 255, 255}},
    {"mediumpurple2", {159, 121, 238, 255}},
    {"mediumpurple3", {137, 104, 205, 255}},
    {"mediumpurple4", {93, 71, 139, 255}},
    {"mediumseagreen", {60, 179, 113, 255}},
    {"mediumslateblue", {123, 104, 238, 255}},
    {"mediumspringgreen", {0, 250, 154, 255}},
    {"mediumturquoise", {72, 209, 204, 255}},
    {"mediumvioletred", {199, 21, 133, 255}},
    {"midnightblue", {25, 25, 112, 255}},
    {"mintcream", {245, 255, 250, 255}},
    {"mistyrose", {255, 228, 225, 255}},
    {"mistyrose1", {255, 228, 225, 255}},
    {"mistyrose2", {238, 213, 210, 255}},
    {"mistyrose3", {205, 183, 181, 255}},
    {"mistyrose4", {139, 125, 123, 255}},
    {"moccasin", {255, 228, 181, 255}},
    {"navajowhite", {255, 222, 173, 255}},
    {"navajowhite1", {255, 222, 173, 255}},
    {"navajowhite2", {238, 207, 161, 255}},
    {"navajowhite3", {205, 179, 139, 255}},
    {"navajowhite4", {139, 121, 94, 255}},
    {"navy", {0, 0, 128, 255}},
    {"navyblue", {0, 0, 128, 255}},
    {"none", {255, 255, 254, 0}},
    {"oldlace", {253, 245, 230, 255}},
    {"olive", {128, 128, 0, 255}},
    {"olivedrab", {107, 142, 35, 255}},
    {"olivedrab1", {192, 255, 62, 255}},
    {"olivedrab2", {179, 238, 58, 255}},
    {"olivedrab3", {154, 205, 50, 255}},
    {"olivedrab4", {105, 139, 34, 255}},
    {"orange", {255, 165, 0, 255}},
    {"orange1", {255, 165, 0, 255}},
    {"orange2", {238, 154, 0, 255}},
    {"orange3", {205, 133, 0, 255}},
    {"orange4", {139, 90, 0, 255}},
    {"orangered", {255, 69, 0, 255}},
    {"orangered1", {255, 69, 0, 255}},
    {"orangered2", {238, 64, 0, 255}},
    {"orangered3", {205, 55, 0, 255}},
    {"orangered4", {139, 37, 0, 255}},
    {"orchid", {218, 112, 214, 255}},
    {"orchid1", {255, 131, 250, 255}},
    {"orchid2", {238, 122, 233, 255}},
    {"orchid3", {205, 105, 201, 255}},
    {"orchid4", {139, 71, 137, 255}},
    {"palegoldenrod", {238, 232, 170, 255}},
    {"palegreen", {152, 251, 152, 255}},
    {"palegreen1", {154, 255, 154, 255}},
    {"palegreen2", {144, 238, 144, 255}},
    {"palegreen3", {124, 205, 124, 255}},
    {"palegreen4", {84, 139, 84, 255}},
    {"paleturquoise", {175, 238, 238, 255}},
    {"paleturquoise1", {187, 255, 255, 255}},
    {"paleturquoise2", {174, 238, 238, 255}},
    {"paleturquoise3", {150, 205, 205, 255}},
    {"paleturquoise4", {102, 139, 139, 255}},
    {"palevioletred", {219, 112, 147, 255}},
    {"palevioletred1", {255, 130, 171, 255}},
    {"palevioletred2", {238, 121, 159, 255}},
    {"palevioletred3", {205, 104, 137, 255}},
    {"palevioletred4", {139, 71, 93, 255}},
    {"papayawhip", {255, 239, 213, 255}},
    {"peachpuff", {255, 218, 185, 255}},
    {"peachpuff1", {255, 218, 185, 255}},
    {"peachpuff2", {238, 203, 173, 255}},
    {"peachpuff3", {205, 175, 149, 255}},
    {"peachpuff4", {139, 119, 101, 255}},
    {"peru", {205, 133, 63, 255}},
    {"pink", {255, 192, 203, 255}},
    {"pink1", {255, 181, 197, 255}},
    {"pink2", {238, 169, 184, 255}},
    {"pink3", {205, 145, 158, 255}},
    {"pink4", {139, 99, 108, 255}},
    {"plum", {221, 160, 221, 255}},
    {"plum1", {255, 187, 255, 255}},
    {"plum2", {238, 174, 238, 255}},
    {"plum3", {205, 150, 205, 255}},
    {"plum4", {139, 102, 139, 255}},
    {"powderblue", {176, 224, 230, 255}},
    {"purple", {160, 32, 240, 255}},
    {"purple1", {155, 48, 255, 255}},
    {"purple2", {145, 44, 238, 255}},
    {"purple3", {125, 38, 205, 255}},
    {"purple4", {85, 26, 139, 255}},
    {"rebeccapurple", {102, 51, 153, 255}},
    {"red", {255, 0, 0, 255}},
    {"red1", {255, 0, 0, 255}},
    {"red2", {238, 0, 0, 255}},
    {"red3", {205, 0, 0, 255}},
    {"red4", {139, 0, 0, 255}},
    {"rosybrown", {188, 143, 143, 255}},
    {"rosybrown1", {255, 193, 193, 255}},
    {"rosybrown2", {238, 180, 180, 255}},
    {"rosybrown3", {205, 155, 155, 255}},
    {"rosybrown4", {139, 105, 105, 255}},
    {"royalblue", {65, 105, 225, 255}},
    {"royalblue1", {72, 118, 255, 255}},
    {"royalblue2", {67, 110, 238, 255}},
    {"royalblue3", {58, 95, 205, 255}},
    {"royalblue4", {39, 64, 139, 255}},
    {"saddlebrown", {139, 69, 19, 255}},
    {"salmon", {250, 128, 114, 255}},
    {"salmon1", {255, 140, 105, 255}},
    {"salmon2", {238, 130, 98, 255}},
    {"salmon3", {205, 112, 84, 255}},
    {"salmon4", {139, 76, 57, 255}},
    {"sandybrown", {244, 164, 96, 255}},
    {"seagreen", {46, 139, 87, 255}},
    {"seagreen1", {84, 255, 159, 255}},
    {"seagreen2", {78, 238, 148, 255}},
    {"seagreen3", {67, 205, 128, 255}},
    {"seagreen4", {46, 139, 87, 255}},
    {"seashell", {255, 245, 238, 255}},
    {"seashell1", {255, 245, 238, 255}},
    {"seashell2", {238, 229, 222, 255}},
    {"seashell3", {205, 197, 191, 255}},
    {"seashell4", {139, 134, 130, 255}},
    {"sienna", {160, 82, 45, 255}},
    {"sienna1", {255, 130, 71, 255}},
    {"sienna2", {238, 121, 66, 255}},
    {"sienna3", {205, 104, 57, 255}},
    {"sienna4", {139, 71, 38, 255}},
    {"silver", {192, 192, 192, 255}},
    {"skyblue", {135, 206, 235, 255}},
    {"skyblue1", {135, 206, 255, 255}},
    {"skyblue2", {126, 192, 238, 255}},
    {"skyblue3", {108, 166, 205, 255}},
    {"skyblue4", {74, 112, 139, 255}},
    {"slateblue", {106, 90, 205, 255}},
    {"slateblue1", {131, 111, 255, 255}},
    {"slateblue2", {122, 103, 238, 255}},
    {"slateblue3", {105, 89, 205, 255}},
    {"slateblue4", {71, 60, 139, 255}},
    {"slategray", {112, 128, 144, 255}},
    {"slategray1", {198, 226, 255, 255}},
    {"slategray2", {185, 211, 238, 255}},
    {"slategray3", {159, 182, 205, 255}},
    {"slategray4", {108, 123, 139, 255}},
    {"slategrey", {112, 128, 144, 255}},
    {"snow", {255, 250, 250, 255}},
    {"snow1", {255, 250, 250, 255}},
    {"snow2", {238, 233, 233, 255}},
    {"snow3", {205, 201, 201, 255}},
    {"snow4", {139, 137, 137, 255}},
    {"springgreen", {0, 255, 127, 255}},
    {"springgreen1", {0, 255, 127, 255}},
    {"springgreen2", {0, 238, 118, 255}},
    {"springgreen3", {0, 205, 102, 255}},
    {"springgreen4", {0, 139, 69, 255}},
    {"steelblue", {70, 130, 180, 255}},
    {"steelblue1", {99, 184, 255, 255}},
    {"steelblue2", {92, 172, 238, 255}},
    {"steelblue3", {79, 148, 205, 255}},
    {"steelblue4", {54, 100, 139, 255}},
    {"tan", {210, 180, 140, 255}},
    {"tan1", {255, 165, 79, 255}},
    {"tan2", {238, 154, 73, 255}},
    {"tan3", {205, 133, 63, 255}},
    {"tan4", {139, 90, 43, 255}},
    {"teal", {0, 128, 128, 255}},
    {"thistle", {216, 191, 216, 255}},
    {"thistle1", {255, 225, 255, 255}},
    {"thistle2", {238, 210, 238, 255}},
    {"thistle3", {205, 181, 205, 255}},
    {"thistle4", {139, 123, 139, 255}},
    {"tomato", {255, 99, 71, 255}},
    {"tomato1", {255, 99, 71, 255}},
    {"tomato2", {238, 92, 66, 255}},
    {"tomato3", {205, 79, 57, 255}},
    {"tomato4", {139, 54, 38, 255}},
    {"transparent", {255, 255, 254, 0}},
    {"turquoise", {64, 224, 208, 255}},
    {"turquoise1", {0, 245, 255, 255}},
    {"turquoise2", {0, 229, 238, 255}},
    {"turquoise3", {0, 197, 205, 255}},
    {"turquoise4", {0, 134, 139, 255}},
    {"violet", {238, 130, 238, 255}},
    {"violetred", {208, 32, 144, 255}},
    {"violetred1", {255, 62, 150, 255}},
    {"violetred2", {238, 58, 140, 255}},
    {"violetred3", {205, 50, 120, 255}},
    {"violetred4", {139, 34, 82, 255}},
    {"webgray", {128, 128, 128, 255}},
    {"webgreen", {0, 128, 0, 255}},
    {"webgrey", {128, 128, 128, 255}},
    {"webmaroon", {128, 0, 0, 255}},
    {"webpurple", {128, 0, 128, 255}},
    {"wheat", {245, 222, 179, 255}},
    {"wheat1", {255, 231, 186, 255}},
    {"wheat2", {238, 216, 174, 255}},
    {"wheat3", {205, 186, 150, 255}},
    {"wheat4", {139, 126, 102, 255}},
    {"white", {255, 255, 255, 255}},
    {"whitesmoke", {245, 245, 245, 255}},
    {"x11gray", {190, 190, 190, 255}},
    {"x11green", {0, 255, 0, 255}},
    {"x11grey", {190, 190, 190, 255}},
    {"x11maroon", {176, 48, 96, 255}},
    {"x11purple", {160, 32, 240, 255}},
    {"yellow", {255, 255, 0, 255}},
    {"yellow1", {255, 255, 0, 255}},
    {"yellow2", {238, 238, 0, 255}},
    {"yellow3", {205, 205, 0, 255}},
    {"yellow4", {139, 139, 0, 255}},
    {"yellowgreen", {154, 205, 50, 255}},
};
static const colorscheme_t X11 = {"X11", X11_colors,
                                  sizeof(X11_colors) / sizeof(X11_colors[0])};

static const colorscheme_t color_lib[] = {
    accent3,    accent4,    accent5,   accent6,   accent7,   accent8,

    blues3,     blues4,     blues5,    blues6,    blues7,    blues8,
    blues9,

    brbg10,     brbg11,     brbg3,     brbg4,     brbg5,     brbg6,
    brbg7,      brbg8,      brbg9,

    bugn3,      bugn4,      bugn5,     bugn6,     bugn7,     bugn8,
    bugn9,

    bupu3,      bupu4,      bupu5,     bupu6,     bupu7,     bupu8,
    bupu9,

    dark23,     dark24,     dark25,    dark26,    dark27,    dark28,

    gnbu3,      gnbu4,      gnbu5,     gnbu6,     gnbu7,     gnbu8,
    gnbu9,

    greens3,    greens4,    greens5,   greens6,   greens7,   greens8,
    greens9,

    greys3,     greys4,     greys5,    greys6,    greys7,    greys8,
    greys9,

    oranges3,   oranges4,   oranges5,  oranges6,  oranges7,  oranges8,
    oranges9,

    orrd3,      orrd4,      orrd5,     orrd6,     orrd7,     orrd8,
    orrd9,

    paired10,   paired11,   paired12,  paired3,   paired4,   paired5,
    paired6,    paired7,    paired8,   paired9,

    pastel13,   pastel14,   pastel15,  pastel16,  pastel17,  pastel18,
    pastel19,   pastel23,   pastel24,  pastel25,  pastel26,  pastel27,
    pastel28,

    piyg10,     piyg11,     piyg3,     piyg4,     piyg5,     piyg6,
    piyg7,      piyg8,      piyg9,

    prgn10,     prgn11,     prgn3,     prgn4,     prgn5,     prgn6,
    prgn7,      prgn8,      prgn9,

    pubu3,      pubu4,      pubu5,     pubu6,     pubu7,     pubu8,
    pubu9,

    pubugn3,    pubugn4,    pubugn5,   pubugn6,   pubugn7,   pubugn8,
    pubugn9,

    puor10,     puor11,     puor3,     puor4,     puor5,     puor6,
    puor7,      puor8,      puor9,

    purd3,      purd4,      purd5,     purd6,     purd7,     purd8,
    purd9,

    purples3,   purples4,   purples5,  purples6,  purples7,  purples8,
    purples9,

    rdbu10,     rdbu11,     rdbu3,     rdbu4,     rdbu5,     rdbu6,
    rdbu7,      rdbu8,      rdbu9,

    rdgy10,     rdgy11,     rdgy3,     rdgy4,     rdgy5,     rdgy6,
    rdgy7,      rdgy8,      rdgy9,

    rdpu3,      rdpu4,      rdpu5,     rdpu6,     rdpu7,     rdpu8,
    rdpu9,

    rdylbu10,   rdylbu11,   rdylbu3,   rdylbu4,   rdylbu5,   rdylbu6,
    rdylbu7,    rdylbu8,    rdylbu9,

    rdylgn10,   rdylgn11,   rdylgn3,   rdylgn4,   rdylgn5,   rdylgn6,
    rdylgn7,    rdylgn8,    rdylgn9,

    reds3,      reds4,      reds5,     reds6,     reds7,     reds8,
    reds9,

    set13,      set14,      set15,     set16,     set17,     set18,
    set19,      set23,      set24,     set25,     set26,     set27,
    set28,      set310,     set311,    set312,    set33,     set34,
    set35,      set36,      set37,     set38,     set39,

    spectral10, spectral11, spectral3, spectral4, spectral5, spectral6,
    spectral7,  spectral8,  spectral9,

    svg,

    X11,

    ylgn3,      ylgn4,      ylgn5,     ylgn6,     ylgn7,     ylgn8,
    ylgn9,

    ylgnbu3,    ylgnbu4,    ylgnbu5,   ylgnbu6,   ylgnbu7,   ylgnbu8,
    ylgnbu9,

    ylorbr3,    ylorbr4,    ylorbr5,   ylorbr6,   ylorbr7,   ylorbr8,
    ylorbr9,

    ylorrd3,    ylorrd4,    ylorrd5,   ylorrd6,   ylorrd7,   ylorrd8,
    ylorrd9,
};

static int schemecmpf(const void *p0, const void *p1) {
  return strcasecmp(p0, ((const colorscheme_t *)p1)->name);
}

static const colorscheme_t *findScheme(char const *schemeName) {
  return bsearch(schemeName, color_lib,
                 sizeof(color_lib) / sizeof(colorscheme_t),
                 sizeof(color_lib[0]), schemecmpf);
}

static int colorcmpf(const void *p0, const void *p1) {
  return strcasecmp(p0, ((const color_t *)p1)->name);
}

static const color_t *findColor(colorscheme_t const *scheme,
                                char const *colorName) {
  if (scheme == NULL)
    return NULL;
  return bsearch(colorName, scheme->colors, scheme->num_colors, sizeof(color_t),
                 colorcmpf);
}

static char *globalColorSchemeStr = NULL;
static colorscheme_t const *globalColorScheme = &X11;
char *setColorScheme(const char *s) {
  char *previous = globalColorSchemeStr;
  if (s == NULL) {
    globalColorSchemeStr = NULL;
    globalColorScheme = &X11;
    return previous;
  }

  globalColorSchemeStr = gv_strdup(s);
  if (s[0] == '\0') {
    globalColorScheme = &X11;
  } else {
    globalColorScheme = findScheme(s);
  }
  return previous;
}

/* resolveNamedColor:
 * Resolve input color str allowing color scheme namespaces.
 *  0) "black" => "black"
 *     "white" => "white"
 *     "lightgrey" => "lightgrey"
 *    NB: This is something of a hack due to the remaining codegen.
 *        Once these are gone, this case could be removed and all references
 *        to "black" could be replaced by "/X11/black".
 *  1) No initial / =>
 *          if colorscheme is defined and no "X11", return /colorscheme/str
 *          else return str
 *  2) One initial / => return str+1
 *  3) Two initial /'s =>
 *       a) If colorscheme is defined and not "X11", return /colorscheme/(str+2)
 *       b) else return (str+2)
 *  4) Two /'s, not both initial => return str.
 *
 * Note that 1), 2), and 3b) allow the default X11 color scheme.
 *
 * In other words,
 *   xxx => /colorscheme/xxx     if colorscheme is defined and not "X11"
 *   xxx => xxx                  otherwise
 *   /xxx => xxx
 *   /X11/yyy => yyy
 *   /xxx/yyy => /xxx/yyy
 *   //yyy => /colorscheme/yyy   if colorscheme is defined and not "X11"
 *   //yyy => yyy                otherwise
 *
 * At present, no other error checking is done. For example,
 * yyy could be "". This will be caught later.
 */
static color_t const *resolveNamedColor(char *str) {

  if (!strcmp(str, "black") || !strcmp(str, "white") ||
      !strcmp(str, "lightgrey"))
    return findColor(&X11, str);

  if (str[0] == '/') {
    if (str[1] == '/') {
      return findColor(globalColorScheme, &str[2]);
    }
    char *secondSlash = strchr(&str[1], '/');
    if (secondSlash == NULL) {
      return findColor(&X11, &str[1]);
    }

    *secondSlash = '\0';
    colorscheme_t const *scheme = findScheme(&str[1]);
    *secondSlash = '/';
    return findColor(scheme, secondSlash + 1);
  }

  return findColor(globalColorScheme, str);
}

static rgba_t hsva2rgb(double h, double s, double v, double a) {
  /* clip to reasonable values */
  h = fmax(fmin(h, 1.0), 0.0);
  s = fmax(fmin(s, 1.0), 0.0);
  a = fmax(fmin(a, 1.0), 0.0);
  v = fmax(fmin(v, 1.0), 0.0);

  rgba_t color = {0};
  color.a = (a * 255);

  if (s == 0.0) { /* achromatic */
    color.r = color.g = color.b = (v * 255);
    return color;
  }

  if (h == 1.0)
    h = 0.0;
  h = 6.0 * h;
  int i = (int)h;
  double f = h - i;
  double p = v * (1 - s);
  double q = v * (1 - s * f);
  double t = v * (1 - s * (1 - f));

  // conver floats in [0,1] range into [0, 255] integers
  switch (i) {
  case 0:
    color.r = (v * 255);
    color.g = (t * 255);
    color.b = (p * 255);
    return color;
  case 1:
    color.r = (q * 255);
    color.g = (v * 255);
    color.b = (p * 255);
    return color;
  case 2:
    color.r = (p * 255);
    color.g = (v * 255);
    color.b = (t * 255);
    return color;
  case 3:
    color.r = (p * 255);
    color.g = (q * 255);
    color.b = (v * 255);
    return color;
  case 4:
    color.r = (t * 255);
    color.g = (p * 255);
    color.b = (v * 255);
    return color;
  case 5:
    color.r = (v * 255);
    color.g = (p * 255);
    color.b = (q * 255);
    return color;
  default:
    UNREACHABLE();
  }
}

bool resolveColor(char *str, gvcolor_t *result) {
  for (; *str == ' '; str++)
    ; /* skip over any leading whitespace */

  /* test for rgb value such as: "#ff0000" or rgba value such as "#ff000080" */
  unsigned char a =
      255; // default alpha channel value=opaque in case not supplied
  unsigned char r, g, b;
  if (sscanf(str, "#%2hhx%2hhx%2hhx%2hhx", &r, &g, &b, &a) >= 3) {
    result->type = RGBA_BYTE;
    result->u.rgba[0] = r;
    result->u.rgba[1] = g;
    result->u.rgba[2] = b;
    result->u.rgba[3] = a;
    return true;
  }

  // try 3 letter form
  if (strlen(str) == 4 && sscanf(str, "#%1hhx%1hhx%1hhx", &r, &g, &b) == 3) {
    result->type = RGBA_BYTE;
    result->u.rgba[0] = r | (r << 4);
    result->u.rgba[1] = g | (g << 4);
    result->u.rgba[2] = b | (b << 4);
    result->u.rgba[3] = a;
    return true;
  }

  /* test for hsv value such as: ".6,.5,.3" */
  if (str[0] == '.' || (str[0] >= '0' && str[0] <= '9')) {
    char *canon = strdup(str);
    for (char *p = canon; *p != '\0'; ++p) {
      if (*p == ',')
        *p = ' ';
    }

    double H, S, V, A = 1.0; // default
    if (sscanf(canon, "%lf%lf%lf%lf", &H, &S, &V, &A) >= 3) {
      free(canon);
      rgba_t rgb = hsva2rgb(H, S, V, A);
      result->type = RGBA_BYTE;
      result->u.rgba[0] = rgb.r;
      result->u.rgba[1] = rgb.g;
      result->u.rgba[2] = rgb.b;
      result->u.rgba[3] = rgb.a;
      return true;
    }
    free(canon);
  }

  // SVG specific: if it's SVG color use it as is
  if (findColor(&svg, str) != NULL) {
    result->type = COLOR_STRING;
    result->u.string = str;
    return true;
  }

  /* test for known color name (generic, not renderer specific known names) */
  const color_t *known = resolveNamedColor(str);
  if (known != NULL) {
    result->type = RGBA_BYTE;
    result->u.rgba[0] = known->value.r;
    result->u.rgba[1] = known->value.g;
    result->u.rgba[2] = known->value.b;
    result->u.rgba[3] = known->value.a;
    return true;
  }

  return false;
}
