#pragma once
#include "Components.hpp"
#include "GameEvents.hpp"
#include <R-Engine/Application.hpp>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include <iomanip>

/* EXAMPLES OF ECS SYSTEMS (NOT FINAL) */

inline void spawn_player_system(r::ecs::Commands& commands) {
    const int MAX_PLAYERS = 10; // Number max of clients in a game
    
    std::cout << "===> [ECS] spawn_player_system: Spawning " << MAX_PLAYERS << " player slots..." << std::endl;

    for (float i = 0; i < MAX_PLAYERS; ++i) {
        float start_x = 100.0f;
        float start_y = 100.0f + (i * 50.0f);

        commands.spawn(
            Player{0}, 
            Position{{start_x, start_y}},
            Velocity{{0.0f, 0.0f}}
        );
    }
    std::cout << "===> [ECS] Player slots created." << std::endl;
}

inline void handle_player_input_system(
    r::ecs::EventReader<PlayerInputEvent> events,
    r::ecs::Query<r::ecs::Mut<Velocity>, r::ecs::Ref<Player>> query
) {
    const float PLAYER_SPEED = 200.0f;

    for (const auto& event : events) {
        for (auto [velocity, player] : query) {
            if (player.ptr->clientId == event.clientId) {
                switch (event.action) {
                    case PlayerAction::MoveUp:    velocity.ptr->value.y = -PLAYER_SPEED; break;
                    case PlayerAction::MoveDown:  velocity.ptr->value.y = PLAYER_SPEED;  break;
                    case PlayerAction::MoveLeft:  velocity.ptr->value.x = -PLAYER_SPEED; break;
                    case PlayerAction::MoveRight: velocity.ptr->value.x = PLAYER_SPEED;  break;
                    case PlayerAction::Stop:      velocity.ptr->value = {0.0f, 0.0f};    break;
                    default: velocity.ptr->value = {0.0f, 0.0f};    break;
                }
            }
        }
    }
}

inline void debug_print_player_positions_system(
    r::ecs::Query<r::ecs::Ref<Player>, r::ecs::Ref<Position>> query
) {
    for (auto [player, position] : query) {
        if (player.ptr->clientId != 0) {
            std::cout << "[SERVER DEBUG] Player " << player.ptr->clientId 
                      << ": Position (" << std::fixed << std::setprecision(1) << position.ptr->value.x
                      << ", " << position.ptr->value.y << ")" << std::endl;
        }
    }
}

inline void movement_system(
    r::ecs::Res<r::core::FrameTime> time,
    r::ecs::Query<r::ecs::Mut<Position>, r::ecs::Ref<Velocity>> query
) {
    for (auto [position, velocity] : query) {
        const float delta = time.ptr->delta_time;

        const float dx = velocity.ptr->value.x * delta;
        const float dy = velocity.ptr->value.y * delta;

        position.ptr->value.x += dx;
        position.ptr->value.y += dy;
    }
}

template<typename T>
void write_big_endian(uint8_t*& ptr, T value) {
    for (int i = sizeof(T) - 1; i >= 0; --i) {
        *ptr++ = (value >> (i * 8)) & 0xFF;
    }
}

inline void write_big_endian(uint8_t*& ptr, float value) {
    uint32_t as_int;
    std::memcpy(&as_int, &value, sizeof(float));
    write_big_endian(ptr, as_int);
}


inline void create_snapshot_system(
    r::ecs::Commands& commands,
    r::ecs::ResMut<SnapshotSequence> snapshot_seq,
    r::ecs::Query<r::ecs::Ref<Position>, r::ecs::Ref<Player>> query 
) {
    snapshot_seq.ptr->sequence_number++;

    uint32_t entity_count = 0;
    for (auto [position, player] : query) {
        if (player.ptr->clientId != 0) {
            entity_count++;
        }
    }

    if (entity_count == 0) {
        commands.insert_resource(GameStateSnapshot{});
        return;
    }

    size_t payload_size = sizeof(uint32_t) + (entity_count * (sizeof(uint32_t) + sizeof(float) * 2));
    std::vector<uint8_t> snapshot_data(payload_size);
    uint8_t* ptr = snapshot_data.data();

    write_big_endian(ptr, entity_count);

    for (auto it = query.begin(); it != query.end(); ++it) {
        auto [position, player] = *it;
        if (player.ptr->clientId == 0) continue;

        r::ecs::Entity entity_id = static_cast<uint32_t>(it.entity());
        float x = position.ptr->value.x;
        float y = position.ptr->value.y;

        write_big_endian(ptr, entity_id);
        write_big_endian(ptr, x);
        write_big_endian(ptr, y);
    }
    
    commands.insert_resource(GameStateSnapshot{std::move(snapshot_data)});
}

inline void assign_player_slot_system(
    r::ecs::EventReader<AssignPlayerSlotEvent> events,
    r::ecs::Query<r::ecs::Mut<Player>> query
) {
    for (const auto& event : events) {
        bool slot_found = false;
        for (auto [player] : query) {
            if (player.ptr->clientId == 0) {
                player.ptr->clientId = event.clientId;
                slot_found = true;
                std::cout <<"[ECS] Client ID " << event.clientId << " has been asigned to an entity player." << std::endl;
                break;
            }
        }

        if (!slot_found) {
            std::cerr << "[ECS] No slot available: " << event.clientId << std::endl;
        }
    }
}