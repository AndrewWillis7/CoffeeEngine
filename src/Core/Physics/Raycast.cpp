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
        if (!body || body == ignore || !body->raycastTarget) continue;
        if (!body->terrain && !body->collisionShape) continue;

        const float halfW = body->size.x * 0.5f;
        const float halfH = body->size.y * 0.5f;
        const float left   = body->transform.position.x - halfW;
        const float right  = body->transform.position.x + halfW;
        const float top    = body->transform.position.y - halfH;
        const float bottom = body->transform.position.y + halfH;

        // The X test matters more than it looks for heightmap ground:
        // SurfaceWorldY deliberately CLAMPS an out-of-range X to the
        // nearest column (convenient when placing props), so without
        // this a foot walking off the end of a chunk would keep snapping
        // to the chunk's edge height out over the void.
        if (x < left || x > right) continue;
        if (bottom < fromY || top > maxY) continue;

        float surface = 0.0f;
        bool found = false;

        if (body->terrain) {
            // Ask the heightmap directly rather than probing pixels.
            // This returns the top of the DIRT, which is what bodies
            // stand on; the per-pixel path below would plant the foot on
            // a blade of grass instead, since blades are solid pixels in
            // the same sprite standing up to grassMaxHeight above the
            // real surface. It is also the exact number
            // TerrainChunk::ResolveBody stands the collider on, so feet
            // and body agree by construction rather than by coincidence.
            surface = body->terrain->SurfaceWorldY(x, *body);
            found = true;
        } else if (body->sprite && body->size.x > 0.0f && body->size.y > 0.0f) {
            // Per-pixel refinement is what makes this work against
            // carved geometry rather than only flat boxes: punch a hole
            // with PunchCircle and feet drop into it the next frame,
            // with no extra bookkeeping anywhere.
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