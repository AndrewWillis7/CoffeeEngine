#include "Raycast.h"

#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Renderer/PixelSprite.h"

#include <algorithm>
#include <cmath>

namespace Physics {

GroundHit RaycastDown(const ActorRegistry& actors,
                      float x, float fromY, float maxY,
                      const RigidBody2D* ignore) {
    GroundHit best;
    if (maxY < fromY) return best;

    for (const auto& owned : actors.GetBodies()) {
        const RigidBody2D* body = owned.get();
        if (!body || body == ignore || !body->raycastTarget || body->destroyed) continue;
        if (!body->terrain && !body->collisionShape) continue;

        const float halfW = body->size.x * 0.5f;
        const float left   = body->transform.position.x - halfW;
        const float right  = body->transform.position.x + halfW;

        // The X test carries weight for heightmap ground: SurfaceWorldY clamps an
        // out-of-range X to the nearest column (handy when placing props), so
        // without it a foot walking off a chunk keeps snapping to the edge height
        // out over the void.
        if (x < left || x > right) continue;

        const float halfH = body->size.y * 0.5f;
        const float top    = body->transform.position.y - halfH;
        const float bottom = body->transform.position.y + halfH;
        if (bottom < fromY || top > maxY) continue;

        float surface = 0.0f;
        bool found = false;

        if (body->terrain) {
            // Top of the DIRT, which is what bodies stand on -- the per-pixel
            // path below would plant the foot on a blade of grass instead. It is
            // also exactly what TerrainChunk::ResolveBody uses, so feet and body
            // agree by construction.
            surface = body->terrain->SurfaceWorldY(x, *body);
            found = true;
        } else if (body->sprite && body->size.x > 0.0f && body->size.y > 0.0f) {
            // Per-pixel refinement is what makes carved geometry work: punch a
            // hole and feet drop into it next frame, with no extra bookkeeping.
            const PixelSprite* sprite = body->sprite;
            const int tw = sprite->GetWidth();
            const int th = sprite->GetHeight();
            if (tw > 0 && th > 0) {
                int col = static_cast<int>(std::floor((x - left) / body->size.x * static_cast<float>(tw)));
                col = std::clamp(col, 0, tw - 1);

                int startRow = static_cast<int>(
                    std::floor((std::max(fromY, top) - top) / body->size.y * static_cast<float>(th)));
                if (startRow < 0) startRow = 0;

                for (int row = startRow; row < th; ++row) {
                    if (sprite->IsSolid(col, row)) {
                        surface = top + (static_cast<float>(row) / static_cast<float>(th)) * body->size.y;
                        found = true;
                        break;
                    }
                }
            }
        } else {
            surface = top; // shapeless box: its own top edge
            found = true;
        }

        if (!found || surface < fromY || surface > maxY) continue;
        if (!best.hit || surface < best.y) {
            best.hit = true;
            best.y = surface;
            best.body = body;
        }
    }

    return best;
}

} // namespace Physics