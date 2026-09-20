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

        // A chunk's own AABB covers all of its grass, so leaving terrain in would
        // permanently flatten every blade under the ground it grows out of.
        if (body->terrain) continue;

        // A camera is a body with mass that moves constantly, and would drag an
        // invisible wave of grass around wherever the view is looking.
        if (body->camera) continue;

        // mass <= 0 is the engine's "static/immovable" convention: scenery never
        // moves, so it can't sweep anything. playerConfig is checked anyway so a
        // scripted, mass-0 player still parts the grass.
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