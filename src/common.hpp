#pragma once

#define OURO_TITLE "OURO"

#define OURO_TITLE "OURO"
#if defined(OURO_DEBUG) || defined(OURO_DEVEL) || defined(OURO_TRACE)
#define OURO_IS_DEBUG true
#define OURO_IS_DEBUG_NUM 1
#else
#define OURO_IS_DEBUG false
#define OURO_IS_DEBUG_NUM 0
#endif

#if defined(OURO_DEBUG) || defined(OURO_TRACE)
#define OURO_IS_DEBUG_NO_DEVEL true
#define OURO_IS_DEBUG_NO_DEVEL_NUM 1
#else
#define OURO_IS_DEBUG_NO_DEVEL false
#define OURO_IS_DEBUG_NO_DEVEL_NUM 0
#endif

// Custom colors
#define BASICALLYBLACK  CLITERAL(Color) {  20,  20,  20, 255 }
#define C64CYAN         CLITERAL(Color) { 170, 255, 238, 255 }
#define C64DARKGRAY     CLITERAL(Color) {  68,  68,  68, 255 }
#define C64LIGHTBLUE    CLITERAL(Color) {   0, 136, 255, 255 }
#define C64LIGHTGRAY    CLITERAL(Color) { 187, 187, 187, 255 }
#define CREAM           CLITERAL(Color) { 255, 253, 208, 255 }
#define CYAN            CLITERAL(Color) {   0, 255, 255, 255 }
#define DARKESTGRAY     CLITERAL(Color) {  50,  50,  50, 255 }
#define DAYLIGHT        CLITERAL(Color) { 255, 223, 191, 255 }
#define EVABLUE         CLITERAL(Color) {  80, 114, 153, 255 }
#define EVAFATBLUE      CLITERAL(Color) {  67, 116, 173, 255 }
#define EVAFATGREEN     CLITERAL(Color) {  40, 150,  60, 255 }
#define EVAFATORANGE    CLITERAL(Color) { 220, 100,  40, 255 }
#define EVAFATPURPLE    CLITERAL(Color) { 138,  48, 138, 255 }
#define EVAGREEN        CLITERAL(Color) { 173, 241, 130, 255 }
#define EVALIGHTBLUE    CLITERAL(Color) {  97, 175, 239, 255 }
#define EVALIGHTGREEN   CLITERAL(Color) {  82, 196,  89, 255 }
#define EVALIGHTORANGE  CLITERAL(Color) { 255, 203, 107, 255 }
#define EVALIGHTPURPLE  CLITERAL(Color) { 145, 108, 173, 255 }
#define EVAORANGE       CLITERAL(Color) { 220, 125, 104, 255 }
#define EVAORANGE       CLITERAL(Color) { 220, 125, 104, 255 }
#define EVAPURPLE       CLITERAL(Color) {  82,  56, 116, 255 }
#define EVAPURPLE       CLITERAL(Color) {  82,  56, 116, 255 }
#define HACKERGREEN     CLITERAL(Color) {  50, 125,  25, 255 }
#define NAYBEIGE        CLITERAL(Color) { 209, 184, 151, 255 }
#define NAYBLUE         CLITERAL(Color) {  41,  61,  61, 255 }
#define NAYGREEN        CLITERAL(Color) {   6,  35,  41, 255 }
#define NEARBLACK       CLITERAL(Color) {  30,  30,  30, 255 }
#define NEARNEARBLACK   CLITERAL(Color) {  35,  35,  35, 255 }
#define NEARESTBLACK    CLITERAL(Color) {  10,  10,  10, 255 }
#define OFFCOLOR        CLITERAL(Color) { 179,  22,  27, 255 }
#define ONCOLOR         CLITERAL(Color) {  15, 128,  56, 255 }
#define REDBLACK        CLITERAL(Color) {  50,   0,   0, 255 }
#define PERSIMMON       CLITERAL(Color) { 255,  94,  72, 255 }
#define SALMON          CLITERAL(Color) { 255, 128, 114, 255 }
#define TANGERINE       CLITERAL(Color) { 255, 140,  80, 255 }
#define TUSCAN          CLITERAL(Color) { 255, 170, 130, 255 }
#define YELLOWWHITE     CLITERAL(Color) { 255, 255, 224, 255 }
#define DEEPOCEANBLUE   CLITERAL(Color) {  18,  35,  55, 255 }
#define FROSTWHITE      CLITERAL(Color) { 235, 245, 250, 255 }
#define ELECTRICTEAL    CLITERAL(Color) {  36, 209, 209, 255 }
#define SUNSETAMBER     CLITERAL(Color) { 255, 183,  77, 255 }
#define SOFTLAVENDER    CLITERAL(Color) { 190, 173, 250, 255 }
#define SPRINGLEAF      CLITERAL(Color) { 120, 220, 150, 255 }
#define MIDNIGHTBLUE    CLITERAL(Color) {  25,  45,  70, 255 }

