#include "SceneEditor.h"
#include "Core/ActorRegistry.h"
#include "Core/ScriptEngine.h"
#include "Core/Input/KeyMap.h"
#include "Core/Input/UserInputService.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Physics/Raycast.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Gameplay/Camera2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Gameplay/UI/SceneExplorer.h"
#include "Core/Gameplay/UI/UIPanel.h"
#include "Core/Gameplay/UI/UIStyle.h"
#include "Renderer/Font.h"
#include "Renderer/PixelSprite.h"
#include "Renderer/Renderer2D.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <iterator>

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Free-camera zoom, as a multiple of how much world the game camera shows.
// Only clean ratios, so every texel stays the same whole number of screen
// pixels -- or, zoomed out, the same fraction of one.
constexpr float kZooms[] = {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
constexpr int kZoomCount = static_cast<int>(sizeof(kZooms) / sizeof(kZooms[0]));
constexpr int kDefaultZoom = 2;

constexpr size_t kMaxUndo = 200;

// Screen pixels, before UI scale.
constexpr float kHandle = 7.0f;     // handle square
constexpr float kHitSlop = 6.0f;    // how near counts as on a handle
constexpr float kArrow = 44.0f;     // move-axis arrow length
constexpr float kRingPad = 16.0f;   // rotate ring, beyond the body
constexpr float kIconRadius = 7.0f; // a light with nothing drawn
constexpr float kDeadZone = 3.0f;   // travel before a press becomes a drag
constexpr float kPanSpeed = 600.0f; // keyboard pan, per second, at any zoom

const char* const kToolNames[] = {"MOVE", "ROTATE", "SCALE", "COLLIDER"};
const char* const kToolButtons[] = {"Move (Q)", "Rotate (E)", "Scale (R)", "Collider (C)"};

std::string Num(float value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

std::string Pair(const Vector2& v, int decimals) {
    return Num(v.x, decimals) + ", " + Num(v.y, decimals);
}

std::string ZoomText(float zoom) {
    return std::to_string(static_cast<int>(std::lround(100.0f / zoom))) + "%";
}

float SnapTo(float value, float step) {
    return step > 0.0f ? std::round(value / step) * step : value;
}

// Snaps a scale factor's size but keeps its sign, and never to zero: a
// zero-width body cannot be grabbed again.
float SnapMagnitude(float value, float step) {
    const float magnitude = std::max(step, SnapTo(std::fabs(value), step));
    return value < 0.0f ? -magnitude : magnitude;
}

float SafeRatio(float a, float b) {
    return std::fabs(b) > 0.0001f ? a / b : 1.0f;
}

// Into (-pi, pi], so the inspector reads -90 rather than 270 after a few
// turns of the ring.
float WrapAngle(float angle) {
    angle = std::fmod(angle + kPi, 2.0f * kPi);
    if (angle < 0.0f) angle += 2.0f * kPi;
    return angle - kPi;
}

float DistanceToSegment(const Vector2& p, const Vector2& a, const Vector2& b) {
    const Vector2 ab = b - a;
    const float lengthSq = ab.LengthSquared();
    const float t = lengthSq > 0.0f ? std::clamp((p - a).Dot(ab) / lengthSq, 0.0f, 1.0f) : 0.0f;
    return Vector2::Distance(p, a + ab * t);
}

std::string ScenePathFor(const std::string& scriptPath) {
    const size_t slash = scriptPath.find_last_of("/\\");
    const size_t dot = scriptPath.rfind('.');
    const bool hasExtension = dot != std::string::npos && (slash == std::string::npos || dot > slash);
    return (hasExtension ? scriptPath.substr(0, dot) : scriptPath) + ".scene";
}

Color GroupColor(SceneFields::Group group) {
    switch (group) {
        case SceneFields::Group::Transform: return UITheme::Accent;
        case SceneFields::Group::Body:      return UITheme::KindSprite;
        case SceneFields::Group::Collider:  return UITheme::KindCollider;
        case SceneFields::Group::Light:     return UITheme::KindLight;
        case SceneFields::Group::Actor:     return UITheme::KindActor;
        case SceneFields::Group::Script:    return UITheme::KindPlayer;
        case SceneFields::Group::World:     return UITheme::KindCamera;
        case SceneFields::Group::Camera:    return UITheme::KindCamera;
        case SceneFields::Group::Terrain:   return UITheme::KindTerrain;
        case SceneFields::Group::Asset:     return UITheme::KindSprite;
        default:                            return UITheme::TextDim;
    }
}

Color Opaque(const Color& color) { return {color.r, color.g, color.b, 1.0f}; }
Color Faded(const Color& color, float alpha) { return {color.r, color.g, color.b, color.a * alpha}; }

// --- Screen-space drawing ---------------------------------------------------
// Gizmos draw in screen space, not through the camera: handles have to be a
// fixed number of pixels whatever the zoom, and hit-testing happens in the
// same pixels the user is pointing at.

void DrawLine(Renderer2D& renderer, const Vector2& a, const Vector2& b, float thickness, const Color& color) {
    const Vector2 delta = b - a;
    const float length = delta.Length();
    if (length < 0.5f) return;
    Transform2D transform;
    transform.position = (a + b) * 0.5f;
    transform.rotation = std::atan2(delta.y, delta.x);
    // Overshoots by the thickness, so consecutive segments meet without a notch.
    renderer.DrawScreenQuad(transform, {length + thickness, thickness}, color, nullptr);
}

void DrawSquare(Renderer2D& renderer, const Vector2& center, float size, const Color& color, float rotation = 0.0f) {
    Transform2D transform;
    transform.position = center;
    transform.rotation = rotation;
    renderer.DrawScreenQuad(transform, {size, size}, color, nullptr);
}

void DrawKnob(Renderer2D& renderer, const Vector2& center, float size, const Color& fill, bool hot) {
    DrawSquare(renderer, center, size + 2.0f, UITheme::PanelBg);
    DrawSquare(renderer, center, size, hot ? UITheme::Accent : fill);
}

void DrawCircle(Renderer2D& renderer, const Vector2& center, float radius, float thickness, const Color& color) {
    if (radius < 0.5f) return;
    const int segments = std::clamp(static_cast<int>(radius / 4.0f), 16, 96);
    Vector2 previous{center.x + radius, center.y};
    for (int i = 1; i <= segments; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const Vector2 current{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        DrawLine(renderer, previous, current, thickness, color);
        previous = current;
    }
}

void DrawArrow(Renderer2D& renderer, const Vector2& from, const Vector2& to, float thickness,
               float head, const Color& color) {
    DrawLine(renderer, from, to, thickness, color);
    const Vector2 direction = (to - from).Normalized();
    const Vector2 side{-direction.y, direction.x};
    const Vector2 back = to - direction * head;
    DrawLine(renderer, to, back + side * (head * 0.5f), thickness, color);
    DrawLine(renderer, to, back - side * (head * 0.5f), thickness, color);
}

void DrawRectOutline(Renderer2D& renderer, const Vector2& center, const Vector2& half, float thickness, const Color& color) {
    const Vector2 a{center.x - half.x, center.y - half.y};
    const Vector2 b{center.x + half.x, center.y - half.y};
    const Vector2 c{center.x + half.x, center.y + half.y};
    const Vector2 d{center.x - half.x, center.y + half.y};
    DrawLine(renderer, a, b, thickness, color);
    DrawLine(renderer, b, c, thickness, color);
    DrawLine(renderer, c, d, thickness, color);
    DrawLine(renderer, d, a, thickness, color);
}

// Corners and edge midpoints, clockwise from top-left. The same eight serve
// the scale tool and the collider tool. Bits as SceneEditor's kLeft=1,
// kRight=2, kTop=4, kBottom=8.
constexpr int kHandleMasks[] = {1 | 4, 4, 2 | 4, 2, 2 | 8, 8, 1 | 8, 1};

// Just the corners, in outline order.
constexpr int kCorners[] = {1 | 4, 2 | 4, 2 | 8, 1 | 8};

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

SceneEditor::SceneEditor(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input,
                         ScriptEngine& scripts, SceneExplorer& explorer)
    : m_Actors(actors), m_Renderer(renderer), m_Input(input), m_Scripts(scripts), m_Explorer(explorer),
      m_Font(std::make_unique<Font>()) {
    m_Keys.q = KeyMap::Get("Q");
    m_Keys.e = KeyMap::Get("E");
    m_Keys.r = KeyMap::Get("R");
    m_Keys.c = KeyMap::Get("C");
    m_Keys.f = KeyMap::Get("F");
    m_Keys.z = KeyMap::Get("Z");
    m_Keys.y = KeyMap::Get("Y");
    m_Keys.s = KeyMap::Get("S");
    m_Keys.w = KeyMap::Get("W");
    m_Keys.a = KeyMap::Get("A");
    m_Keys.d = KeyMap::Get("D");
    m_Keys.up = KeyMap::Get("Up");
    m_Keys.down = KeyMap::Get("Down");
    m_Keys.left = KeyMap::Get("Left");
    m_Keys.right = KeyMap::Get("Right");
    m_Keys.shift = KeyMap::Get("Shift");
    m_Keys.ctrl = KeyMap::Get("Ctrl");
    m_Keys.tab = KeyMap::Get("Tab");
    m_Keys.escape = KeyMap::Get("Escape");
}

SceneEditor::~SceneEditor() {
    // The view override and the muted actors are state on other objects;
    // leave them as found. Also closes any open gesture into the file.
    SetActive(false);
}

bool SceneEditor::Down(int keycode) const { return keycode != 0 && m_Input.IsKeyDown(keycode); }
bool SceneEditor::Pressed(int keycode) const { return keycode != 0 && m_Input.IsKeyPressed(keycode); }

void SceneEditor::OnSceneLoaded() {
    // Every body pointer from before this load was freed with the old scene.
    m_Drag = Drag{};
    m_TrackBody = nullptr;
    m_WroteBody = nullptr;
    m_SnapBackFrames = 0;
    m_Driven.clear();
    m_MutedActors.clear();
    m_PendingPlacement.reset();

    // Keyed by name rather than pointer, so an open gesture survives intact.
    CommitGesture();

    if (!m_FileLoaded) {
        m_ScenePath = ScenePathFor(m_Scripts.GetScriptPath());
        m_Overrides.Load(m_ScenePath);
        m_FileLoaded = true;
    }

    // Recreates every placed asset from the spawn list. Unconditional, and
    // before the edits below: existence isn't the "apply saved edits" toggle's
    // business, and a spawned body has to exist before an edit can land on it.
    SpawnPersistedAssets();

    if (m_ApplyOnLoad && !m_Overrides.Empty()) {
        const int applied = m_Overrides.Apply(m_Actors);
        std::cout << "Scene edits: applied " << applied << " field(s) from " << m_ScenePath << "\n";
    }

    // The scripts just built fresh actors, every one listening to the keyboard.
    if (m_Active) MuteActors();
}

void SceneEditor::SetActive(bool active) {
    if (active == m_Active) return;

    if (!active) {
        if (m_Drag.handle != Handle::None) EndDrag();
        CommitGesture();
        m_Renderer.ClearViewOverride();
        UnmuteActors();
        m_Panning = false;
        m_Active = false;
        return;
    }

    m_Active = true;
    // Start exactly where the game camera is, so entering never jumps the view.
    m_ViewPos = m_Renderer.GetViewCenter();
    m_ZoomIndex = kDefaultZoom;
    MuteActors();
}

bool SceneEditor::TakeReloadRequest() {
    const bool requested = m_ReloadRequested;
    m_ReloadRequested = false;
    return requested;
}

std::string SceneEditor::Summary() const {
    std::string summary = m_Active ? "editing" : "";
    const size_t fields = m_Overrides.FieldCount();
    if (fields > 0) summary += (summary.empty() ? "" : ", ") + std::to_string(fields) + " edits";
    if (m_Overrides.IsDirty()) summary += " *";
    return summary.empty() ? "off" : summary;
}

// ============================================================================
// Asset Menu: placing and spawning persisted objects
// ============================================================================

const char* SceneEditor::AssetTypeName(AssetType type) {
    switch (type) {
        case AssetType::Light:     return "Light";
        case AssetType::Rectangle: return "Rectangle";
        case AssetType::Character: return "Character";
    }
    return "Light";
}

std::string SceneEditor::NextAssetName(AssetType type) const {
    // Fixed per type, distinct from anything the scripts name themselves --
    // disambiguation among several of the same type comes for free from
    // SceneOverrides::KeyFor/Resolve counting bodies that share this name.
    switch (type) {
        case AssetType::Light:     return "Placed Light";
        case AssetType::Rectangle: return "Debug Quad";
        case AssetType::Character: return "Placed NPC";
    }
    return "Placed Asset";
}

void SceneEditor::ArmPlacement(AssetType type) {
    m_PendingPlacement = type;
    Flash(std::string("Click in the world to place a ") + AssetTypeName(type), UITheme::Value);
}

void SceneEditor::CreateAssetBody(AssetType type, const std::string& name, const Vector2& position) {
    switch (type) {
        case AssetType::Light: {
            // A plain engine-level light: no sprite, so IsIconOnly() picks it
            // by its icon and the existing Light gizmo/fields cover the rest.
            RigidBody2D* body = m_Actors.CreateRigidBody(position.x, position.y, 24.0f, 24.0f);
            body->lightEmitter = m_Actors.CreateLightEmitter();
            body->name = name;
            break;
        }
        case AssetType::Rectangle:
        case AssetType::Character:
            // Both need a per-frame DrawBody()/Update() the way every other
            // visible or animated object in this engine gets one -- which only
            // Lua ever calls -- so both are spawned through
            // scripts/core/spawn_registry.lua rather than direct ActorRegistry
            // calls here.
            m_Scripts.CallSpawn(type == AssetType::Rectangle ? "SpawnRectangleAt" : "SpawnNPCAt",
                                position.x, position.y, name);
            break;
    }
}

void SceneEditor::SpawnPersistedAssets() {
    for (const SceneOverrides::Spawn& spawn : m_Overrides.Spawns()) {
        AssetType type;
        if (spawn.type == "Light") type = AssetType::Light;
        else if (spawn.type == "Rectangle") type = AssetType::Rectangle;
        else if (spawn.type == "Character") type = AssetType::Character;
        else {
            std::cerr << "Engine Warning: scene file has a placed asset of unknown type '"
                      << spawn.type << "' ('" << spawn.name << "') -- skipped\n";
            continue;
        }
        CreateAssetBody(type, spawn.name, spawn.position);
    }
}

void SceneEditor::PlaceAsset(AssetType type, const Vector2& world) {
    const std::string name = NextAssetName(type);

    // Computed BEFORE creation: Resolve() counts bodies by NAME, not vector
    // position, so this is exactly the key the new body answers to once it
    // exists, whatever else its construction creates alongside it (an NPC's
    // leg canvases, say, which never share its name).
    int ordinal = 0;
    for (const auto& owned : m_Actors.GetBodies()) {
        if (owned->name == name) ++ordinal;
    }
    const std::string key = name + "#" + std::to_string(ordinal);

    SceneOverrides::Spawn spawn;
    spawn.type = AssetTypeName(type);
    spawn.name = name;
    spawn.position = world;
    m_Overrides.AddSpawn(spawn);

    CreateAssetBody(type, name, world);
    SaveNow(false);

    if (RigidBody2D* body = SceneOverrides::Resolve(m_Actors, key)) m_Explorer.Select(body);
    Flash("Placed " + name, UITheme::Good);
}

void SceneEditor::MuteActors() {
    for (const auto& owned : m_Actors.GetBodies()) {
        PlayerActorConfig* config = owned->playerConfig;
        if (!config || !config->inputEnabled) continue;
        config->inputEnabled = false;
        m_MutedActors.push_back(config);
    }
}

void SceneEditor::UnmuteActors() {
    for (PlayerActorConfig* config : m_MutedActors) config->inputEnabled = true;
    m_MutedActors.clear();
}

void SceneEditor::Flash(const std::string& text, const Color& color) {
    m_Flash = text;
    m_FlashColor = color;
    // Long enough to read: a lock's reason is a sentence, not a word.
    m_FlashTime = text.size() > 40 ? 5.0f : 2.5f;
}

// ============================================================================
// Per frame
// ============================================================================

void SceneEditor::BeginFrame(float realDeltaTime, bool panelHasMouse, float uiScale) {
    m_UIScale = uiScale;
    m_PanelHasMouse = panelHasMouse;
    m_FlashTime = std::max(0.0f, m_FlashTime - realDeltaTime);
    m_DiscardArmed = std::max(0.0f, m_DiscardArmed - realDeltaTime);
    if (!m_Active) return;

    const bool ctrl = Down(m_Keys.ctrl);
    const bool shift = Down(m_Keys.shift);

    // Undo and redo wait for the drag to end too: rolling back the step that
    // gave a body its collider, say, would pull it out from under the handle.
    if (ctrl) {
        if (m_Drag.handle == Handle::None && Pressed(m_Keys.z)) Undo();
        if (m_Drag.handle == Handle::None && Pressed(m_Keys.y)) Redo();
        if (Pressed(m_Keys.s)) SaveNow(true);
    } else if (m_Drag.handle == Handle::None) {
        // Tool keys wait for the drag to end: swapping tools mid-drag would hand
        // the rest of the gesture to a handle that never started it.
        if (Pressed(m_Keys.q)) m_Tool = Tool::Move;
        if (Pressed(m_Keys.e)) m_Tool = Tool::Rotate;
        if (Pressed(m_Keys.r)) m_Tool = Tool::Scale;
        if (Pressed(m_Keys.c)) m_Tool = Tool::Collider;
        if (Pressed(m_Keys.tab)) {
            m_Tool = static_cast<Tool>((static_cast<int>(m_Tool) + 1) % static_cast<int>(Tool::Count));
        }
        if (Pressed(m_Keys.f)) FrameSelection();
    }
    if (Pressed(m_Keys.escape)) {
        if (m_PendingPlacement) {
            m_PendingPlacement.reset();
            Flash("Placement cancelled", UITheme::TextDim);
        } else if (m_Drag.handle != Handle::None) {
            CancelDrag();
        } else {
            m_Explorer.ClearSelection();
        }
    }

    UpdateFreeLook(realDeltaTime, panelHasMouse, ctrl, shift);

    // Through LAST frame's mapping, deliberately: that is the picture the user
    // was looking at when they clicked.
    const Vector2 mouse = m_Input.GetMousePosition();
    const Vector2 world = m_Renderer.ScreenToWorld(mouse);

    if (m_PendingPlacement && !panelHasMouse && m_Input.IsMouseButtonPressed(MouseButton::Left)) {
        PlaceAsset(*m_PendingPlacement, world);
        m_PendingPlacement.reset();
    } else if (m_Drag.handle == Handle::None && !panelHasMouse && m_Input.IsMouseButtonPressed(MouseButton::Left)) {
        PressInWorld(m_Explorer.Selected(m_Actors), mouse, world);
    }

    if (m_Drag.handle != Handle::None) {
        if (m_Input.IsMouseButtonDown(MouseButton::Left)) {
            BeginTracking(m_Drag.body);
            UpdateDrag(mouse, world, m_Snap != ctrl, shift);
            EndTracking();
        } else {
            EndDrag();
        }
    }

    // A gesture runs from one press to its release, whichever handle or
    // widget it went through, so it only closes once the button is up.
    if (m_Drag.handle == Handle::None && !m_Input.IsMouseButtonDown(MouseButton::Left)) CommitGesture();

    // Picked up by the scripts' SyncCamera() later this frame.
    m_Renderer.SetViewOverride(m_ViewPos, Zoom());
}

void SceneEditor::AfterScripts() {
    RigidBody2D* body = m_WroteBody;
    m_WroteBody = nullptr;
    if (!body || !m_Active || m_Simulate) return;

    // The scripts moved it back to exactly where the drag began, frame after
    // frame: something repositions it every Update() (a leg canvas pinned to
    // its owner). Physics can't be the cause while the sim is frozen, and a
    // collision push-out lands on a surface, not back on the start.
    const Vector2 start = m_Drag.startTransform.position;
    if (m_WrotePos != start && body->transform.position == start) {
        if (++m_SnapBackFrames >= 3 && m_Driven.insert(body).second) {
            Flash("This part is placed by script every frame", UITheme::Warn);
        }
    } else {
        m_SnapBackFrames = 0;
    }
}

// ============================================================================
// Free camera
// ============================================================================

float SceneEditor::Zoom() const { return kZooms[m_ZoomIndex]; }

bool SceneEditor::HasGameCamera() const { return m_Actors.GetActiveCamera() != nullptr; }

void SceneEditor::UpdateFreeLook(float deltaTime, bool panelHasMouse, bool ctrl, bool shift) {
    const Vector2 mouse = m_Input.GetMousePosition();
    const float pixelsPerUnit = std::max(m_Renderer.PixelsPerWorldUnit(), 0.0001f);

    // Right or middle drag pans: starts only off the panel, then keeps going
    // even if the cursor crosses it.
    const bool panHeld = m_Input.IsMouseButtonDown(MouseButton::Right) ||
                         m_Input.IsMouseButtonDown(MouseButton::Middle);
    const bool panPressed = m_Input.IsMouseButtonPressed(MouseButton::Right) ||
                            m_Input.IsMouseButtonPressed(MouseButton::Middle);
    if (!m_Panning && panPressed && !panelHasMouse) {
        m_Panning = true;
        m_PanLastMouse = mouse;
    }
    if (m_Panning) {
        if (!panHeld) {
            m_Panning = false;
        } else {
            m_ViewPos -= (mouse - m_PanLastMouse) / pixelsPerUnit;
            m_PanLastMouse = mouse;
        }
    }

    // Ctrl is reserved for Ctrl+S and friends, so it never pans.
    if (!ctrl) {
        Vector2 direction;
        if (Down(m_Keys.w) || Down(m_Keys.up))    direction.y -= 1.0f;
        if (Down(m_Keys.s) || Down(m_Keys.down))  direction.y += 1.0f;
        if (Down(m_Keys.a) || Down(m_Keys.left))  direction.x -= 1.0f;
        if (Down(m_Keys.d) || Down(m_Keys.right)) direction.x += 1.0f;
        if (direction.x != 0.0f || direction.y != 0.0f) {
            const float speed = kPanSpeed * (shift ? 3.0f : 1.0f) / pixelsPerUnit;
            m_ViewPos += direction.Normalized() * (speed * deltaTime);
        }
    }

    const float scroll = m_Input.GetScrollDelta();
    if (scroll != 0.0f && !panelHasMouse) {
        const int next = std::clamp(m_ZoomIndex + (scroll > 0.0f ? -1 : 1), 0, kZoomCount - 1);
        if (next != m_ZoomIndex) {
            // About the cursor: the world point under it stays under it. The
            // renderer still holds last frame's centre, hence the rebase onto
            // this frame's m_ViewPos.
            const Vector2 anchor = m_ViewPos + (m_Renderer.ScreenToWorld(mouse) - m_Renderer.GetViewCenter());
            m_ViewPos = anchor - (anchor - m_ViewPos) * (kZooms[next] / kZooms[m_ZoomIndex]);
            m_ZoomIndex = next;
        }
    }
}

void SceneEditor::FrameSelection() {
    if (RigidBody2D* selected = m_Explorer.Selected(m_Actors)) {
        m_ViewPos = selected->transform.position;
    } else {
        Flash("Nothing selected to frame", UITheme::TextDim);
    }
}

void SceneEditor::ResetView() {
    const RigidBody2D* camera = m_Actors.GetActiveCamera();
    m_ViewPos = camera ? camera->transform.position : m_Renderer.GetViewCenter();
    m_ZoomIndex = kDefaultZoom;
}

// ============================================================================
// Change tracking, undo, persistence
// ============================================================================

void SceneEditor::BeginTracking(RigidBody2D* body) {
    m_TrackBody = body;
    if (body) m_TrackBefore = SceneFields::Capture(*body);
}

void SceneEditor::EndTracking() {
    RigidBody2D* body = m_TrackBody;
    m_TrackBody = nullptr;
    if (!body) return;

    const SceneFields::Snapshot after = SceneFields::Capture(*body);
    const auto& before = m_TrackBefore.entries;
    std::string key; // only worked out if something actually changed
    for (size_t i = 0; i < after.entries.size(); ++i) {
        const SceneFields::Snapshot::Entry& now = after.entries[i];

        // Matched by id, not position: an onChange that re-exposes script
        // values can reorder a body's script fields between the two captures.
        const SceneFields::Snapshot::Entry* was = (i < before.size() && before[i].field == now.field) ? &before[i] : nullptr;
        for (size_t j = 0; !was && j < before.size(); ++j) {
            if (before[j].field == now.field) was = &before[j];
        }

        const bool wasPresent = was && was->present;
        if (!wasPresent && !now.present) continue;
        if (wasPresent && now.present && was->value == now.value) continue;

        if (key.empty()) key = SceneOverrides::KeyFor(m_Actors, *body);
        NoteChange(key, now.field, wasPresent, now.present, was ? was->value : SceneFields::Value{}, now.value);
    }
}

void SceneEditor::NoteChange(const std::string& key, int field, bool beforePresent, bool afterPresent,
                             const SceneFields::Value& before, const SceneFields::Value& after) {
    // The FIRST before of each field is kept, so a whole drag is one undo step.
    auto it = std::find_if(m_Gesture.changes.begin(), m_Gesture.changes.end(),
                           [&](const Change& c) { return c.field == field && c.key == key; });
    if (it == m_Gesture.changes.end()) {
        Change change;
        change.key = key;
        change.field = field;
        change.before = before;
        change.beforePresent = beforePresent;
        if (const SceneOverrides::Edit* edit = m_Overrides.Find(key, field)) change.editBefore = *edit;
        m_Gesture.changes.push_back(std::move(change));
        it = std::prev(m_Gesture.changes.end());
    }
    it->after = after;
    it->afterPresent = afterPresent;

    // A field that just stopped existing -- the half size of a box turned
    // circle -- has nothing to save. Its entry is left alone; the change is
    // kept only so undo can put the old value back.
    if (!afterPresent) return;

    SceneOverrides::Edit edit;
    edit.field = field;
    edit.value = after;
    if (it->editBefore && it->editBefore->hasBaseline) {
        edit.baseline = it->editBefore->baseline;
        edit.hasBaseline = true;
    } else if (it->beforePresent) {
        edit.baseline = it->before;
        edit.hasBaseline = true;
    }
    m_Overrides.Set(key, edit);
}

void SceneEditor::ApplyEditState(const std::string& key, int field,
                                 const std::optional<SceneOverrides::Edit>& edit) {
    if (edit) m_Overrides.Set(key, *edit);
    else m_Overrides.Forget(key, field);
}

void SceneEditor::CommitGesture() {
    if (m_Gesture.changes.empty()) return;

    Step step;
    for (Change& change : m_Gesture.changes) {
        // Dragged out and back to exactly where it began: no change at all, so
        // the entry goes back as it was rather than keeping an edit of nothing.
        if (change.beforePresent == change.afterPresent && change.before == change.after) {
            ApplyEditState(change.key, change.field, change.editBefore);
            continue;
        }
        if (const SceneOverrides::Edit* edit = m_Overrides.Find(change.key, change.field)) change.editAfter = *edit;
        step.changes.push_back(std::move(change));
    }
    m_Gesture = Step{};

    if (!step.changes.empty()) PushStep(std::move(step));
    else if (m_Overrides.IsDirty()) AfterEditsChanged();
}

void SceneEditor::PushStep(Step step) {
    if (step.label.empty()) {
        step.label = step.changes.size() == 1
            ? std::string(SceneFields::At(step.changes.front().field).key)
            : std::to_string(step.changes.size()) + " fields";
    }
    m_Undo.push_back(std::move(step));
    if (m_Undo.size() > kMaxUndo) m_Undo.erase(m_Undo.begin());
    m_Redo.clear();
    AfterEditsChanged();
}

// Both directions walk the changes FORWARD, in table order. A type switch
// sits ahead of the sizes that depend on it, so putting the type back first
// is what lets the old half size land on a shape that has one.
void SceneEditor::Undo() {
    CommitGesture();
    if (m_Undo.empty()) {
        Flash("Nothing to undo", UITheme::TextDim);
        return;
    }
    Step step = std::move(m_Undo.back());
    m_Undo.pop_back();

    for (const Change& change : step.changes) {
        RigidBody2D* body = SceneOverrides::Resolve(m_Actors, change.key);
        if (body && change.beforePresent) {
            SceneFields::Set(SceneFields::At(change.field), *body, change.before, m_Actors);
        }
        ApplyEditState(change.key, change.field, change.editBefore);
    }

    Flash("Undo " + step.label, UITheme::Value);
    m_Redo.push_back(std::move(step));
    AfterEditsChanged();
}

void SceneEditor::Redo() {
    CommitGesture();
    if (m_Redo.empty()) {
        Flash("Nothing to redo", UITheme::TextDim);
        return;
    }
    Step step = std::move(m_Redo.back());
    m_Redo.pop_back();

    for (const Change& change : step.changes) {
        RigidBody2D* body = SceneOverrides::Resolve(m_Actors, change.key);
        if (body && change.afterPresent) {
            SceneFields::Set(SceneFields::At(change.field), *body, change.after, m_Actors);
        }
        ApplyEditState(change.key, change.field, change.editAfter);
    }

    Flash("Redo " + step.label, UITheme::Value);
    m_Undo.push_back(std::move(step));
    AfterEditsChanged();
}

void SceneEditor::RevertField(RigidBody2D* body, const std::string& key, int field) {
    CommitGesture();
    const SceneOverrides::Edit* edit = m_Overrides.Find(key, field);
    if (!edit) return;

    const SceneFields::Field& info = SceneFields::At(field);
    Change change;
    change.key = key;
    change.field = field;
    change.editBefore = *edit; // editAfter stays empty: reverting means no edit

    if (body && SceneFields::IsPresent(info, *body)) {
        change.before = SceneFields::Get(info, *body);
        change.beforePresent = true;
        change.afterPresent = true;
        // Without a baseline there is nothing to go back to short of a reload;
        // the edit is dropped and the value stays as it is until then.
        change.after = edit->hasBaseline ? edit->baseline : change.before;
        if (edit->hasBaseline) SceneFields::Set(info, *body, edit->baseline, m_Actors);
    }
    m_Overrides.Forget(key, field);

    Step step;
    step.label = std::string("revert ") + info.key;
    step.changes.push_back(std::move(change));
    PushStep(std::move(step));
}

void SceneEditor::RevertObject(const std::string& key) {
    CommitGesture();
    const SceneOverrides::Entry* entry = m_Overrides.FindEntry(key);
    if (!entry) return;

    // Copied first: forgetting edits reshapes the list being walked.
    const std::vector<SceneOverrides::Edit> edits = entry->edits;
    RigidBody2D* body = SceneOverrides::Resolve(m_Actors, key);

    Step step;
    step.label = "revert " + key;
    for (const SceneOverrides::Edit& edit : edits) {
        const SceneFields::Field& info = SceneFields::At(edit.field);
        Change change;
        change.key = key;
        change.field = edit.field;
        change.editBefore = edit;
        if (body && SceneFields::IsPresent(info, *body)) {
            change.before = SceneFields::Get(info, *body);
            change.beforePresent = true;
            change.afterPresent = true;
            change.after = edit.hasBaseline ? edit.baseline : change.before;
            if (edit.hasBaseline) SceneFields::Set(info, *body, edit.baseline, m_Actors);
        }
        m_Overrides.Forget(key, edit.field);
        step.changes.push_back(std::move(change));
    }
    PushStep(std::move(step));
    Flash("Reverted " + key, UITheme::Value);
}

void SceneEditor::SaveNow(bool announce) {
    if (m_ScenePath.empty()) return;
    const bool saved = m_Overrides.Save(m_ScenePath, m_Scripts.GetScriptPath());
    if (!saved) Flash("Save failed -- see the Engine Log", UITheme::Bad);
    else if (announce) Flash("Saved " + m_ScenePath, UITheme::Good);
}

void SceneEditor::AfterEditsChanged() {
    if (m_AutoSave) SaveNow(false);
}

// ============================================================================
// Picking and gizmos
// ============================================================================

bool SceneEditor::IsIconOnly(const RigidBody2D& body) const {
    // A light fixture with nothing drawn (Spotlight): its size is RigidBody2D's
    // 50x50 default, which nobody can see, so it is picked by an icon instead.
    return !body.sprite && !body.terrain && body.lightEmitter;
}

bool SceneEditor::AimsCone(const RigidBody2D& body) const {
    return body.lightEmitter && body.lightEmitter->type == LightEmitterConfig::Type::Cone &&
           !body.lightEmitter->useOwnerRotation;
}

SceneEditor::Geometry SceneEditor::GeometryOf(const RigidBody2D& body) const {
    Geometry geometry;
    geometry.center = m_Renderer.WorldToScreen(body.transform.position);
    geometry.axisX = Vector2(1.0f, 0.0f).Rotated(body.transform.rotation);
    geometry.axisY = Vector2(0.0f, 1.0f).Rotated(body.transform.rotation);
    geometry.icon = IsIconOnly(body);

    if (geometry.icon) {
        geometry.half = {Px(kIconRadius), Px(kIconRadius)};
        return geometry;
    }
    const float pixelsPerUnit = m_Renderer.PixelsPerWorldUnit();
    geometry.half = {std::fabs(body.size.x * body.transform.scale.x) * 0.5f * pixelsPerUnit,
                     std::fabs(body.size.y * body.transform.scale.y) * 0.5f * pixelsPerUnit};
    return geometry;
}

namespace {

Vector2 HandlePoint(const Vector2& center, const Vector2& half, const Vector2& axisX, const Vector2& axisY, int mask) {
    const float sx = (mask & 1) ? -1.0f : (mask & 2) ? 1.0f : 0.0f;
    const float sy = (mask & 4) ? -1.0f : (mask & 8) ? 1.0f : 0.0f;
    return center + axisX * (sx * half.x) + axisY * (sy * half.y);
}

float RingRadius(const Vector2& half, float pad, float minimum) {
    return std::max(std::max(half.x, half.y) + pad, minimum);
}

} // namespace

bool SceneEditor::UnderCursor(const RigidBody2D& body, const Vector2& mouse) const {
    const Geometry geometry = GeometryOf(body);
    if (geometry.icon) return Vector2::Distance(mouse, geometry.center) <= geometry.half.x + Px(2.0f);

    const Vector2 offset = mouse - geometry.center;
    const float localX = offset.Dot(geometry.axisX);
    const float localY = offset.Dot(geometry.axisY);
    if (std::fabs(localX) > geometry.half.x || std::fabs(localY) > geometry.half.y) return false;

    const PixelSprite* sprite = body.sprite;
    if (!sprite || sprite->GetWidth() <= 0 || sprite->GetHeight() <= 0) return true;
    if (geometry.half.x <= 0.0f || geometry.half.y <= 0.0f) return false;

    // Box-local to texel, the way DrawTexturedQuad lays the sprite out: row 0
    // at the top, and a negative scale mirroring it.
    const int width = sprite->GetWidth();
    const int height = sprite->GetHeight();
    float u = localX / (2.0f * geometry.half.x) + 0.5f;
    float v = localY / (2.0f * geometry.half.y) + 0.5f;
    if (body.transform.scale.x < 0.0f) u = 1.0f - u;
    if (body.transform.scale.y < 0.0f) v = 1.0f - v;
    const int column = std::clamp(static_cast<int>(std::floor(u * static_cast<float>(width))), 0, width - 1);
    const int row = std::clamp(static_cast<int>(std::floor(v * static_cast<float>(height))), 0, height - 1);

    // A few pixels of slack, in texels: a one-texel leg is two or three screen
    // pixels wide at 1x and would otherwise take a sniper to click.
    const float texelPx = std::min(2.0f * geometry.half.x / static_cast<float>(width),
                                   2.0f * geometry.half.y / static_cast<float>(height));
    const int reach = texelPx >= Px(kHitSlop) ? 0 : static_cast<int>(std::ceil(Px(3.0f) / std::max(texelPx, 0.01f)));
    for (int dy = -reach; dy <= reach; ++dy) {
        for (int dx = -reach; dx <= reach; ++dx) {
            if (sprite->IsSolid(column + dx, row + dy)) return true;
        }
    }
    return false;
}

std::vector<RigidBody2D*> SceneEditor::BodiesUnder(const Vector2& mouse) const {
    struct Hit {
        RigidBody2D* body;
        bool driven;
        float area;
        size_t order;
    };
    std::vector<Hit> hits;

    const auto& bodies = m_Actors.GetBodies();
    for (size_t i = 0; i < bodies.size(); ++i) {
        RigidBody2D& body = *bodies[i];
        // Invisible and parked mid-screen, so it would win every click there.
        // Still selectable from the explorer tree.
        if (body.camera) continue;
        // Deleted: inert until the next reload actually drops it.
        if (body.destroyed) continue;
        if (!UnderCursor(body, mouse)) continue;

        // A part stands in for its owner: clicking a character's legs picks
        // the character, which is the thing that can actually be moved.
        RigidBody2D* target = body.partOf ? body.partOf : &body;
        const float area = IsIconOnly(body)
            ? 0.0f
            : std::fabs(body.size.x * body.transform.scale.x * body.size.y * body.transform.scale.y);

        auto existing = std::find_if(hits.begin(), hits.end(), [&](const Hit& h) { return h.body == target; });
        if (existing != hits.end()) {
            existing->area = std::min(existing->area, area);
            continue;
        }
        hits.push_back({target, m_Driven.count(target) != 0, area, i});
    }

    // Smallest first, so what sits ON something is picked before what it sits
    // on -- a prop before the terrain under it. Ties go to the later body,
    // since scripts mostly draw in creation order. Script-driven parts sink to
    // the bottom: a drag on one is undone every frame.
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.driven != b.driven) return !a.driven;
        if (a.area != b.area) return a.area < b.area;
        return a.order > b.order;
    });

    std::vector<RigidBody2D*> result;
    result.reserve(hits.size());
    for (const Hit& hit : hits) result.push_back(hit.body);
    return result;
}

