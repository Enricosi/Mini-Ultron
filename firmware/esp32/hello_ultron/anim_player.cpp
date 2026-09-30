#include "anim_player.h"
#include "anim_library.h"
#include "features.h"

#if MINI_ULTRON_ENABLE_ANIM_FS
#include <LittleFS.h>
#endif

enum AnimSource { SRC_NONE, SRC_PROGMEM, SRC_FS };

static AnimSource source = SRC_NONE;
static const uint8_t *progmemData = nullptr;
#if MINI_ULTRON_ENABLE_ANIM_FS
static File fsFile;
static bool fsMounted = false;
#endif

static AnimHeader header;
static uint8_t frameBuffer[ANIM_MAX_FRAME_BYTES];
static uint16_t frameIndex = 0;
static unsigned long nextFrameMs = 0;
static bool looping = false;
static bool playing = false;

static bool animNameIsValid(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > 24) return false;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// Copies len bytes starting at offset from whichever source is open.
static bool readSource(size_t offset, uint8_t *dst, size_t len) {
    if (source == SRC_PROGMEM) {
        memcpy_P(dst, progmemData + offset, len);
        return true;
    }
#if MINI_ULTRON_ENABLE_ANIM_FS
    if (source == SRC_FS) {
        return fsFile.seek(offset) && fsFile.read(dst, len) == len;
    }
#endif
    return false;
}

static void closeSource() {
#if MINI_ULTRON_ENABLE_ANIM_FS
    if (fsFile) fsFile.close();
#endif
    source = SRC_NONE;
    progmemData = nullptr;
}

static bool openBuiltin(const char *name, size_t &size) {
    for (size_t i = 0; i < BUILTIN_ANIM_COUNT; i++) {
        if (strcmp(BUILTIN_ANIMS[i].name, name) == 0) {
            source = SRC_PROGMEM;
            progmemData = BUILTIN_ANIMS[i].data;
            size = BUILTIN_ANIMS[i].size;
            return true;
        }
    }
    return false;
}

static bool openFs(const char *name, size_t &size) {
#if MINI_ULTRON_ENABLE_ANIM_FS
    if (!fsMounted) return false;
    char path[40];
    snprintf(path, sizeof(path), "/anim/%s.ula", name);
    if (!LittleFS.exists(path)) return false;
    fsFile = LittleFS.open(path, "r");
    if (!fsFile) return false;
    source = SRC_FS;
    size = fsFile.size();
    return true;
#else
    (void)name;
    (void)size;
    return false;
#endif
}

void animInit() {
#if MINI_ULTRON_ENABLE_ANIM_FS
    // formatOnFail=false: a blank/foreign partition just means "no FS
    // animations", it is never silently wiped.
    fsMounted = LittleFS.begin(false);
    Serial.printf("[anim] LittleFS %s\n", fsMounted ? "mounted" : "NOT mounted (no FS animations)");
#endif
    Serial.printf("[anim] %u built-in animation(s)\n", (unsigned)BUILTIN_ANIM_COUNT);
}

bool animStart(const char *name, bool loop) {
    animStop();
    if (!animNameIsValid(name)) return false;

    size_t size = 0;
    if (!openBuiltin(name, size) && !openFs(name, size)) return false;

    uint8_t raw[ANIM_HEADER_SIZE];
    if (size < ANIM_HEADER_SIZE || !readSource(0, raw, sizeof(raw)) ||
        !animParseHeader(raw, header) || header.totalSize != size) {
        Serial.printf("[anim] '%s' is not a valid .ula for this display, ignored\n", name);
        closeSource();
        return false;
    }

    frameIndex = 0;
    looping = loop;
    nextFrameMs = millis();
    playing = true;
    return true;
}

void animStop() {
    playing = false;
    closeSource();
}

bool animIsPlaying() {
    return playing;
}

bool animUpdate(unsigned long now, const uint8_t **frame) {
    if (!playing) return false;
    if ((long)(now - nextFrameMs) < 0) return false;  // signed: see motion.cpp's elapsedAtLeast()

    if (frameIndex >= header.frameCount) {
        if (!looping) {
            animStop();
            return false;
        }
        frameIndex = 0;
    }

    uint8_t delayRaw[2];
    size_t frameOffset = header.framesOffset + (size_t)frameIndex * header.frameBytes;
    if (!readSource(header.delaysOffset + 2 * (size_t)frameIndex, delayRaw, 2) ||
        !readSource(frameOffset, frameBuffer, header.frameBytes)) {
        Serial.println("[anim] read failed, stopping");
        animStop();
        return false;
    }

    nextFrameMs = now + (delayRaw[0] | (delayRaw[1] << 8));
    frameIndex++;
    *frame = frameBuffer;
    return true;
}

const AnimHeader &animCurrentHeader() {
    return header;
}

void animList(Print &out) {
    for (size_t i = 0; i < BUILTIN_ANIM_COUNT; i++) {
        out.printf("ANIM %s builtin\n", BUILTIN_ANIMS[i].name);
    }
#if MINI_ULTRON_ENABLE_ANIM_FS
    if (!fsMounted) return;
    File dir = LittleFS.open("/anim");
    if (!dir || !dir.isDirectory()) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        String fileName = f.name();
        if (fileName.endsWith(".ula")) {
            out.printf("ANIM %s littlefs\n", fileName.substring(0, fileName.length() - 4).c_str());
        }
    }
#endif
}
