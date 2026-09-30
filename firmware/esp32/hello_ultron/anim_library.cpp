#include "anim_library.h"
#include "src/animations/anim_scan.h"
#include "src/animations/anim_calabaza.h"

// name -> .ula blob. Names must be [a-z0-9_], max 24 chars (animStart() rejects
// anything else), and unique - a LittleFS file with the same name is shadowed.
const BuiltinAnim BUILTIN_ANIMS[] = {
    {"scan", ANIM_SCAN, sizeof(ANIM_SCAN)},  // demo: red eye sweeping side to side
    {"calabaza", ANIM_CALABAZA, sizeof(ANIM_CALABAZA)},  // "La calabaza" dancing cats, green-screen outline, 14 s
};
const size_t BUILTIN_ANIM_COUNT = sizeof(BUILTIN_ANIMS) / sizeof(BUILTIN_ANIMS[0]);