SceneEditor::Handle SceneEditor::HitHandle(const RigidBody2D& body, const Vector2& mouse, int& edges) const {
    edges = 0;
    const Geometry g = GeometryOf(body);
    const float slop = Px(kHitSlop);
    const float pixelsPerUnit = m_Renderer.PixelsPerWorldUnit();

    switch (m_Tool) {
        case Tool::Move:
            if (std::fabs(mouse.x - g.center.x) <= slop && std::fabs(mouse.y - g.center.y) <= slop) return Handle::Move;
            if (DistanceToSegment(mouse, g.center, g.center + Vector2(Px(kArrow), 0.0f)) <= slop) return Handle::AxisX;
            if (DistanceToSegment(mouse, g.center, g.center + Vector2(0.0f, Px(kArrow))) <= slop) return Handle::AxisY;
            break;

        case Tool::Rotate: {
            const float ring = RingRadius(g.half, Px(kRingPad), Px(28.0f));
            if (std::fabs(Vector2::Distance(mouse, g.center) - ring) <= slop) return Handle::Rotate;
            break;
        }

        case Tool::Scale:
            if (g.icon) {
                const Vector2 knob = g.center + Vector2(body.lightEmitter->radius * pixelsPerUnit, 0.0f);
                if (Vector2::Distance(mouse, knob) <= slop) return Handle::LightRadius;
                break;
            }
            for (int mask : kHandleMasks) {
                if (Vector2::Distance(mouse, HandlePoint(g.center, g.half, g.axisX, g.axisY, mask)) <= slop) {
                    edges = mask;
                    return Handle::Scale;
                }
            }
            break;

        case Tool::Collider: {
            if (!body.collisionShape) break;
            const CollisionShape2D& shape = *body.collisionShape;
            const Vector2 center = m_Renderer.WorldToScreen(body.transform.position + shape.GetOffset());
            if (shape.GetType() == CollisionShape2D::Type::Box) {
                const Vector2 half = shape.GetHalfExtents() * pixelsPerUnit;
                for (int mask : kHandleMasks) {
                    const Vector2 point = HandlePoint(center, half, {1.0f, 0.0f}, {0.0f, 1.0f}, mask);
                    if (Vector2::Distance(mouse, point) <= slop) {
                        edges = mask;
                        return Handle::ColliderEdge;
                    }
                }
                if (std::fabs(mouse.x - center.x) <= half.x && std::fabs(mouse.y - center.y) <= half.y) {
                    return Handle::ColliderMove;
                }
            } else {
                const float radius = shape.GetRadius() * pixelsPerUnit;
                if (Vector2::Distance(mouse, center + Vector2(radius, 0.0f)) <= slop) return Handle::ColliderRadius;
                if (Vector2::Distance(mouse, center) <= radius) return Handle::ColliderMove;
            }
            break;
        }

        default:
            break;
    }

    // The body itself moves it, whatever the tool, so repositioning something
    // never needs a tool switch first.
    return UnderCursor(body, mouse) ? Handle::Move : Handle::None;
}