// Globals
#define ANIMATION_RATE (1.0F / 60.0F)
#define OUC_MAX_SEGMENT_LENGTH 50000

// Defines for the game and libraries
#define OURO_TALK false  // WARN: Turning this on causes huge make check times
#define GLFW_INCLUDE_NONE  // Prevent GLFW from including OpenGL headers
#define RAYMATH_STATIC_INLINE

#include <inttypes.h>  // IWYU pragma: keep
#include <stddef.h>    // IWYU pragma: keep

#define U8                                  uint8_t
#define U16                                 uint16_t
#define U32                                 uint32_t
#define U64                                 uint64_t
#define S8                                  int8_t
#define S16                                 int16_t
#define S32                                 int32_t
#define S64                                 int64_t
#define F32                                 float
#define F64                                 double
#define C8                                  char
#define SZ                                  size_t
#define BOOL                                bool
#define U8_MIN                              0
#define U8_MAX                              UINT8_MAX
#define U16_MIN                             0
#define U16_MAX                             UINT16_MAX
#define U32_MIN                             0
#define U32_MAX                             UINT32_MAX
#define U64_MIN                             0
#define U64_MAX                             UINT64_MAX
#define S8_MIN                              INT8_MIN
#define S8_MAX                              INT8_MAX
#define S16_MIN                             INT16_MIN
#define S16_MAX                             INT16_MAX
#define S32_MIN                             INT32_MIN
#define S32_MAX                             INT32_MAX
#define S64_MIN                             INT64_MIN
#define S64_MAX                             INT64_MAX
#define F32_MIN                             1.17549435e-38F // Smallest positive normalized float according to IEEE 754
#define F32_MAX                             3.40282347e+38F // Largest positive finite float according to IEEE 754
#define F64_MIN                             2.2250738585072014e-308 // Smallest positive normalized double
#define F64_MAX                             1.7976931348623157e+308 // Largest positive finite double
#define C8_MIN                              CHAR_MIN
#define C8_MAX                              CHAR_MAX
#define SZ_MIN                              0
#define SZ_MAX                              SIZE_MAX
using EID =                                 U32;
#define unused(x)                           (void)x;
#define fwd_decl_enum(x, t)                 enum x : t
#define fwd_decl(x)                         struct x
#define fwd_decl_ns(ns, x)                  namespace ns { class x; }
#define PERC(perc, X)                       ((X) * ((F32)(perc) / 100.0F))
#define EPSILON                             0.000001F
#define F32_EQUAL(a, b)                     (math_abs_f32(((a) - (b))) <= (EPSILON))
#define BOOL_TO_STR(condition)              ((condition) ? "true" : "false")
#define FLAG_SET(flags, flag)               ((flags) |= (flag))
#define FLAG_CLEAR(flags, flag)             ((flags) &= ~(flag))
#define FLAG_TOGGLE(flags, flag)            ((flags) ^= (flag))
#define FLAG_HAS(flags, flag)               (((flags) & (flag)) != 0)
#define FLAG_ADD_IF(flags, flag, condition) do { if (condition) FLAG_SET(flags, flag); } while(0)
