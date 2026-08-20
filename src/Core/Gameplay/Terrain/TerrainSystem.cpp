#include "TerrainSystem.h"
#include "TerrainChunk.h"
#include "../../ActorRegistry.h"
#include "../../Physics/RigidBody2D.h"
#include "../../../Renderer/PixelSprite.h"

void TerrainSystem::Update(ActorRegistry& actors, float deltaTime) {
    const auto& bodies = actors.GetBodies();

    // ---- Pass 1: who parts the grass this frame ------------------------
    m_Disturbers.clear();
    for (const auto& owned : bodies) {
        RigidBody2D* body = owned.get();

        // Terrain doesn't disturb itself or its neighbors -- a chunk's own
        // AABB covers all of its grass, so leaving it in would have every
        // blade permanently flattened by the ground it grows out of.
        if (body->terrain) continue;

        // A camera is a RigidBody2D that happens to hold a Camera2D and
        // has mass -- it moves constantly and would drag a wave of grass
        // around the level with it, invisibly, wherever the view happens
        // to be looking.
        if (body->camera) continue;

        // mass <= 0 is the engine's existing "static/immovable" convention
        // (see RigidBody2D::Integrate and ResolveCollisionWith). Static
        // scenery is never going anywhere, so it can't sweep anything --
        // the player and other dynamic bodies are what the grass reacts
        // to. playerConfig is checked anyway so a scripted, mass-0 player
        // still parts the grass.
        if (body->mass <= 0.0f && !body->playerConfig) continue;

        m_Disturbers.push_back(body);
    }

    // ---- Pass 2: tick each chunk --------------------------------------
    for (const auto& owned : bodies) {
        RigidBody2D* body = owned.get();
        if (!body->terrain || !body->sprite) continue;
        body->terrain->Update(*body->sprite, *body, m_Disturbers, deltaTime);
    }
}