void SceneEditor::PressInWorld(RigidBody2D* selected, const Vector2& mouse, const Vector2& world) {
    if (selected) {
        int edges = 0;
        const Handle handle = HitHandle(*selected, mouse, edges);
        if (handle != Handle::None) {
            // A locked handle explains itself instead of dragging nothing.
            if (const std::string lock = HandleLock(*selected, handle); !lock.empty()) {
                Flash(lock, UITheme::Warn);
                return;
            }
            StartDrag(*selected, handle, edges, mouse, world);
            // Pressing the selection itself and letting go without moving steps
            // to whatever is underneath: how a part hidden behind another is
            // reached without the tree.
            m_Drag.clickCycles = handle == Handle::Move && UnderCursor(*selected, mouse);
            return;
        }
    }

    const std::vector<RigidBody2D*> under = BodiesUnder(mouse);
    RigidBody2D* hit = under.empty() ? nullptr : under.front();
    m_Explorer.Select(hit);

    // Press-and-drag on something new grabs it in the same motion. Not with
    // the collider tool, where what gets dragged is the collider, not the body,
    // and not when its position is locked -- selecting it is still useful.
    if (hit && m_Tool != Tool::Collider && HandleLock(*hit, Handle::Move).empty()) {
        StartDrag(*hit, Handle::Move, 0, mouse, world);
    }
}

