#include "SceneExplorer.h"
#include "UIPanel.h"
#include "UIStyle.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/Camera2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Renderer/PixelSprite.h"
#include <cstdio>
#include <iterator>

namespace {

constexpr float kRadToDeg = 57.29577951f;

std::string Num(float value, int decimals = 1) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

std::string Pair(float a, float b, int decimals = 1) {
    return Num(a, decimals) + ", " + Num(b, decimals);
}

const char* YesNo(bool value) { return value ? "yes" : "no"; }

} // namespace

SceneExplorer::Kind SceneExplorer::Classify(const RigidBody2D& body, const RigidBody2D* playerActor) {
    if (body.playerConfig)   return &body == playerActor ? Kind::Player : Kind::Actor;
    if (body.camera)         return Kind::Camera;
    if (body.lightEmitter)   return Kind::Light;
    if (body.terrain)        return Kind::Terrain;
    if (body.collisionShape) return Kind::Collider;
    if (body.sprite)         return Kind::Sprite;
    return Kind::Plain;
}

const char* SceneExplorer::KindName(Kind kind) {
    switch (kind) {
        case Kind::Player:   return "Player";
        case Kind::Actor:    return "Actor";
        case Kind::Camera:   return "Camera";
        case Kind::Light:    return "Light";
        case Kind::Terrain:  return "Terrain";
        case Kind::Collider: return "Collider";
        case Kind::Sprite:   return "Sprite";
        default:             return "Body";
    }
}

const char* SceneExplorer::KindPlural(Kind kind) {
    switch (kind) {
        case Kind::Player:   return "Players";
        case Kind::Actor:    return "Actors";
        case Kind::Camera:   return "Cameras";
        case Kind::Light:    return "Lights";
        case Kind::Terrain:  return "Terrain";
        case Kind::Collider: return "Colliders";
        case Kind::Sprite:   return "Sprites";
        default:             return "Bodies";
    }
}

Color SceneExplorer::KindColor(Kind kind) {
    switch (kind) {
        case Kind::Player:   return UITheme::KindPlayer;
        case Kind::Actor:    return UITheme::KindActor;
        case Kind::Camera:   return UITheme::KindCamera;
        case Kind::Light:    return UITheme::KindLight;
        case Kind::Terrain:  return UITheme::KindTerrain;
        case Kind::Collider: return UITheme::KindCollider;
        case Kind::Sprite:   return UITheme::KindSprite;
        default:             return UITheme::KindPlain;
    }
}

std::string SceneExplorer::DisplayName(const RigidBody2D& body, size_t index, Kind kind) {
    std::string label = "#" + std::to_string(index) + " ";
    return label + (body.name.empty() ? KindName(kind) : body.name);
}

RigidBody2D* SceneExplorer::Selected(const ActorRegistry& actors) const {
    if (!m_Selected) return nullptr;
    for (const auto& body : actors.GetBodies()) {
        if (body.get() == m_Selected) return m_Selected;
    }
    return nullptr;
}

void SceneExplorer::PruneDeadPointers(const ActorRegistry& actors) {
    const auto& bodies = actors.GetBodies();

    bool selectedAlive = false;
    for (const auto& body : bodies) {
        if (body.get() == m_Selected) { selectedAlive = true; break; }
    }
    if (!selectedAlive) m_Selected = nullptr;

    if (m_Expanded.empty()) return;
    for (auto it = m_Expanded.begin(); it != m_Expanded.end();) {
        bool alive = false;
        for (const auto& body : bodies) {
            if (body.get() == *it) { alive = true; break; }
        }
        it = alive ? std::next(it) : m_Expanded.erase(it);
    }
}

void SceneExplorer::ToggleExpanded(const RigidBody2D* body) {
    if (!m_Expanded.erase(body)) m_Expanded.insert(body);
}

void SceneExplorer::SetAllExpanded(const ActorRegistry& actors, bool expanded) {
    m_Expanded.clear();
    if (!expanded) return;
    for (const auto& body : actors.GetBodies()) m_Expanded.insert(body.get());
}

