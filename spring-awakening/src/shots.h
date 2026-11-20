// shots.h — shot table.
#pragma once
#include "scene.h"
#include "geom.h"

struct FrameCtx { int f, nf, W, H; double t; Camera cam; Env env; };
struct ShotDef { const char* name; float dur; void (*init)(Scene&); void (*frame)(Scene&, FrameCtx&); };

#include "shot_test.h"
#include "shot_seedling.h"
#include "shot_ice.h"
#include "shot_crocus.h"
#include "shot_fern.h"
#include "shot_bud.h"
#include "shot_ladybird.h"
#include "shot_snail.h"
#include "shot_wide.h"

static const ShotDef SHOTS[] = {
    {"test", 2.0f, test_init, test_frame},
    {"seedling", 9.5f, sdl::init, sdl::frame},
    {"icicles", 6.0f, ice::init, ice::frameWide},
    {"drop", 4.5f, ice::init, ice::frameTip},
    {"thaw", 7.0f, cro::init, cro::frameWide},
    {"crocus", 7.5f, cro::init, cro::frameClose},
    {"fern", 7.5f, fern::init, fern::frame},
    {"bud", 7.5f, bud::init, bud::frame},
    {"rain", 7.0f, bud::init, bud::frameRain},
    {"ladybird", 8.0f, lbs::init, lbs::frameClimb},
    {"takeoff", 4.0f, lbs::init, lbs::frameFly},
    {"snail", 7.0f, snl::init, snl::frame},
    {"winter", 7.5f, wid::initWinter, wid::frame},
    {"spring", 10.0f, wid::initSpring, wid::frame},
};
static inline const ShotDef* findShot(const char* n) {
    for (const ShotDef& s : SHOTS) if (!strcmp(s.name, n)) return &s;
    return nullptr;
}