void SceneEditor::StartDrag(RigidBody2D& body, Handle handle, int edges, const Vector2& mouse, const Vector2& world) {
    m_Drag = Drag{};
    m_Drag.handle = handle;
    m_Drag.edges = edges;
    m_Drag.body = &body;
    m_Drag.startMouseScreen = mouse;
    m_Drag.startMouseWorld = world;
    m_Drag.startTransform = body.transform;
    if (body.collisionShape) {
        m_Drag.startColliderOffset = body.collisionShape->GetOffset();
        m_Drag.startHalfExtents = body.collisionShape->GetHalfExtents();
        m_Drag.startRadius = body.collisionShape->GetRadius();
    }
    if (body.lightEmitter) {
        m_Drag.startLightRadius = body.lightEmitter->radius;
        m_Drag.startConeAim = body.lightEmitter->coneDirectionRad;
    }
    m_SnapBackFrames = 0;
}

void SceneEditor::UpdateDrag(const Vector2& mouse, const Vector2& world, bool snap, bool uniform) {
    Drag& drag = m_Drag;
    RigidBody2D& body = *drag.body;

    if (!drag.moved) {
        if (Vector2::Distance(mouse, drag.startMouseScreen) < Px(kDeadZone)) return;
        drag.moved = true;
    }

    const Vector2 delta = world - drag.startMouseWorld;
    const Vector2 origin = drag.startTransform.position;
    const float grid = snap ? m_MoveSnap : 0.0f;

    // Nothing should remove these mid-drag -- undo waits for the release and
    // a reload resets the drag -- but a stale handle must never dereference null.
    const bool needsCollider = drag.handle == Handle::ColliderMove || drag.handle == Handle::ColliderEdge ||
                               drag.handle == Handle::ColliderRadius;
    if ((needsCollider && !body.collisionShape) || (drag.handle == Handle::LightRadius && !body.lightEmitter)) {
        m_Drag = Drag{};
        return;
    }

    switch (drag.handle) {
        case Handle::Move:
        case Handle::AxisX:
        case Handle::AxisY: {
            Vector2 position = origin;
            if (drag.handle != Handle::AxisY) position.x = SnapTo(origin.x + delta.x, grid);
            if (drag.handle != Handle::AxisX) position.y = SnapTo(origin.y + delta.y, grid);
            body.transform.position = position;
            // Held still rather than thrown: velocity from before the grab would
            // fling it the moment it is let go under simulation.
            body.velocity = Vector2::Zero();
            m_WroteBody = &body;
            m_WrotePos = position;
            break;
        }

        case Handle::Rotate: {
            const float from = std::atan2(drag.startMouseWorld.y - origin.y, drag.startMouseWorld.x - origin.x);
            const float to = std::atan2(world.y - origin.y, world.x - origin.x);
            const float step = snap ? m_RotateSnapDeg * kDegToRad : 0.0f;
            // A wall-mounted cone light has no visible body to turn; its beam is
            // what the ring aims.
            if (AimsCone(body)) {
                body.lightEmitter->coneDirectionRad = WrapAngle(SnapTo(drag.startConeAim + (to - from), step));
            } else {
                body.transform.rotation = WrapAngle(SnapTo(drag.startTransform.rotation + (to - from), step));
            }
            break;
        }

        case Handle::Scale: {
            // The side opposite the handle stays put and the dragged side
            // follows the cursor, as in any drawing tool -- so growing a wall
            // upward doesn't also sink it into the floor. All of it in the
            // body's own frame, so a rotated body scales along its own axes.
            const float rotation = drag.startTransform.rotation;
            const Vector2 travel = delta.Rotated(-rotation);
            const Vector2 half{std::fabs(body.size.x * drag.startTransform.scale.x) * 0.5f,
                               std::fabs(body.size.y * drag.startTransform.scale.y) * 0.5f};
            const float sideX = (drag.edges & kLeft) ? -1.0f : (drag.edges & kRight) ? 1.0f : 0.0f;
            const float sideY = (drag.edges & kTop) ? -1.0f : (drag.edges & kBottom) ? 1.0f : 0.0f;

            Vector2 ratio{1.0f, 1.0f};
            if (sideX != 0.0f && half.x > 0.001f) ratio.x = (2.0f * half.x + sideX * travel.x) / (2.0f * half.x);
            if (sideY != 0.0f && half.y > 0.001f) ratio.y = (2.0f * half.y + sideY * travel.y) / (2.0f * half.y);
            if (uniform) {
                // A corner moves along its diagonal; an edge sets both axes.
                float factor = sideX != 0.0f ? ratio.x : ratio.y;
                if (sideX != 0.0f && sideY != 0.0f) {
                    const Vector2 diagonal{2.0f * sideX * half.x, 2.0f * sideY * half.y};
                    factor = 1.0f + SafeRatio(travel.Dot(diagonal), diagonal.LengthSquared());
                }
                ratio = {factor, factor};
            }
            // Dragging past the far side is a flip, not a shrink. Hold at a sliver.
            ratio.x = std::max(ratio.x, 0.01f);
            ratio.y = std::max(ratio.y, 0.01f);

            Vector2 scale{drag.startTransform.scale.x * ratio.x, drag.startTransform.scale.y * ratio.y};
            if (snap && m_ScaleSnap > 0.0f) {
                scale.x = SnapMagnitude(scale.x, m_ScaleSnap);
                scale.y = SnapMagnitude(scale.y, m_ScaleSnap);
            }
            body.transform.scale = scale;

            // Re-centred from the scale that actually landed, after snapping,
            // so the anchored side is exactly where it started. An axis with
            // no side (the other axis of an edge drag) grows about the centre.
            const Vector2 grown{half.x * std::fabs(SafeRatio(scale.x, drag.startTransform.scale.x)),
                                half.y * std::fabs(SafeRatio(scale.y, drag.startTransform.scale.y))};
            const Vector2 shift{sideX * (grown.x - half.x), sideY * (grown.y - half.y)};
            body.transform.position = origin + shift.Rotated(rotation);

            if (m_ColliderFollowsScale && body.collisionShape) {
                const Vector2 k{std::fabs(SafeRatio(scale.x, drag.startTransform.scale.x)),
                                std::fabs(SafeRatio(scale.y, drag.startTransform.scale.y))};
                CollisionShape2D& shape = *body.collisionShape;
                shape.SetOffset({drag.startColliderOffset.x * k.x, drag.startColliderOffset.y * k.y});
                if (shape.GetType() == CollisionShape2D::Type::Box) {
                    shape.SetHalfExtents({drag.startHalfExtents.x * k.x, drag.startHalfExtents.y * k.y});
                } else {
                    shape.SetRadius(drag.startRadius * std::max(k.x, k.y));
                }
            }
            break;
        }

        case Handle::LightRadius: {
            const float grabbed = Vector2::Distance(drag.startMouseWorld, origin);
            const float radius = drag.startLightRadius + Vector2::Distance(world, origin) - grabbed;
            body.lightEmitter->radius = std::max(SnapTo(radius, grid), 1.0f);
            break;
        }

        case Handle::ColliderMove:
            body.collisionShape->SetOffset({SnapTo(drag.startColliderOffset.x + delta.x, grid),
                                            SnapTo(drag.startColliderOffset.y + delta.y, grid)});
            break;

        case Handle::ColliderEdge: {
            // The dragged sides follow the cursor and the opposite ones stay put,
            // which is how a box is resized in every drawing tool.
            const Vector2 center = origin + drag.startColliderOffset;
            Vector2 lo = center - drag.startHalfExtents;
            Vector2 hi = center + drag.startHalfExtents;
            const Vector2 startLo = lo, startHi = hi;
            const float minSpan = std::max(grid, 1.0f);

            if (drag.edges & kLeft)   lo.x = std::min(SnapTo(startLo.x + delta.x, grid), hi.x - minSpan);
            if (drag.edges & kRight)  hi.x = std::max(SnapTo(startHi.x + delta.x, grid), lo.x + minSpan);
            if (drag.edges & kTop)    lo.y = std::min(SnapTo(startLo.y + delta.y, grid), hi.y - minSpan);
            if (drag.edges & kBottom) hi.y = std::max(SnapTo(startHi.y + delta.y, grid), lo.y + minSpan);

            body.collisionShape->SetHalfExtents((hi - lo) * 0.5f);
            body.collisionShape->SetOffset((lo + hi) * 0.5f - origin);
            break;
        }

        case Handle::ColliderRadius: {
            const Vector2 center = origin + drag.startColliderOffset;
            const float grabbed = Vector2::Distance(drag.startMouseWorld, center);
            const float radius = drag.startRadius + Vector2::Distance(world, center) - grabbed;
            body.collisionShape->SetRadius(std::max(SnapTo(radius, grid), 0.5f));
            break;
        }

        default:
            break;
    }
}

