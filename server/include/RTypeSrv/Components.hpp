#pragma once

#include <R-Engine/Maths/Vec.hpp>
#include <cstdint>
#include <vector>

/* Examples of ECS Components (NOT FINAL) */

struct Player {
    uint32_t clientId;
};

struct Position {
    r::Vec2f value;
};

struct Velocity {
    r::Vec2f value;
};

struct GameStateSnapshot {
    std::vector<uint8_t> data;
};

struct SnapshotSequence {
    uint32_t sequence_number = 0;
};
