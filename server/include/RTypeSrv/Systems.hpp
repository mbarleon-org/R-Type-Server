#pragma once
#include "Components.hpp"
#include "GameEvents.hpp"
#include <R-Engine/Application.hpp>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>

/* EXAMPLES OF ECS SYSTEMS (NOT FINAL) */

inline void spawn_player_system(r::ecs::Commands& commands) {
    commands.spawn(
        Player{0},
        Position{{100.0f, 300.0f}},
        Velocity{{0.0f, 0.0f}}
    );
    std::cout << "===> [ECS] spawn_player_system: Player spawned in the scene.";
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

inline void movement_system(
    r::ecs::Res<r::core::FrameTime> time,
    r::ecs::Query<r::ecs::Mut<Position>, r::ecs::Ref<Velocity>> query
) {
    for (auto [position, velocity] : query) {
        position.ptr->value += velocity.ptr->value * time.ptr->delta_time;
    }
}

inline void create_snapshot_system(
    r::ecs::Commands& commands,
    r::ecs::ResMut<SnapshotSequence> snapshot_seq,
    r::ecs::Query<r::ecs::Ref<Position>, r::ecs::Ref<Player>> query 
) {
    std::vector<uint8_t> snapshot_data;
    
    // --- SNAPSHOT (Example) ---
    // [Header: uint32_t entity_count]
    // [Entity 1: uint32_t entity_id, float x, float y]
    // [Entity 2: uint32_t entity_id, float x, float y]
    // ...

    snapshot_seq.ptr->sequence_number++;
    uint32_t current_seq = snapshot_seq.ptr->sequence_number;

    uint32_t entity_count = static_cast<uint32_t>(query.size());

    size_t header_size = sizeof(uint32_t) * 2;
    snapshot_data.resize(header_size);
    memcpy(snapshot_data.data(), &current_seq, sizeof(uint32_t));
    memcpy(snapshot_data.data() + sizeof(uint32_t), &entity_count, sizeof(uint32_t));

    for (auto it = query.begin(); it != query.end(); ++it) {
        auto [position, player] = *it;
        
        r::ecs::Entity entity_id = it.entity();
        float x = position.ptr->value.x;
        float y = position.ptr->value.y;
        
        size_t current_size = snapshot_data.size();
        snapshot_data.resize(current_size + sizeof(r::ecs::Entity) + sizeof(float) * 2);
        
        memcpy(snapshot_data.data() + current_size, &entity_id, sizeof(r::ecs::Entity));
        current_size += sizeof(r::ecs::Entity);
        
        memcpy(snapshot_data.data() + current_size, &x, sizeof(float));
        current_size += sizeof(float);
        
        memcpy(snapshot_data.data() + current_size, &y, sizeof(float));
    }

    commands.insert_resource(GameStateSnapshot{snapshot_data});
}