void SceneEditor::EndDrag() {
    const bool cycle = m_Drag.clickCycles && !m_Drag.moved;
    RigidBody2D* body = m_Drag.body;
    const Vector2 at = m_Drag.startMouseScreen;
    m_Drag = Drag{};
    if (!cycle) return;

    const std::vector<RigidBody2D*> under = BodiesUnder(at);
    if (under.size() < 2) return;
    auto it = std::find(under.begin(), under.end(), body);
    const bool wrap = it == under.end() || std::next(it) == under.end();
    m_Explorer.Select(wrap ? under.front() : *std::next(it));
}

void SceneEditor::CancelDrag() {
    // Everything the drag changed is in the open gesture: roll the bodies and
    // the saved edits both back to how they were at the press.
    m_Drag = Drag{};
    for (const Change& change : m_Gesture.changes) {
        RigidBody2D* body = SceneOverrides::Resolve(m_Actors, change.key);
        if (body && change.beforePresent) {
            SceneFields::Set(SceneFields::At(change.field), *body, change.before, m_Actors);
        }
        ApplyEditState(change.key, change.field, change.editBefore);
    }
    m_Gesture = Step{};
    // The file already matches, but rolling the entries back marked the store
    // dirty; a save puts the unsaved flag right again.
    AfterEditsChanged();
    Flash("Drag cancelled", UITheme::TextDim);
}