void SceneExplorer::EmitComponents(UIPanel& panel, const RigidBody2D& body, int depth) {
    UIPanel::RowSpec row;
    row.depth = depth;
    row.accent = Color::Transparent();
    row.labelColor = UITheme::TextDim;
    row.valueColor = UITheme::Value;

    auto line = [&](const char* label, const std::string& value, Color color = UITheme::Value) {
        row.label = label;
        row.value = value;
        row.valueColor = color;
        panel.Row(row);
    };

    line("position", Pair(body.transform.position.x, body.transform.position.y));
    if (body.transform.rotation != 0.0f) line("rotation", Num(body.transform.rotation * kRadToDeg) + " deg");
    if (body.transform.scale.x != 1.0f || body.transform.scale.y != 1.0f) {
        line("scale", Pair(body.transform.scale.x, body.transform.scale.y, 2));
    }
    line("size", Pair(body.size.x, body.size.y, 0));

    const bool moving = body.velocity.LengthSquared() > 0.0001f;
    line("velocity", Pair(body.velocity.x, body.velocity.y), moving ? UITheme::Warn : UITheme::TextFaint);
    line("mass / drag", Num(body.mass, 2) + " / " + Num(body.drag, 2));
    if (body.mass > 0.0f) line("grounded", YesNo(body.IsGrounded()), body.IsGrounded() ? UITheme::Good : UITheme::TextFaint);

    if (body.sprite) {
        line("sprite", std::to_string(body.sprite->GetWidth()) + " x " + std::to_string(body.sprite->GetHeight()),
             UITheme::KindSprite);
    }

    if (body.collisionShape) {
        const CollisionShape2D& shape = *body.collisionShape;
        std::string text = shape.GetType() == CollisionShape2D::Type::Box
            ? "box " + Pair(shape.GetHalfExtents().x * 2.0f, shape.GetHalfExtents().y * 2.0f, 0)
            : "circle r" + Num(shape.GetRadius(), 1);
        line("collider", text, UITheme::KindCollider);
    }

    if (body.playerConfig) {
        const PlayerActorConfig& config = *body.playerConfig;
        line("move / jump", Num(config.moveSpeed, 0) + " / " + Num(config.jumpForce, 0), UITheme::KindPlayer);
        line("input", YesNo(config.inputEnabled), config.inputEnabled ? UITheme::Good : UITheme::Bad);
    }

    if (body.camera) {
        const Camera2D& camera = *body.camera;
        const Vector2 view = camera.EffectiveViewportSize();
        line("viewport", Pair(view.x, view.y, 0), UITheme::KindCamera);
        line("zoom out", std::to_string(camera.GetZoomOut()) + "x", UITheme::KindCamera);
        line("active", YesNo(camera.active), camera.active ? UITheme::Good : UITheme::TextFaint);
        line("smoothing", Num(camera.followSmoothing, 2));
        if (camera.focusOffset.x != 0.0f || camera.focusOffset.y != 0.0f) {
            line("focus offset", Pair(camera.focusOffset.x, camera.focusOffset.y, 0));
        }
        line("follows", camera.followTarget ? "target" : "nothing",
             camera.followTarget ? UITheme::Value : UITheme::TextFaint);
    }

    if (body.lightEmitter) {
        const LightEmitterConfig& light = *body.lightEmitter;
        const bool cone = light.type == LightEmitterConfig::Type::Cone;
        line("light", cone ? "cone" : "point", UITheme::KindLight);
        line("radius", Num(light.radius, 0), UITheme::KindLight);
        line("brightness", Num(light.brightness, 2),
             light.brightness > 0.0f ? UITheme::KindLight : UITheme::TextFaint);
        line("falloff", Num(light.falloffExponent, 2));
        if (light.toneSteps > 0) line("tone steps", std::to_string(light.toneSteps));
        if (cone) {
            line("cone angle", Num(light.coneAngleRad * kRadToDeg, 0) + " deg");
            line("cone aim", Num(light.coneDirectionRad * kRadToDeg, 0) + " deg");
        }
        if (light.flicker) line("flicker", Num(light.flickerSpeed, 1) + " hz", UITheme::Warn);
        line("tint", Pair(light.color.r, light.color.g, 2) + ", " + Num(light.color.b, 2), light.color);
    }

    if (body.terrain) {
        const TerrainChunk& terrain = *body.terrain;
        line("terrain", std::to_string(terrain.GetWidth()) + " x " + std::to_string(terrain.GetHeight()),
             UITheme::KindTerrain);
        line("blades", std::to_string(terrain.GetBladeCount()), UITheme::KindTerrain);
        line("seed", std::to_string(terrain.seed));
        line("step height", Num(terrain.maxStepHeight, 1));
        line("surface at x", Num(terrain.SurfaceWorldY(body.transform.position.x, body), 1));
    }

    line("shader", body.shader ? "custom" : "default",
         body.shader ? UITheme::Value : UITheme::TextFaint);
    line("blocks light", YesNo(body.lightBlocking), body.lightBlocking ? UITheme::Warn : UITheme::TextFaint);
    line("raycast target", YesNo(body.raycastTarget), body.raycastTarget ? UITheme::Value : UITheme::TextFaint);
    line("color", Num(body.color.r, 2) + ", " + Num(body.color.g, 2) + ", " + Num(body.color.b, 2), body.color);
}

