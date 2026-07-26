// ASCII Art Converter - Converts footage to ASCII art
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define NAME "ASCII Art Converter"
#define DESCRIPTION "Converts images to ASCII art using built-in or system fonts"
#define MAJOR_VERSION 4
#define MINOR_VERSION 0

enum {
    INPUT_LAYER = 0,
    CHAR_SIZE_PARAM,
    FONT_PARAM,
    CHARSET_PARAM,
    INVERT_PARAM,
    COLOR_MODE_PARAM,
    CELL_ASPECT_PARAM,
    TRACKING_PARAM,
    LINE_SPACING_PARAM,
    FONT_STYLE_PARAM,
    EDGE_MODE_PARAM,
    EDGE_STRENGTH_PARAM,
    RANDOM_SEED_PARAM,
    CYCLE_RATE_PARAM,
    REVEAL_PARAM,
    SCRAMBLE_PARAM,
    GLITCH_PARAM,
    TEMPORAL_STABILITY_PARAM,
    NUM_PARAMS
};

// Different character sets ordered from light to dark
static const char letter_chars[] = " .':;ilIoCOGM@";
static const char number_chars[] = " .1234567890";
static const char special_chars[] = " .,':;!*+=<>#$@%&";

// 8x16 bitmap font (Public Domain)
// Author: Daniel Hepper, based on VGA fonts
static const unsigned char font8x16[][16] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x20, ' '
    { 0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,  },       //0x21, '!'
    { 0x00, 0x66, 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x22, '"'
    { 0x00, 0x00, 0x00, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00,  },       //0x23, '#'
    { 0x18, 0x18, 0x7C, 0xC6, 0xC2, 0xC0, 0x7C, 0x06, 0x06, 0x86, 0xC6, 0x7C, 0x18, 0x18, 0x00, 0x00,  },       //0x24, '$'
    { 0x00, 0x00, 0x00, 0x00, 0xC2, 0xC6, 0x0C, 0x18, 0x30, 0x60, 0xC6, 0x86, 0x00, 0x00, 0x00, 0x00,  },       //0x25, '%'
    { 0x00, 0x00, 0x38, 0x6C, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00,  },       //0x26, '&'
    { 0x00, 0x30, 0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x27, '''
    { 0x00, 0x00, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00,  },       //0x28, '('
    { 0x00, 0x00, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,  },       //0x29, ')'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x2A, '*'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x2B, '+'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00,  },       //0x2C, ','
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x2D, '-'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,  },       //0x2E, '.'
    { 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00,  },       //0x2F, '/'
    { 0x00, 0x00, 0x38, 0x6C, 0xC6, 0xC6, 0xD6, 0xD6, 0xC6, 0xC6, 0x6C, 0x38, 0x00, 0x00, 0x00, 0x00,  },       //0x30, '0'
    { 0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00,  },       //0x31, '1'
    { 0x00, 0x00, 0x7C, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00,  },       //0x32, '2'
    { 0x00, 0x00, 0x7C, 0xC6, 0x06, 0x06, 0x3C, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x33, '3'
    { 0x00, 0x00, 0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00, 0x00,  },       //0x34, '4'
    { 0x00, 0x00, 0xFE, 0xC0, 0xC0, 0xC0, 0xFC, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x35, '5'
    { 0x00, 0x00, 0x38, 0x60, 0xC0, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x36, '6'
    { 0x00, 0x00, 0xFE, 0xC6, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,  },       //0x37, '7'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x38, '8'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0x06, 0x0C, 0x78, 0x00, 0x00, 0x00, 0x00,  },       //0x39, '9'
    { 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x3A, ':'
    { 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,  },       //0x3B, ';'
    { 0x00, 0x00, 0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00,  },       //0x3C, '<'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x3D, '='
    { 0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00,  },       //0x3E, '>'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0x0C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,  },       //0x3F, '?'
    { 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xDE, 0xDE, 0xDE, 0xDC, 0xC0, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x40, '@'
    { 0x00, 0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x41, 'A'
    { 0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0x66, 0xFC, 0x00, 0x00, 0x00, 0x00,  },       //0x42, 'B'
    { 0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xC0, 0xC0, 0xC2, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x43, 'C'
    { 0x00, 0x00, 0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00, 0x00, 0x00, 0x00,  },       //0x44, 'D'
    { 0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00,  },       //0x45, 'E'
    { 0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00,  },       //0x46, 'F'
    { 0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xDE, 0xC6, 0xC6, 0x66, 0x3A, 0x00, 0x00, 0x00, 0x00,  },       //0x47, 'G'
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x48, 'H'
    { 0x00, 0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x49, 'I'
    { 0x00, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0xCC, 0x78, 0x00, 0x00, 0x00, 0x00,  },       //0x4A, 'J'
    { 0x00, 0x00, 0xE6, 0x66, 0x66, 0x6C, 0x78, 0x78, 0x6C, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00,  },       //0x4B, 'K'
    { 0x00, 0x00, 0xF0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00,  },       //0x4C, 'L'
    { 0x00, 0x00, 0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x4D, 'M'
    { 0x00, 0x00, 0xC6, 0xE6, 0xF6, 0xFE, 0xDE, 0xCE, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x4E, 'N'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x4F, 'O'
    { 0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00,  },       //0x50, 'P'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xDE, 0x7C, 0x0C, 0x0E, 0x00, 0x00,  },       //0x51, 'Q'
    { 0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00,  },       //0x52, 'R'
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0x60, 0x38, 0x0C, 0x06, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x53, 'S'
    { 0x00, 0x00, 0x7E, 0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x54, 'T'
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x55, 'U'
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,  },       //0x56, 'V'
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xD6, 0xFE, 0xEE, 0x6C, 0x00, 0x00, 0x00, 0x00,  },       //0x57, 'W'
    { 0x00, 0x00, 0xC6, 0xC6, 0x6C, 0x7C, 0x38, 0x38, 0x7C, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x58, 'X'
    { 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x59, 'Y'
    { 0x00, 0x00, 0xFE, 0xC6, 0x86, 0x0C, 0x18, 0x30, 0x60, 0xC2, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00,  },       //0x5A, 'Z'
    { 0x00, 0x00, 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x5B, '['
    { 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00,  },       //0x5C, '\'
    { 0x00, 0x00, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x5D, ']'
    { 0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x5E, '^'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,  },       //0x5F, '_'
    { 0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x60, '`'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00,  },       //0x61, 'a'
    { 0x00, 0x00, 0xE0, 0x60, 0x60, 0x78, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x62, 'b'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x63, 'c'
    { 0x00, 0x00, 0x1C, 0x0C, 0x0C, 0x3C, 0x6C, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00,  },       //0x64, 'd'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x65, 'e'
    { 0x00, 0x00, 0x38, 0x6C, 0x64, 0x60, 0xF0, 0x60, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00,  },       //0x66, 'f'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xCC, 0x78, 0x00,  },       //0x67, 'g'
    { 0x00, 0x00, 0xE0, 0x60, 0x60, 0x6C, 0x76, 0x66, 0x66, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00,  },       //0x68, 'h'
    { 0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x69, 'i'
    { 0x00, 0x00, 0x06, 0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00,  },       //0x6A, 'j'
    { 0x00, 0x00, 0xE0, 0x60, 0x60, 0x66, 0x6C, 0x78, 0x78, 0x6C, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00,  },       //0x6B, 'k'
    { 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00,  },       //0x6C, 'l'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xD6, 0xD6, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x6D, 'm'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,  },       //0x6E, 'n'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x6F, 'o'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00,  },       //0x70, 'p'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0x0C, 0x1E, 0x00,  },       //0x71, 'q'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x76, 0x66, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00,  },       //0x72, 'r'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00,  },       //0x73, 's'
    { 0x00, 0x00, 0x10, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x30, 0x30, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00,  },       //0x74, 't'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00,  },       //0x75, 'u'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x00,  },       //0x76, 'v'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00, 0x00, 0x00, 0x00,  },       //0x77, 'w'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x6C, 0x38, 0x38, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00,  },       //0x78, 'x'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0xF8, 0x00,  },       //0x79, 'y'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xCC, 0x18, 0x30, 0x60, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00,  },       //0x7A, 'z'
    { 0x00, 0x00, 0x0E, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x00, 0x00, 0x00, 0x00,  },       //0x7B, '{'
    { 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,  },       //0x7C, '|'
    { 0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00,  },       //0x7D, '}'
    { 0x00, 0x00, 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  },       //0x7E, '~'
};

static PF_Err About(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg, "%s v%d.%d\r%s", NAME, MAJOR_VERSION, MINOR_VERSION, DESCRIPTION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output) {
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, 0, 0, 0);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

struct FontCatalog {
    std::vector<std::string> families;
    std::string popup_items;
};

static int CALLBACK CollectFontFamily(const LOGFONTA *log_font, const TEXTMETRICA *, DWORD, LPARAM user_data) {
    if (!log_font || !log_font->lfFaceName[0] || log_font->lfFaceName[0] == '@') {
        return 1;
    }
    std::vector<std::string> *families = reinterpret_cast<std::vector<std::string> *>(user_data);
    families->push_back(log_font->lfFaceName);
    return 1;
}

static const FontCatalog &GetFontCatalog() {
    static const FontCatalog catalog = []() {
        FontCatalog result;
        HDC dc = CreateCompatibleDC(nullptr);
        if (dc) {
            LOGFONTA query = {};
            query.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExA(dc, &query, reinterpret_cast<FONTENUMPROCA>(CollectFontFamily),
                                reinterpret_cast<LPARAM>(&result.families), 0);
            DeleteDC(dc);
        }

        std::sort(result.families.begin(), result.families.end(),
                  [](const std::string &a, const std::string &b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
        result.families.erase(
            std::unique(result.families.begin(), result.families.end(),
                        [](const std::string &a, const std::string &b) { return _stricmp(a.c_str(), b.c_str()) == 0; }),
            result.families.end());

        result.popup_items = "Built-in VGA";
        for (const std::string &family : result.families) {
            result.popup_items += "|";
            result.popup_items += family;
        }
        return result;
    }();
    return catalog;
}

static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output) {
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Character Size", 4, 128, 4, 128, 8, PF_Precision_INTEGER, 0, 0, CHAR_SIZE_PARAM);
    AEFX_CLR_STRUCT(def);
    const FontCatalog &fonts = GetFontCatalog();
    PF_ADD_POPUP("Font", static_cast<A_long>(fonts.families.size() + 1), 1,
                 fonts.popup_items.c_str(), FONT_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Character Set", 3, 1, "Letters|Numbers|Special Characters", CHARSET_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOXX("Invert", 0, 0, INVERT_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Color Mode", 2, 1, "Monochrome|Source Color", COLOR_MODE_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Cell Aspect %", 25, 400, 25, 400, 200, PF_Precision_INTEGER, 0, 0, CELL_ASPECT_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Tracking %", -75, 300, -75, 300, 0, PF_Precision_INTEGER, 0, 0, TRACKING_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Line Spacing %", -75, 300, -75, 300, 0, PF_Precision_INTEGER, 0, 0, LINE_SPACING_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Font Style", 4, 1, "Regular|Bold|Italic|Bold Italic", FONT_STYLE_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Edge Mapping", 3, 1, "Off|Directional|Mixed", EDGE_MODE_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Edge Strength", 0, 100, 0, 100, 75, PF_Precision_TENTHS, 0, 0, EDGE_STRENGTH_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("Random Seed", 0, 9999, 0, 9999, 1, RANDOM_SEED_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Character Cycle Rate", 0, 60, 0, 60, 0, PF_Precision_TENTHS, 0, 0, CYCLE_RATE_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Reveal", 0, 100, 0, 100, 100, PF_Precision_TENTHS, 0, 0, REVEAL_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Scramble", 0, 100, 0, 100, 0, PF_Precision_TENTHS, 0, 0, SCRAMBLE_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Glitch", 0, 100, 0, 100, 0, PF_Precision_TENTHS, 0, 0, GLITCH_PARAM);
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Temporal Stability", 0, 100, 0, 100, 75, PF_Precision_TENTHS, 0, 0, TEMPORAL_STABILITY_PARAM);
    out_data->num_params = NUM_PARAMS;
    return PF_Err_NONE;
}

struct GlyphAtlas {
    int width = 0;
    int height = 0;
    std::string characters;
    std::vector<uint8_t> masks;
};

static void SortAtlasByDensity(GlyphAtlas &atlas) {
    std::vector<size_t> order(atlas.characters.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    const size_t pixels_per_glyph = static_cast<size_t>(atlas.width) * atlas.height;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        uint64_t density_a = 0, density_b = 0;
        for (size_t p = 0; p < pixels_per_glyph; ++p) {
            density_a += atlas.masks[a * pixels_per_glyph + p];
            density_b += atlas.masks[b * pixels_per_glyph + p];
        }
        return density_a < density_b;
    });

    std::string sorted_characters;
    std::vector<uint8_t> sorted_masks;
    sorted_characters.reserve(order.size());
    sorted_masks.reserve(atlas.masks.size());
    for (size_t index : order) {
        sorted_characters.push_back(atlas.characters[index]);
        sorted_masks.insert(sorted_masks.end(), atlas.masks.begin() + index * pixels_per_glyph,
                            atlas.masks.begin() + (index + 1) * pixels_per_glyph);
    }
    atlas.characters.swap(sorted_characters);
    atlas.masks.swap(sorted_masks);
}

static GlyphAtlas BuildBuiltinAtlas(const char *characters, int width, int height) {
    GlyphAtlas atlas;
    atlas.width = width;
    atlas.height = height;
    atlas.characters = characters;
    atlas.masks.resize(atlas.characters.size() * static_cast<size_t>(width) * height, 0);
    for (size_t glyph = 0; glyph < atlas.characters.size(); ++glyph) {
        const int code = static_cast<unsigned char>(atlas.characters[glyph]);
        if (code < 32 || code >= 127) continue;
        const unsigned char *bitmap = font8x16[code - 32];
        for (int y = 0; y < height; ++y) {
            const int source_y = y * 16 / height;
            for (int x = 0; x < width; ++x) {
                const int source_x = x * 8 / width;
                atlas.masks[(glyph * height + y) * width + x] =
                    (bitmap[source_y] & (1 << source_x)) ? 255 : 0;
            }
        }
    }
    SortAtlasByDensity(atlas);
    return atlas;
}

static bool BuildSystemFontAtlas(const char *characters, const char *font_family, int font_style, int width, int height,
                                 GlyphAtlas &atlas) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *dib_pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &dib_pixels, nullptr, 0);
    const bool bold = font_style == 2 || font_style == 4;
    const bool italic = font_style == 3 || font_style == 4;
    HFONT font = CreateFontA(-static_cast<int>(height * 0.88), 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                             italic ? TRUE : FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, font_family);
    if (!bitmap || !font || !dib_pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (font) DeleteObject(font);
        DeleteDC(dc);
        return false;
    }

    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
    HGDIOBJ old_font = SelectObject(dc, font);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, OPAQUE);

    atlas.width = width;
    atlas.height = height;
    atlas.characters = characters;
    atlas.masks.assign(atlas.characters.size() * static_cast<size_t>(width) * height, 0);
    RECT bounds = {0, 0, width, height};
    for (size_t glyph = 0; glyph < atlas.characters.size(); ++glyph) {
        PatBlt(dc, 0, 0, width, height, BLACKNESS);
        char text[2] = {atlas.characters[glyph], 0};
        DrawTextA(dc, text, 1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        const uint8_t *source = static_cast<const uint8_t *>(dib_pixels);
        uint8_t *destination = atlas.masks.data() + glyph * static_cast<size_t>(width) * height;
        for (int pixel = 0; pixel < width * height; ++pixel) {
            destination[pixel] = std::max(source[pixel * 4], std::max(source[pixel * 4 + 1], source[pixel * 4 + 2]));
        }
    }

    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    SortAtlasByDensity(atlas);
    return true;
}

static std::shared_ptr<const GlyphAtlas> GetCachedFontAtlas(const char *characters, const char *font_family,
                                                            int font_style, int width, int height) {
    static std::mutex cache_mutex;
    static std::unordered_map<std::string, std::shared_ptr<const GlyphAtlas>> cache;
    std::ostringstream key_stream;
    key_stream << characters << '|' << (font_family ? font_family : "Built-in VGA") << '|'
               << font_style << '|' << width << 'x' << height;
    const std::string key = key_stream.str();
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        const auto found = cache.find(key);
        if (found != cache.end()) return found->second;
    }

    GlyphAtlas atlas;
    if (font_family) BuildSystemFontAtlas(characters, font_family, font_style, width, height, atlas);
    if (atlas.masks.empty()) atlas = BuildBuiltinAtlas(characters, width, height);
    const std::shared_ptr<const GlyphAtlas> result = std::make_shared<const GlyphAtlas>(std::move(atlas));
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (cache.size() >= 48) cache.clear();
        cache[key] = result;
    }
    return result;
}

static uint32_t HashCell(uint32_t x, uint32_t y, uint32_t seed, uint32_t frame) {
    uint32_t value = x * 0x9E3779B9u ^ y * 0x85EBCA6Bu ^ seed * 0xC2B2AE35u ^ frame * 0x27D4EB2Fu;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}

template <typename PixelT, uint32_t MaxChannel>
static PixelT MakePixel(uint32_t red, uint32_t green, uint32_t blue) {
    PixelT pixel = {};
    pixel.alpha = static_cast<decltype(pixel.alpha)>(MaxChannel);
    pixel.red = static_cast<decltype(pixel.red)>(red);
    pixel.green = static_cast<decltype(pixel.green)>(green);
    pixel.blue = static_cast<decltype(pixel.blue)>(blue);
    return pixel;
}

template <typename PixelT, uint32_t MaxChannel>
static PF_Err RenderTyped(PF_InData *in_data, PF_ParamDef *params[], PF_LayerDef *output) {
    PF_LayerDef *input = &params[INPUT_LAYER]->u.ld;
    const int character_width = std::max(4, static_cast<int>(params[CHAR_SIZE_PARAM]->u.fs_d.value));
    const float aspect = static_cast<float>(params[CELL_ASPECT_PARAM]->u.fs_d.value) / 100.0f;
    const int character_height = std::max(2, static_cast<int>(std::round(character_width * aspect)));
    const float tracking = static_cast<float>(params[TRACKING_PARAM]->u.fs_d.value) / 100.0f;
    const float line_spacing = static_cast<float>(params[LINE_SPACING_PARAM]->u.fs_d.value) / 100.0f;
    const int cell_width = std::max(1, static_cast<int>(std::round(character_width * (1.0f + tracking))));
    const int cell_height = std::max(1, static_cast<int>(std::round(character_height * (1.0f + line_spacing))));
    const A_long font_choice = params[FONT_PARAM]->u.pd.value;
    const A_long charset_mode = params[CHARSET_PARAM]->u.pd.value;
    const bool invert = params[INVERT_PARAM]->u.bd.value != FALSE;
    const A_long color_mode = params[COLOR_MODE_PARAM]->u.pd.value;
    const int font_style = params[FONT_STYLE_PARAM]->u.pd.value;
    const int edge_mode = params[EDGE_MODE_PARAM]->u.pd.value;

    std::string character_string = charset_mode == 2 ? number_chars : (charset_mode == 3 ? special_chars : letter_chars);
    if (edge_mode > 1) character_string += "-|/\\";

    const FontCatalog &fonts = GetFontCatalog();
    const char *family = nullptr;
    if (font_choice > 1 && static_cast<size_t>(font_choice - 2) < fonts.families.size()) {
        family = fonts.families[font_choice - 2].c_str();
    }
    const std::shared_ptr<const GlyphAtlas> cached_atlas = GetCachedFontAtlas(
        character_string.c_str(), family, font_style, character_width, character_height);
    const GlyphAtlas *atlas = cached_atlas.get();

    const PixelT background = invert ? MakePixel<PixelT, MaxChannel>(MaxChannel, MaxChannel, MaxChannel)
                                     : MakePixel<PixelT, MaxChannel>(0, 0, 0);
    for (int y = 0; y < output->height; ++y) {
        PixelT *row = reinterpret_cast<PixelT *>(reinterpret_cast<char *>(output->data) + y * output->rowbytes);
        std::fill(row, row + output->width, background);
    }

    const float edge_strength = static_cast<float>(params[EDGE_STRENGTH_PARAM]->u.fs_d.value) / 100.0f;
    const uint32_t seed = params[RANDOM_SEED_PARAM]->u.sd.value;
    const float cycle_rate = static_cast<float>(params[CYCLE_RATE_PARAM]->u.fs_d.value);
    const float reveal = static_cast<float>(params[REVEAL_PARAM]->u.fs_d.value) / 100.0f;
    const float scramble = static_cast<float>(params[SCRAMBLE_PARAM]->u.fs_d.value) / 100.0f;
    const float glitch = static_cast<float>(params[GLITCH_PARAM]->u.fs_d.value) / 100.0f;
    const float stability = static_cast<float>(params[TEMPORAL_STABILITY_PARAM]->u.fs_d.value) / 100.0f;
    const double seconds = in_data->time_scale ? static_cast<double>(in_data->current_time) / in_data->time_scale : 0.0;
    const uint32_t animation_frame = cycle_rate > 0.0f ? static_cast<uint32_t>(std::floor(seconds * cycle_rate)) : 0;
    const int grid_columns = (output->width + cell_width - 1) / cell_width;
    const int grid_rows = (output->height + cell_height - 1) / cell_height;
    const size_t glyph_pixels = static_cast<size_t>(character_width) * character_height;

    auto point_luma = [&](int x, int y) {
        x = std::max(0, std::min(input->width - 1, x));
        y = std::max(0, std::min(input->height - 1, y));
        const PixelT *row = reinterpret_cast<const PixelT *>(reinterpret_cast<const char *>(input->data) + y * input->rowbytes);
        return (static_cast<float>(row[x].red) + row[x].green + row[x].blue) / (3.0f * MaxChannel);
    };
    auto atlas_index = [&](char wanted, size_t fallback) {
        const size_t found = atlas->characters.find(wanted);
        return found == std::string::npos ? fallback : found;
    };

    for (int grid_y = 0, block_y = 0; block_y < output->height; ++grid_y, block_y += cell_height) {
        for (int grid_x = 0, block_x = 0; block_x < output->width; ++grid_x, block_x += cell_width) {
            if (block_x >= input->width || block_y >= input->height) continue;
            const int sample_width = std::min(cell_width, input->width - block_x);
            const int sample_height = std::min(cell_height, input->height - block_y);
            uint64_t red_sum = 0, green_sum = 0, blue_sum = 0, count = 0;
            for (int y = 0; y < sample_height; ++y) {
                const PixelT *row = reinterpret_cast<const PixelT *>(
                    reinterpret_cast<const char *>(input->data) + (block_y + y) * input->rowbytes);
                for (int x = 0; x < sample_width; ++x) {
                    red_sum += row[block_x + x].red;
                    green_sum += row[block_x + x].green;
                    blue_sum += row[block_x + x].blue;
                    ++count;
                }
            }
            if (!count) continue;

            const uint32_t average_red = static_cast<uint32_t>(red_sum / count);
            const uint32_t average_green = static_cast<uint32_t>(green_sum / count);
            const uint32_t average_blue = static_cast<uint32_t>(blue_sum / count);
            float brightness = (average_red + average_green + average_blue) / (3.0f * MaxChannel);
            if (invert) brightness = 1.0f - brightness;
            const int stability_levels = std::max(4, static_cast<int>(64.0f - stability * 56.0f));
            brightness = std::round(brightness * stability_levels) / stability_levels;
            brightness = std::max(0.0f, std::min(1.0f, brightness));
            size_t glyph = std::min(atlas->characters.size() - 1,
                                    static_cast<size_t>(brightness * atlas->characters.size()));

            const float gx = point_luma(block_x + sample_width, block_y + sample_height / 2) -
                             point_luma(block_x - 1, block_y + sample_height / 2);
            const float gy = point_luma(block_x + sample_width / 2, block_y + sample_height) -
                             point_luma(block_x + sample_width / 2, block_y - 1);
            const float edge_magnitude = std::sqrt(gx * gx + gy * gy) * edge_strength;
            if (edge_mode > 1 && edge_magnitude > (edge_mode == 2 ? 0.04f : 0.16f)) {
                char direction;
                if (std::abs(gx) > std::abs(gy) * 2.0f) direction = '|';
                else if (std::abs(gy) > std::abs(gx) * 2.0f) direction = '-';
                else direction = gx * gy > 0.0f ? '/' : '\\';
                glyph = atlas_index(direction, glyph);
            }

            const uint32_t random = HashCell(grid_x, grid_y, seed, animation_frame);
            const float random_unit = (random & 0xffff) / 65535.0f;
            const float cell_progress = static_cast<float>(grid_y * grid_columns + grid_x) /
                                        std::max(1, grid_rows * grid_columns - 1);
            if (cell_progress > reveal) continue;
            if (random_unit < scramble || cycle_rate > 0.0f) glyph = (random >> 16) % atlas->characters.size();
            int draw_x = block_x;
            if (glitch > 0.0f && random_unit < glitch) {
                draw_x += static_cast<int>((static_cast<int>((random >> 20) & 15) - 7) * character_width * glitch / 4.0f);
                glyph = (random >> 12) % atlas->characters.size();
            }

            const PixelT foreground = color_mode == 2
                ? MakePixel<PixelT, MaxChannel>(average_red, average_green, average_blue)
                : (invert ? MakePixel<PixelT, MaxChannel>(0, 0, 0)
                          : MakePixel<PixelT, MaxChannel>(MaxChannel, MaxChannel, MaxChannel));
            const uint8_t *mask = atlas->masks.data() + glyph * glyph_pixels;
            for (int y = 0; y < character_height && block_y + y < output->height; ++y) {
                PixelT *row = reinterpret_cast<PixelT *>(
                    reinterpret_cast<char *>(output->data) + (block_y + y) * output->rowbytes);
                for (int x = 0; x < character_width; ++x) {
                    const int output_x = draw_x + x;
                    if (output_x < 0 || output_x >= output->width) continue;
                    const uint32_t fill = mask[y * character_width + x];
                    PixelT pixel = background;
                    pixel.red = static_cast<decltype(pixel.red)>((background.red * (255 - fill) + foreground.red * fill + 127) / 255);
                    pixel.green = static_cast<decltype(pixel.green)>((background.green * (255 - fill) + foreground.green * fill + 127) / 255);
                    pixel.blue = static_cast<decltype(pixel.blue)>((background.blue * (255 - fill) + foreground.blue * fill + 127) / 255);
                    row[output_x] = pixel;
                }
            }
        }
    }
    return PF_Err_NONE;
}

static PF_Err Render(PF_InData *in_data, PF_OutData *, PF_ParamDef *params[], PF_LayerDef *output) {
    return PF_WORLD_IS_DEEP(output) ? RenderTyped<PF_Pixel16, PF_MAX_CHAN16>(in_data, params, output)
                                    : RenderTyped<PF_Pixel, PF_MAX_CHAN8>(in_data, params, output);
}

DllExport PF_Err EntryPointFunc(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT: err = About(in_data, out_data, params, output); break;
            case PF_Cmd_GLOBAL_SETUP: err = GlobalSetup(in_data, out_data, params, output); break;
            case PF_Cmd_PARAMS_SETUP: err = ParamsSetup(in_data, out_data, params, output); break;
            case PF_Cmd_RENDER: err = Render(in_data, out_data, params, output); break;
        }
    } catch (PF_Err &thrown_err) { err = thrown_err; }
    return err;
}