void SceneEditor::DropToGround(RigidBody2D& body) {
    // How far below its centre the body reaches: its collider when it has one,
    // since that is what will actually rest on the ground, else what is drawn.
    float below = std::fabs(body.size.y * body.transform.scale.y) * 0.5f;
    float above = below;
    if (body.collisionShape) {
        const CollisionShape2D& shape = *body.collisionShape;
        const float extent = shape.GetType() == CollisionShape2D::Type::Box ? shape.GetHalfExtents().y : shape.GetRadius();
        below = shape.GetOffset().y + extent;
        above = extent - shape.GetOffset().y;
    }

    const float x = body.transform.position.x;
    const float top = body.transform.position.y - above;
    // The first ground under the body; failing that, the highest ground in
    // the column, for something that was dragged below the floor.
    Physics::GroundHit hit = Physics::RaycastDown(m_Actors, x, top, top + 4096.0f, &body);
    if (!hit.hit) hit = Physics::RaycastDown(m_Actors, x, top - 4096.0f, top + 4096.0f, &body);
    if (!hit.hit) {
        Flash("No ground under this object", UITheme::Warn);
        return;
    }
    body.transform.position.y = hit.y - below;
    body.velocity = Vector2::Zero();
}

// ============================================================================
// Panel
// ============================================================================

void SceneEditor::DrawControls(UIPanel& panel) {
    const float width = panel.GetWidth();

    if (!m_Active) {
        if (panel.Button("Enter edit mode (F2)")) SetActive(true);
        panel.Label("Free camera, drag to place, all saved", UITheme::TextFaint);
    } else {
        if (panel.Button("Exit edit mode (F2)")) SetActive(false);

        for (int i = 0; i < static_cast<int>(Tool::Count); ++i) {
            bool on = static_cast<int>(m_Tool) == i;
            if (i % 2 == 1) panel.SameLine();
            if (panel.Toggle(kToolButtons[i], &on, width * 0.44f)) m_Tool = static_cast<Tool>(i);
        }

        panel.Toggle("Snap (Ctrl inverts)", &m_Snap);
        if (m_Snap) {
            panel.DragFloat("  grid", &m_MoveSnap, 0.02f, 2, 0.0f, 0.25f, 64.0f);
            panel.DragFloat("  angle deg", &m_RotateSnapDeg, 0.25f, 0, 0.0f, 1.0f, 90.0f);
            panel.DragFloat("  scale step", &m_ScaleSnap, 0.005f, 2, 0.0f, 0.05f, 2.0f);
        }
        panel.Toggle("Collider follows scale", &m_ColliderFollowsScale);
        panel.Toggle("Simulate while editing", &m_Simulate);

        // Scene-wide constants. They sit on the world proxy, so tracking,
        // undo and saving treat them exactly like a body's fields.
        RigidBody2D& world = SceneFields::WorldProxy();
        BeginTracking(&world);
        DrawFieldGroups(panel, world, SceneOverrides::KeyFor(m_Actors, world));
        EndTracking();

        panel.Separator();
        if (HasGameCamera()) {
            panel.KeyValue("view", Pair(m_ViewPos, 0) + "  " + ZoomText(Zoom()));
        } else {
            panel.KeyValue("view", "no active camera", UITheme::Warn);
        }
        if (panel.Button("Frame (F)", width * 0.3f)) FrameSelection();
        panel.SameLine();
        if (panel.Button("Reset view", width * 0.36f)) ResetView();
        panel.Separator();
    }

    // The file. Shown in and out of edit mode: the edits apply either way.
    const size_t fields = m_Overrides.FieldCount();
    const size_t objects = m_Overrides.Entries().size();
    const bool dirty = m_Overrides.IsDirty();
    const std::string count = fields == 0
        ? std::string("none")
        : std::to_string(fields) + " on " + std::to_string(objects) + (objects == 1 ? " object" : " objects");
    panel.KeyValue("file", m_ScenePath);
    panel.KeyValue("saved edits", count + (dirty ? "  unsaved" : ""), dirty ? UITheme::Warn : UITheme::Value);

    if (panel.Button("Save (Ctrl+S)", width * 0.44f)) SaveNow(true);
    panel.SameLine();
    if (panel.Button("Reload file", width * 0.4f)) {
        // For a .scene edited by hand: read it again and rebuild the scene on
        // top of it. History goes, since it describes edits that may be gone.
        m_Gesture = Step{};
        m_Undo.clear();
        m_Redo.clear();
        m_Overrides.Load(m_ScenePath);
        m_ReloadRequested = true;
        Flash("Reloaded " + m_ScenePath, UITheme::Value);
    }

    if (panel.Button("Undo (" + std::to_string(m_Undo.size()) + ")", width * 0.44f)) Undo();
    panel.SameLine();
    if (panel.Button("Redo (" + std::to_string(m_Redo.size()) + ")", width * 0.4f)) Redo();

    if (panel.Toggle("Auto-save", &m_AutoSave) && m_AutoSave && dirty) SaveNow(false);
    bool apply = m_ApplyOnLoad;
    if (panel.Toggle("Apply saved edits on load", &apply)) {
        // Rebuilt straight away, so the toggle shows the scene with and without.
        m_ApplyOnLoad = apply;
        m_ReloadRequested = true;
    }

    if (m_Overrides.Empty()) return;

    // Two presses: every edit, on disk as well, is not a thing to lose by a slip.
    const bool armed = m_DiscardArmed > 0.0f;
    if (panel.Button(armed ? "Click again to discard ALL" : "Discard all edits")) {
        if (!armed) {
            m_DiscardArmed = 3.0f;
        } else {
            m_DiscardArmed = 0.0f;
            m_Gesture = Step{};
            m_Undo.clear();
            m_Redo.clear();
            m_Overrides.Clear();
            SaveNow(false);
            m_ReloadRequested = true;
            Flash("Discarded every edit", UITheme::Warn);
        }
    }

    panel.Separator();
    const RigidBody2D* selected = m_Explorer.Selected(m_Actors);
    const std::string selectedKey = selected ? SceneOverrides::KeyFor(m_Actors, *selected) : std::string();
    std::vector<std::string> missing;
    for (const SceneOverrides::Entry& entry : m_Overrides.Entries()) {
        RigidBody2D* body = SceneOverrides::Resolve(m_Actors, entry.key);
        UIPanel::RowSpec row;
        row.label = entry.key;
        row.value = body ? std::to_string(entry.edits.size()) + (entry.edits.size() == 1 ? " field" : " fields")
                         : "not in scene";
        row.accent = body ? UITheme::Warn : UITheme::TextFaint;
        row.labelColor = body ? UITheme::Text : UITheme::TextFaint;
        row.valueColor = body ? UITheme::TextDim : UITheme::Bad;
        row.selected = entry.key == selectedKey;
        // The world has no row in the tree to select; its fields are above.
        if (panel.Row(row) == UIPanel::RowHit::Body && body && body != &SceneFields::WorldProxy()) {
            m_Explorer.Select(body);
        }
        if (!body) missing.push_back(entry.key);
    }

    if (!missing.empty() && panel.Button("Forget objects not in scene")) {
        for (const std::string& key : missing) m_Overrides.ForgetObject(key);
        AfterEditsChanged();
    }
}

std::string SceneEditor::FieldLock(const SceneFields::Field& field, const RigidBody2D& body) const {
    std::string lock = SceneFields::LockReason(field, body);
    if (lock.empty() && field.group == SceneFields::Group::Transform && m_Driven.count(&body)) {
        lock = "The scripts put this back every frame, so a change here would not last. "
               "Move whatever places it instead.";
    }
    return lock;
}

std::string SceneEditor::HandleLock(const RigidBody2D& body, Handle handle) const {
    // Looked up by key on each press rather than cached as raw indices, so a
    // reordered table can't quietly lock the wrong handle.
    switch (handle) {
        case Handle::Move:
        case Handle::AxisX:
        case Handle::AxisY:
            return FieldLock(SceneFields::At(SceneFields::IndexOf("position")), body);
        case Handle::Rotate:
            // A wall-mounted cone light turns its beam, not its body.
            return AimsCone(body) ? std::string() : FieldLock(SceneFields::At(SceneFields::IndexOf("rotation")), body);
        case Handle::Scale:
            return FieldLock(SceneFields::At(SceneFields::IndexOf("scale")), body);
        default:
            return {};
    }
}

void SceneEditor::DrawField(UIPanel& panel, int id, RigidBody2D& body, const std::string& key) {
    using SceneFields::Type;
    const SceneFields::Field& field = SceneFields::At(id);

    // A script constant is shown under its prefix ("gait"), so the row only
    // needs the rest of the name.
    std::string label = field.label;
    if (field.script) {
        const size_t dot = label.find('.');
        if (dot != std::string::npos && dot + 1 < label.size()) label = label.substr(dot + 1);
    }

    if (field.type == Type::Info) {
        panel.LockedField(label, field.describe ? field.describe(body) : std::string(),
                          SceneFields::LockReason(field, body));
        return;
    }

    SceneFields::Value value = SceneFields::Get(field, body);

    // Editable in general, just not on this body: shown, with its value and
    // the reason, rather than hidden -- nothing about the object goes missing.
    const std::string lock = FieldLock(field, body);
    if (!lock.empty()) {
        panel.LockedField(label, SceneFields::Describe(field, value), lock);
        return;
    }

    const bool edited = m_Overrides.Find(key, id) != nullptr;
    const Color labelColor = edited ? UITheme::Warn : UITheme::TextDim;
    const float half = panel.GetWidth() * 0.46f;
    bool changed = false;

    switch (field.type) {
        case Type::Float: {
            float f = value.F();
            changed = panel.DragFloat(label, &f, field.speed, field.decimals, 0.0f,
                                      field.minValue, field.maxValue, labelColor);
            if (changed) value = SceneFields::Value::Of(f);
            break;
        }
        case Type::Angle: {
            float degrees = value.F() * kRadToDeg;
            changed = panel.DragFloat(label + " deg", &degrees, field.speed, field.decimals,
                                      0.0f, field.minValue, field.maxValue, labelColor);
            if (changed) value = SceneFields::Value::Of(degrees * kDegToRad);
            break;
        }
        case Type::Int: {
            float f = static_cast<float>(value.I());
            changed = panel.DragFloat(label, &f, field.speed, 0, 0.0f, field.minValue, field.maxValue, labelColor);
            if (changed) value = SceneFields::Value::Of(static_cast<int>(std::lround(f)));
            break;
        }
        case Type::Bool: {
            bool b = value.B();
            changed = panel.Toggle(label, &b, 0.0f, edited ? UITheme::Warn : UITheme::Accent);
            if (changed) value = SceneFields::Value::Of(b);
            break;
        }
        case Type::Vec2: {
            float x = value.v[0];
            float y = value.v[1];
            const bool changedX = panel.DragFloat(label + " x", &x, field.speed, field.decimals,
                                                  half, field.minValue, field.maxValue, labelColor);
            panel.SameLine();
            const bool changedY = panel.DragFloat("y", &y, field.speed, field.decimals, 0.0f,
                                                  field.minValue, field.maxValue, labelColor);
            changed = changedX || changedY;
            if (changed) value = SceneFields::Value::Of(Vector2(x, y));
            break;
        }
        case Type::Color: {
            // The readout itself is painted in the color, which is the swatch.
            panel.KeyValue(label, SceneFields::Describe(field, value), Opaque(value.C()));
            static const char* const kChannels[] = {"r", "g", "b", "a"};
            const float quarter = panel.GetWidth() * 0.215f;
            for (int channel = 0; channel < 4; ++channel) {
                if (channel > 0) panel.SameLine();
                if (panel.DragFloat(kChannels[channel], &value.v[channel], field.speed, field.decimals,
                                    channel == 3 ? 0.0f : quarter, field.minValue, field.maxValue, labelColor)) {
                    changed = true;
                }
            }
            break;
        }
        case Type::Enum: {
            // One button that steps through the options: there are never many.
            const int current = value.I();
            const bool valid = current >= 0 && current < field.enumCount;
            const std::string name = valid ? field.enumNames[current] : "?";
            if (panel.Button(label + ": " + name + "  >")) {
                value = SceneFields::Value::Of(field.enumCount > 0 ? (current + 1) % field.enumCount : 0);
                changed = true;
            }
            break;
        }
        case Type::Info:
            break;
    }

    if (changed) SceneFields::Set(field, body, value, m_Actors);
}