void SceneExplorer::DrawTree(UIPanel& panel, ActorRegistry& actors) {
    PruneDeadPointers(actors);
    const auto& bodies = actors.GetBodies();
    const RigidBody2D* playerActor = actors.GetPlayerActor();

    if (panel.Button("Expand all", panel.GetWidth() * 0.32f)) SetAllExpanded(actors, true);
    panel.SameLine();
    if (panel.Button("Collapse", panel.GetWidth() * 0.26f)) SetAllExpanded(actors, false);
    panel.SameLine();
    if (panel.Button("Deselect", panel.GetWidth() * 0.26f)) ClearSelection();

    panel.Toggle("Group by type", &m_Grouped);

    // Per-kind visibility, three to a line, each labelled with its live count so
    // the filter row doubles as a census of the scene.
    int counts[kKindCount] = {};
    for (const auto& body : bodies) ++counts[static_cast<int>(Classify(*body, playerActor))];

    const float halfWidth = panel.GetWidth() * 0.44f;
    for (int i = 0; i < kKindCount; ++i) {
        const Kind kind = static_cast<Kind>(i);
        std::string label = std::string(KindPlural(kind)) + " (" + std::to_string(counts[i]) + ")";
        if (i % 2 == 1) panel.SameLine();
        panel.Toggle(label, &m_ShowKind[i], halfWidth, KindColor(kind));
    }
    panel.Separator();

    if (bodies.empty()) {
        panel.Label("Registry is empty.", UITheme::TextFaint);
        return;
    }

    // One pass per kind when grouped, one pass overall when not. Either way a
    // body is emitted exactly once, so the two paths share the row builder.
    auto emitBody = [&](RigidBody2D& body, size_t index, int depth) {
        const Kind kind = Classify(body, playerActor);
        UIPanel::RowSpec row;
        row.label = DisplayName(body, index, kind);
        row.value = "(" + Num(body.transform.position.x, 0) + "," + Num(body.transform.position.y, 0) + ")";
        row.accent = KindColor(kind);
        row.labelColor = UITheme::Text;
        row.valueColor = UITheme::TextDim;
        row.depth = depth;
        row.hasChildren = true;
        row.expanded = IsExpanded(&body);
        row.selected = m_Selected == &body;

        switch (panel.Row(row)) {
            case UIPanel::RowHit::Expander:
                ToggleExpanded(&body);
                break;
            case UIPanel::RowHit::Body:
                // Clicking the selected body again clears it, so the world
                // gizmo can be dismissed without hunting for the button.
                m_Selected = (m_Selected == &body) ? nullptr : &body;
                break;
            default:
                break;
        }

        if (IsExpanded(&body)) EmitComponents(panel, body, depth + 1);
    };

    if (!m_Grouped) {
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (!m_ShowKind[static_cast<int>(Classify(*bodies[i], playerActor))]) continue;
            emitBody(*bodies[i], i, 0);
        }
        return;
    }

    for (int k = 0; k < kKindCount; ++k) {
        if (!m_ShowKind[k] || counts[k] == 0) continue;
        const Kind kind = static_cast<Kind>(k);

        UIPanel::RowSpec group;
        group.label = KindPlural(kind);
        group.value = std::to_string(counts[k]);
        group.accent = KindColor(kind);
        group.labelColor = KindColor(kind);
        group.valueColor = UITheme::TextDim;
        group.hasChildren = true;
        group.expanded = m_GroupOpen[k];

        if (panel.Row(group) != UIPanel::RowHit::None) m_GroupOpen[k] = !m_GroupOpen[k];
        if (!m_GroupOpen[k]) continue;

        for (size_t i = 0; i < bodies.size(); ++i) {
            if (Classify(*bodies[i], playerActor) != kind) continue;
            emitBody(*bodies[i], i, 1);
        }
    }
}

