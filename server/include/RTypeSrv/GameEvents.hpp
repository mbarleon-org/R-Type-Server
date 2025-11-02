#pragma once
#include <cstdint>

/* Game Events ECS Examples (NOT FINAL) */

enum class PlayerAction {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Stop
};

struct PlayerInputEvent {
    uint32_t clientId;
    PlayerAction action;
};

struct AssignPlayerSlotEvent {
    uint32_t clientId;
};