void SceneEditor::DrawFieldGroups(UIPanel& panel, RigidBody2D& body, const std::string& key) {
    const std::vector<int> ids = SceneFields::FieldsOf(body);
    const float width = panel.GetWidth();

    for (int g = 0; g < static_cast<int>(SceneFields::Group::Count); ++g) {
        const auto group = static_cast<SceneFields::Group>(g);

        bool any = false;
        for (size_t i = 0; i < ids.size() && !any; ++i) {
            const SceneFields::Field& field = SceneFields::At(ids[i]);
            any = field.group == group && SceneFields::IsPresent(field, body);
        }
        if (!any) continue;

        panel.Spacing(2.0f);
        panel.Label(SceneFields::GroupName(group), GroupColor(group));

        // Sub-headings: a table entry names its own and later ones inherit it;
        // a script constant takes the prefix of its name ("gait.stride").
        std::string section;
        for (int id : ids) {
            const SceneFields::Field& field = SceneFields::At(id);
            // Re-checked per field: a widget above can switch a collider on or
            // change its shape, and the rest of the group follows this frame.
            if (field.group != group || !SceneFields::IsPresent(field, body)) continue;

            std::string next = section;
            if (field.script) {
                const std::string name = field.label;
                const size_t dot = name.find('.');
                next = dot == std::string::npos ? std::string() : name.substr(0, dot);
            } else if (field.section) {
                next = field.section;
            }
            if (next != section) {
                section = next;
                if (!section.empty()) panel.Label("  " + section, UITheme::TextFaint);
            }
            DrawField(panel, id, body, key);
        }

        if (group == SceneFields::Group::Transform) {
            if (FieldLock(SceneFields::At(SceneFields::IndexOf("position")), body).empty()) {
                if (panel.Button("Drop to ground", width * 0.36f)) DropToGround(body);
                panel.SameLine();
                if (panel.Button("Rot 0", width * 0.2f)) body.transform.rotation = 0.0f;
                panel.SameLine();
                if (panel.Button("Scale 1", width * 0.24f)) body.transform.scale = Vector2::One();
            }
        } else if (group == SceneFields::Group::Collider && body.collisionShape) {
            if (panel.Button("Fit collider to body")) {
                // Around what is drawn, centred, keeping the shape it has.
                const Vector2 half{std::fabs(body.size.x * body.transform.scale.x) * 0.5f,
                                   std::fabs(body.size.y * body.transform.scale.y) * 0.5f};
                body.collisionShape->SetOffset(Vector2::Zero());
                if (body.collisionShape->GetType() == CollisionShape2D::Type::Box) {
                    body.collisionShape->SetHalfExtents(half);
                } else {
                    body.collisionShape->SetRadius(std::max(half.x, half.y));
                }
            }
            if (body.transform.rotation != 0.0f) {
                panel.Label("Colliders ignore rotation.", UITheme::TextFaint);
            }
        } else if (group == SceneFields::Group::Terrain) {
            panel.Label("Surface, Dirt and Grass regenerate it.", UITheme::TextFaint);
        }
    }
}

void SceneEditor::DrawProperties(UIPanel& panel) {
    RigidBody2D* body = m_Explorer.Selected(m_Actors);
    if (!body) {
        panel.Label("Click an object in the scene or tree.", UITheme::TextFaint);
        return;
    }

    size_t index = 0;
    const auto& bodies = m_Actors.GetBodies();
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].get() == body) { index = i; break; }
    }
    const SceneExplorer::Kind kind = SceneExplorer::Classify(*body, m_Actors.GetPlayerActor());
    const std::string key = SceneOverrides::KeyFor(m_Actors, *body);
    const float width = panel.GetWidth();

    UIPanel::RowSpec title;
    title.label = SceneExplorer::DisplayName(*body, index, kind);
    title.value = SceneExplorer::KindName(kind);
    title.accent = SceneExplorer::KindColor(kind);
    title.valueColor = SceneExplorer::KindColor(kind);
    panel.Row(title);
    panel.KeyValue("saved as", "[" + key + "]", UITheme::TextDim);

    // Only an object the Asset Menu placed -- never a scripted one, which the
    // scripts would just build right back on the next load anyway.
    if (m_Overrides.HasSpawn(key)) {
        if (panel.Button("Delete asset", width * 0.5f)) {
            body->destroyed = true;
            m_Overrides.RemoveSpawn(key);
            AfterEditsChanged();
            Flash("Deleted " + key, UITheme::Warn);
            m_Explorer.ClearSelection();
            return;
        }
    }

    // Parts are only reachable from the tree -- a click in the scene already
    // lands on the owner -- so offer the way there.
    if (body->partOf) {
        const std::string owner = body->partOf->name.empty() ? std::string("owner") : body->partOf->name;
        if (panel.Button("Select " + owner)) {
            m_Explorer.Select(body->partOf);
            return;
        }
    }

    BeginTracking(body);
    DrawFieldGroups(panel, *body, key);
    EndTracking();

    const SceneOverrides::Entry* entry = m_Overrides.FindEntry(key);
    if (!entry) return;

    panel.Separator();
    panel.Label("Saved edits (" + std::to_string(entry->edits.size()) + "), x reverts", UITheme::Warn);
    int revert = -1;
    for (const SceneOverrides::Edit& edit : entry->edits) {
        const SceneFields::Field& field = SceneFields::At(edit.field);
        if (panel.Button("x", width * 0.1f)) revert = edit.field;
        panel.SameLine();
        std::string text = SceneFields::Describe(field, edit.value);
        if (edit.hasBaseline && edit.baseline != edit.value) text += "  was " + SceneFields::Describe(field, edit.baseline);
        panel.KeyValue(field.key, text, UITheme::Warn);
    }
    // After the loop, both: each reshapes the list being drawn.
    const bool revertAll = panel.Button("Revert this object");
    if (revert >= 0) RevertField(body, key, revert);
    else if (revertAll) RevertObject(key);
}

// ============================================================================
// World drawing
// ============================================================================

void SceneEditor::Text(const std::string& text, Vector2 pos, const Color& color) {
    if (!m_Font || !m_Font->IsValid() || text.empty()) return;
    Shader* shader = m_Actors.GetOrCreateNamedShader("Text");
    if (!shader) return;

    const float glyph = Px(8.0f);
    std::vector<Renderer2D::ScreenQuad> quads;
    quads.reserve(text.size());
    for (unsigned char c : text) {
        if (c != ' ') {
            const Font::GlyphUV uv = m_Font->GetGlyphUV(c);
            quads.push_back({{pos.x + glyph * 0.5f, pos.y + glyph * 0.5f}, {glyph, glyph}, color, uv.offset, uv.scale});
        }
        pos.x += glyph;
    }
    m_Renderer.DrawScreenQuadBatch(quads.data(), quads.size(), shader, m_Font->GetAtlas());
}

void SceneEditor::DrawWorld() {
    if (!m_Active) return;
    const Vector2 mouse = m_Input.GetMousePosition();

    // Lights with nothing drawn get an icon in their own color, or there would
    // be nothing on screen to find them by.
    for (const auto& owned : m_Actors.GetBodies()) {
        const RigidBody2D& body = *owned;
        if (!IsIconOnly(body)) continue;
        const Vector2 center = m_Renderer.WorldToScreen(body.transform.position);
        DrawSquare(m_Renderer, center, Px(kIconRadius) * 1.5f + 2.0f, UITheme::PanelBg, kPi * 0.25f);
        DrawSquare(m_Renderer, center, Px(kIconRadius) * 1.5f, Opaque(body.lightEmitter->color), kPi * 0.25f);
    }

    RigidBody2D* selected = m_Explorer.Selected(m_Actors);

    // What a click would pick, outlined, so a pick is never a surprise.
    if (m_Drag.handle == Handle::None && !m_Panning && !m_PanelHasMouse) {
        const std::vector<RigidBody2D*> under = BodiesUnder(mouse);
        int edges = 0;
        const bool onHandle = selected && HitHandle(*selected, mouse, edges) != Handle::None;
        if (!under.empty() && under.front() != selected && !onHandle) {
            const Geometry g = GeometryOf(*under.front());
            if (g.icon) {
                DrawCircle(m_Renderer, g.center, g.half.x + Px(4.0f), Px(1.0f), UITheme::EditHover);
            } else {
                for (int i = 0; i < 4; ++i) {
                    DrawLine(m_Renderer, HandlePoint(g.center, g.half, g.axisX, g.axisY, kCorners[i]),
                             HandlePoint(g.center, g.half, g.axisX, g.axisY, kCorners[(i + 1) % 4]),
                             Px(1.0f), UITheme::EditHover);
                }
            }
        }
    }

    if (selected) DrawGizmo(*selected, mouse);
    DrawStatus();
}