void SceneExplorer::DrawInspector(UIPanel& panel, ActorRegistry& actors) {
    RigidBody2D* body = Selected(actors);
    if (!body) {
        panel.Label("Click a body in the explorer.", UITheme::TextFaint);
        return;
    }

    size_t index = 0;
    const auto& bodies = actors.GetBodies();
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].get() == body) { index = i; break; }
    }

    const Kind kind = Classify(*body, actors.GetPlayerActor());
    UIPanel::RowSpec title;
    title.label = DisplayName(*body, index, kind);
    title.value = KindName(kind);
    title.accent = KindColor(kind);
    title.labelColor = UITheme::Text;
    title.valueColor = KindColor(kind);
    panel.Row(title);

    EmitComponents(panel, *body, 1);
    panel.Spacing();

    // Actions, kept to the ones that are safe to fire mid-frame: they only
    // touch fields the next Integrate() reads anyway.
    if (panel.Button("Stop", panel.GetWidth() * 0.28f)) body->velocity = Vector2::Zero();
    panel.SameLine();
    if (panel.Button("Nudge up", panel.GetWidth() * 0.32f)) body->transform.position.y -= 8.0f;
    panel.SameLine();
    if (panel.Button("Wake", panel.GetWidth() * 0.24f)) body->velocity.y -= 60.0f;

    panel.Toggle("Blocks light", &body->lightBlocking);
    panel.Toggle("Raycast target", &body->raycastTarget);

    if (body->playerConfig) {
        panel.Toggle("Input enabled", &body->playerConfig->inputEnabled);
        panel.Slider("Move speed", &body->playerConfig->moveSpeed, 0.0f, 600.0f, 0);
        panel.Slider("Jump force", &body->playerConfig->jumpForce, 0.0f, 1200.0f, 0);
    }

    if (body->lightEmitter) {
        panel.Slider("Radius", &body->lightEmitter->radius, 0.0f, 600.0f, 0);
        panel.Slider("Brightness", &body->lightEmitter->brightness, 0.0f, 3.0f, 2);
        panel.Slider("Falloff", &body->lightEmitter->falloffExponent, 0.25f, 6.0f, 2);
        panel.SliderInt("Tone steps", &body->lightEmitter->toneSteps, 0, 8);
        panel.Toggle("Flicker", &body->lightEmitter->flicker);
    }

    if (body->camera) {
        int zoom = body->camera->GetZoomOut();
        if (panel.SliderInt("Zoom out", &zoom, 1, 8)) body->camera->SetZoomOut(zoom);
        panel.Slider("Follow smoothing", &body->camera->followSmoothing, 0.0f, 20.0f, 2);
    }
}
