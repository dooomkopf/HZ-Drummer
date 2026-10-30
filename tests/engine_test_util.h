#pragma once

// Shared by the engine test files: the CHECK macro, the failure count, the test tab.

#include "engine/Meter.h"
#include "engine/PatternEdit.h"
#include "engine/OutputGuard.h"
#include "engine/PatternPlayer.h"
#include "engine/TabParser.h"
#include "engine/MeterCatalog.h"
#include "engine/Resampler.h"
#include "engine/SamplePlayer.h"
#include "engine/Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace hz;

extern int failures;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

inline const char* const skankTab = R"(
name:  Thrash Skank
tags:  thrash, skank
tempo: 220 (180-250)
meter: 4/4
grid:  1/8

cr | X . . . . . . . |
sn | . x . x . x . x |
bd | x . x . x . x . |
)";

void runPlayerTests();   // engine_tests_player.cpp