void SceneEditor::DrawGizmo(const RigidBody2D& body, const Vector2& mouse) {
    const Geometry g = GeometryOf(body);
    const float pixelsPerUnit = m_Renderer.PixelsPerWorldUnit();
    const float line = Px(1.0f);
    const float knob = Px(kHandle);

    int hoverEdges = 0;
    Handle hover = Handle::None;
    if (m_Drag.handle != Handle::None) {
        hover = m_Drag.handle;
        hoverEdges = m_Drag.edges;
    } else if (!m_PanelHasMouse) {
        hover = HitHandle(body, mouse, hoverEdges);
    }
    auto hot = [&](Handle handle, int edges) { return hover == handle && (edges == 0 || edges == hoverEdges); };

    // The selection itself.
    if (g.icon) {
        DrawCircle(m_Renderer, g.center, g.half.x + Px(4.0f), line, UITheme::GizmoSelected);
    } else {
        for (int i = 0; i < 4; ++i) {
            DrawLine(m_Renderer, HandlePoint(g.center, g.half, g.axisX, g.axisY, kCorners[i]),
                     HandlePoint(g.center, g.half, g.axisX, g.axisY, kCorners[(i + 1) % 4]),
                     line, UITheme::GizmoSelected);
        }
    }

    // The collider, whatever the tool -- faint unless it is what's being edited.
    // It is what the object actually collides as, which is rarely quite what
    // is drawn.
    Vector2 colliderCenter, colliderHalf;
    float colliderRadius = 0.0f;
    const bool box = body.collisionShape && body.collisionShape->GetType() == CollisionShape2D::Type::Box;
    if (body.collisionShape) {
        const CollisionShape2D& shape = *body.collisionShape;
        colliderCenter = m_Renderer.WorldToScreen(body.transform.position + shape.GetOffset());
        colliderHalf = shape.GetHalfExtents() * pixelsPerUnit;
        colliderRadius = shape.GetRadius() * pixelsPerUnit;
        const Color tint = m_Tool == Tool::Collider ? UITheme::KindCollider : Faded(UITheme::KindCollider, 0.55f);
        if (box) DrawRectOutline(m_Renderer, colliderCenter, colliderHalf, line, tint);
        else DrawCircle(m_Renderer, colliderCenter, colliderRadius, line, tint);
    }

    switch (m_Tool) {
        case Tool::Move: {
            const float head = Px(7.0f);
            const bool hotX = hot(Handle::AxisX, 0);
            const bool hotY = hot(Handle::AxisY, 0);
            DrawArrow(m_Renderer, g.center, g.center + Vector2(Px(kArrow), 0.0f), hotX ? Px(2.0f) : line, head,
                      hotX ? UITheme::EditHandle : UITheme::EditAxisX);
            DrawArrow(m_Renderer, g.center, g.center + Vector2(0.0f, Px(kArrow)), hotY ? Px(2.0f) : line, head,
                      hotY ? UITheme::EditHandle : UITheme::EditAxisY);
            DrawKnob(m_Renderer, g.center, knob, UITheme::EditHandle, hot(Handle::Move, 0));
            break;
        }

        case Tool::Rotate: {
            const float ring = RingRadius(g.half, Px(kRingPad), Px(28.0f));
            const bool hotRing = hot(Handle::Rotate, 0);
            DrawCircle(m_Renderer, g.center, ring, hotRing ? Px(2.0f) : line,
                       hotRing ? UITheme::EditHandle : UITheme::EditRing);

            const bool cone = AimsCone(body);
            const float angle = cone ? body.lightEmitter->coneDirectionRad : body.transform.rotation;
            const Vector2 hand{std::cos(angle), std::sin(angle)};
            DrawLine(m_Renderer, g.center, g.center + hand * ring, line, UITheme::EditRing);
            DrawKnob(m_Renderer, g.center + hand * ring, knob, UITheme::EditHandle, hotRing);
            if (cone) {
                const float spread = body.lightEmitter->coneAngleRad * 0.5f;
                for (float edge : {angle - spread, angle + spread}) {
                    DrawLine(m_Renderer, g.center, g.center + Vector2(std::cos(edge), std::sin(edge)) * ring,
                             line, Faded(Opaque(body.lightEmitter->color), 0.8f));
                }
            }
            break;
        }

        case Tool::Scale:
            if (g.icon) {
                const float radius = body.lightEmitter->radius * pixelsPerUnit;
                const bool hotRadius = hot(Handle::LightRadius, 0);
                DrawCircle(m_Renderer, g.center, radius, hotRadius ? Px(2.0f) : line,
                           Faded(Opaque(body.lightEmitter->color), 0.8f));
                DrawKnob(m_Renderer, g.center + Vector2(radius, 0.0f), knob, UITheme::EditHandle, hotRadius);
                break;
            }
            for (int mask : kHandleMasks) {
                DrawKnob(m_Renderer, HandlePoint(g.center, g.half, g.axisX, g.axisY, mask), knob,
                         UITheme::EditHandle, hot(Handle::Scale, mask));
            }
            break;

        case Tool::Collider:
            if (!body.collisionShape) {
                Text("no collider: tick Has collider", {g.center.x - Px(120.0f), g.center.y + g.half.y + Px(8.0f)},
                     UITheme::Warn);
                break;
            }
            if (box) {
                for (int mask : kHandleMasks) {
                    DrawKnob(m_Renderer, HandlePoint(colliderCenter, colliderHalf, {1.0f, 0.0f}, {0.0f, 1.0f}, mask),
                             knob, UITheme::KindCollider, hot(Handle::ColliderEdge, mask));
                }
            } else {
                DrawKnob(m_Renderer, colliderCenter + Vector2(colliderRadius, 0.0f), knob, UITheme::KindCollider,
                         hot(Handle::ColliderRadius, 0));
            }
            DrawKnob(m_Renderer, colliderCenter, knob * 0.8f, UITheme::KindCollider, hot(Handle::ColliderMove, 0));
            break;

        default:
            break;
    }

    if (m_Drag.handle == Handle::None || !m_Drag.moved) return;

    // What the drag is doing, in numbers, beside the cursor.
    std::string readout;
    switch (m_Drag.handle) {
        case Handle::Move:
        case Handle::AxisX:
        case Handle::AxisY:
            readout = Pair(body.transform.position, m_MoveSnap < 1.0f ? 2 : 0);
            break;
        case Handle::Rotate:
            readout = Num((AimsCone(body) ? body.lightEmitter->coneDirectionRad : body.transform.rotation) * kRadToDeg, 0) + " deg";
            break;
        case Handle::Scale:
            readout = Num(body.transform.scale.x, 2) + " x " + Num(body.transform.scale.y, 2);
            break;
        case Handle::LightRadius:
            readout = "radius " + Num(body.lightEmitter->radius, 0);
            break;
        case Handle::ColliderMove:
            readout = "offset " + Pair(body.collisionShape->GetOffset(), 1);
            break;
        case Handle::ColliderEdge: {
            const Vector2 size = body.collisionShape->GetHalfExtents() * 2.0f;
            readout = Num(size.x, 1) + " x " + Num(size.y, 1);
            break;
        }
        case Handle::ColliderRadius:
            readout = "radius " + Num(body.collisionShape->GetRadius(), 1);
            break;
        default:
            break;
    }

    const Vector2 at{mouse.x + Px(14.0f), mouse.y + Px(12.0f)};
    const float glyph = Px(8.0f);
    Transform2D background;
    background.position = {at.x + static_cast<float>(readout.size()) * glyph * 0.5f, at.y + glyph * 0.5f};
    m_Renderer.DrawScreenQuad(background, {static_cast<float>(readout.size()) * glyph + Px(8.0f), glyph + Px(6.0f)},
                              UITheme::EditHudBg, nullptr);
    Text(readout, at, UITheme::Text);
}

void SceneEditor::DrawStatus() {
    struct Line {
        std::string text;
        Color color;
    };
    std::vector<Line> lines;

    std::string head = std::string("EDIT  ") + kToolNames[static_cast<int>(m_Tool)] + "  zoom " + ZoomText(Zoom());
    head += m_Snap ? "  snap " + Num(m_MoveSnap, m_MoveSnap < 1.0f ? 2 : 0) : "  no snap";
    head += m_Simulate ? "  live" : "  frozen";
    lines.push_back({head, UITheme::Accent});

    if (m_FlashTime > 0.0f) lines.push_back({m_Flash, m_FlashColor});
    if (m_Overrides.IsDirty() && !m_AutoSave) lines.push_back({"unsaved edits  Ctrl+S saves", UITheme::Warn});

    switch (m_Tool) {
        case Tool::Move:     lines.push_back({"drag an object, or an arrow for one axis", UITheme::TextDim}); break;
        case Tool::Rotate:   lines.push_back({"drag the ring; cone lights aim their beam", UITheme::TextDim}); break;
        case Tool::Scale:    lines.push_back({"drag a handle  Shift keeps the aspect", UITheme::TextDim}); break;
        case Tool::Collider: lines.push_back({"drag the collider's edges or middle", UITheme::TextDim}); break;
        default: break;
    }
    lines.push_back({"click again: pick what's below  Esc: none", UITheme::TextFaint});
    lines.push_back({"RMB/MMB or WASD pan  wheel zoom  F frame", UITheme::TextFaint});
    lines.push_back({"Q E R C tools  Ctrl+Z/Y undo  Ctrl+S save", UITheme::TextFaint});

    // Word-wrapped to the width of the fixed hint lines, so a long flash -- a
    // lock's reason, say -- grows the strip downward instead of off screen.
    constexpr size_t kMeasure = 42;
    std::vector<Line> wrapped;
    for (const Line& line : lines) {
        if (line.text.size() <= kMeasure) {
            wrapped.push_back(line);
            continue;
        }
        std::string current;
        size_t start = 0;
        while (start < line.text.size()) {
            size_t end = line.text.find(' ', start);
            if (end == std::string::npos) end = line.text.size();
            const std::string word = line.text.substr(start, end - start);
            if (!current.empty() && current.size() + 1 + word.size() > kMeasure) {
                wrapped.push_back({current, line.color});
                current.clear();
            }
            current += (current.empty() ? "" : " ") + word;
            start = end + 1;
        }
        if (!current.empty()) wrapped.push_back({current, line.color});
    }
    lines.swap(wrapped);

    size_t widest = 0;
    for (const Line& line : lines) widest = std::max(widest, line.text.size());

    const float glyph = Px(8.0f);
    const float lineHeight = Px(11.0f);
    const float pad = Px(6.0f);
    const Vector2 size{static_cast<float>(widest) * glyph + pad * 2.0f,
                       static_cast<float>(lines.size()) * lineHeight + pad * 2.0f - (lineHeight - glyph)};

    // Bottom-right: the panel owns the left edge, and the top is where most
    // scenes keep their sky rather than their subject.
    const Vector2 screen = m_Renderer.GetScreenSize();
    const Vector2 corner{screen.x - Px(8.0f) - size.x, screen.y - Px(8.0f) - size.y};

    Transform2D background;
    background.position = corner + size * 0.5f;
    m_Renderer.DrawScreenQuad(background, size, UITheme::EditHudBg, nullptr);

    Vector2 cursor{corner.x + pad, corner.y + pad};
    for (const Line& line : lines) {
        Text(line.text, cursor, line.color);
        cursor.y += lineHeight;
    }
}
