#pragma once
#include "SceneFields.h"
#include "SceneOverrides.h"
#include "Core/Math/Color.h"
#include "Core/Math/Transform2D.h"
#include "Core/Math/Vector2.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class ActorRegistry;
class Font;
class PlayerActorConfig;
class Renderer2D;
class RigidBody2D;
class SceneExplorer;
class ScriptEngine;
class UIPanel;
class UserInputService;

// The debug menu's edit mode: a free camera over the running scene, click to
// pick, drag to move / rotate / scale / re-shape colliders, and an inspector
// over every field in SceneFields -- where every change is also recorded
// into SceneOverrides, saved to <script>.scene, and laid back over the scene
// each time the scripts build it. That last part is what makes an edit stick:
// the scripts still build the scene exactly as they always did, and the
// edits land on top of it after Init(), on every load and every run.
//
// Nothing here is special-cased per edit path. Each bit of editor code that
// touches a body -- a gizmo drag, an inspector widget, a button -- runs
// between BeginTracking() and EndTracking(), which snapshot every field before
// and after and record whatever differs. So a new widget or tool is persisted
// and undoable without knowing either exists.
//
// Selection is SceneExplorer's, so the tree and the world always agree, and
// like it every stored RigidBody2D* is dropped on reload rather than trusted.
class SceneEditor {
public:
    enum class Tool { Move, Rotate, Scale, Collider, Count };
    enum class AssetType { Light, Rectangle, Character };

    SceneEditor(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input,
                ScriptEngine& scripts, SceneExplorer& explorer);
    ~SceneEditor();

    // Wired to ScriptEngine::SetOnLoaded. The first call finds and reads the
    // .scene file; every call lays the edits over the scene just built.
    void OnSceneLoaded();

    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    // True while editing with "Simulate" off: the engine holds the sim clock
    // at zero so nothing falls, walks or drifts while it is being placed.
    bool FreezesSimulation() const { return m_Active && !m_Simulate; }

    // Before the scripts run: hotkeys, the free camera and the gizmo drag,
    // all mapped through the view the user was looking at when they acted.
    // panelHasMouse is the debug panel's claim on the cursor.
    void BeginFrame(float realDeltaTime, bool panelHasMouse, float uiScale);

    // After the scripts run: notices a dragged body the scripts put straight
    // back, which is how a script-driven part (a leg canvas) gives itself away.
    void AfterScripts();

    // Panel sections. DrawControls is the editor's own section; DrawProperties
    // stands in for the Inspector while edit mode is on.
    void DrawControls(UIPanel& panel);
    void DrawProperties(UIPanel& panel);

    // Screen-space gizmos and the status strip. After the scene, before the panel.
    void DrawWorld();

    // Short state for the section header: "editing", "3 edits", "unsaved".
    std::string Summary() const;

    // Set by the panel when the scene has to be rebuilt -- after discarding
    // every edit, say. The owner does the reload at a safe point in its frame.
    bool TakeReloadRequest();

    // Called by the Asset Menu: the next world click places one of `type` and
    // records it in SceneOverrides so it persists. Escape or another ArmPlacement
    // cancels a pending one.
    void ArmPlacement(AssetType type);
    bool HasPendingPlacement() const { return m_PendingPlacement.has_value(); }

private:
    // --- Placing and spawning persisted assets ------------------------------
    static const char* AssetTypeName(AssetType type);
    // Creates the live body for a spawn of this type at `position` -- the one
    // piece of work shared by placing a brand new asset and recreating every
    // persisted one on load. Light is built directly through ActorRegistry;
    // Rectangle and Character call into Lua (scripts/core/spawn_registry.lua)
    // so the object gets a per-frame Draw()/Update() the way every other
    // visible or animated body in this engine does.
    void CreateAssetBody(AssetType type, const std::string& name, const Vector2& position);
    // Walks m_Overrides.Spawns() in order and recreates each one. Called from
    // OnSceneLoaded() before Apply(), so field edits land on top as usual.
    void SpawnPersistedAssets();
    // The click that lands while a placement is armed: creates, records, saves
    // and selects the new asset.
    void PlaceAsset(AssetType type, const Vector2& world);
    // Next name for a new spawn of this type -- disambiguated the same way
    // SceneOverrides::KeyFor disambiguates bodies, since a spawn's base name
    // is unique to its type and nothing else ever creates a body under it.
    std::string NextAssetName(AssetType type) const;


    enum class Handle { None, Move, AxisX, AxisY, Rotate, Scale, LightRadius,
                        ColliderMove, ColliderEdge, ColliderRadius };

    // Which sides a Scale or ColliderEdge handle drags, as a bitmask.
    static constexpr int kLeft = 1, kRight = 2, kTop = 4, kBottom = 8;

    struct Drag {
        Handle handle = Handle::None;
        int edges = 0;
        RigidBody2D* body = nullptr;
        Vector2 startMouseScreen;
        Vector2 startMouseWorld;
        Transform2D startTransform;
        Vector2 startColliderOffset;
        Vector2 startHalfExtents;
        float startRadius = 0.0f;
        float startLightRadius = 0.0f;
        float startConeAim = 0.0f;
        bool moved = false;       // past the dead zone, so a click is not a nudge
        bool clickCycles = false; // a click on the selection steps to what is under it
    };

    // One field's change inside an undo step. Holds the override store's
    // entry on both sides as well as the body's value, so undo puts the file
    // back exactly as it was, not just the object.
    struct Change {
        std::string key;
        int field = -1;
        SceneFields::Value before, after;
        bool beforePresent = false, afterPresent = false;
        std::optional<SceneOverrides::Edit> editBefore, editAfter;
    };
    struct Step {
        std::string label;
        std::vector<Change> changes;
    };

    // --- Change tracking ----------------------------------------------------
    void BeginTracking(RigidBody2D* body);
    void EndTracking();
    void NoteChange(const std::string& key, int field, bool beforePresent, bool afterPresent,
                    const SceneFields::Value& before, const SceneFields::Value& after);
    void CommitGesture();
    void PushStep(Step step);
    void ApplyEditState(const std::string& key, int field, const std::optional<SceneOverrides::Edit>& edit);
    void Undo();
    void Redo();
    void RevertField(RigidBody2D* body, const std::string& key, int field);
    void RevertObject(const std::string& key);

    // --- Persistence --------------------------------------------------------
    void SaveNow(bool announce);
    void AfterEditsChanged();

    // --- Free camera --------------------------------------------------------
    void UpdateFreeLook(float deltaTime, bool panelHasMouse, bool ctrl, bool shift);
    void FrameSelection();
    void ResetView();
    float Zoom() const;
    bool HasGameCamera() const;

    // Player input is switched off while editing, or WASD would walk the
    // player around underneath the camera it is also panning.
    void MuteActors();
    void UnmuteActors();

    // --- Picking and gizmos -------------------------------------------------
    struct Geometry {
        Vector2 center;   // screen
        Vector2 half;     // screen pixels, along the body's own axes
        Vector2 axisX, axisY;
        bool icon = false;
    };
    Geometry GeometryOf(const RigidBody2D& body) const;
    bool IsIconOnly(const RigidBody2D& body) const;
    // On the body's drawn pixels, not just its box: a sprite's transparent
    // texels -- the sky rows of a terrain chunk, the air around a leg -- are
    // not the body. Icon-only lights test their icon.
    bool UnderCursor(const RigidBody2D& body, const Vector2& mouse) const;
    std::vector<RigidBody2D*> BodiesUnder(const Vector2& mouse) const;
    Handle HitHandle(const RigidBody2D& body, const Vector2& mouse, int& edges) const;
    void PressInWorld(RigidBody2D* selected, const Vector2& mouse, const Vector2& world);
    void StartDrag(RigidBody2D& body, Handle handle, int edges, const Vector2& mouse, const Vector2& world);
    void UpdateDrag(const Vector2& mouse, const Vector2& world, bool snap, bool uniform);
    void EndDrag();
    void CancelDrag();
    bool AimsCone(const RigidBody2D& body) const;
    void DropToGround(RigidBody2D& body);

    // Why a field can't be edited on this body: its own lock, or the editor's
    // (a transform the scripts keep putting back). Empty when it can.
    std::string FieldLock(const SceneFields::Field& field, const RigidBody2D& body) const;
    // The same, for the field a gizmo handle would drag.
    std::string HandleLock(const RigidBody2D& body, Handle handle) const;

    // --- Drawing ------------------------------------------------------------
    void DrawField(UIPanel& panel, int id, RigidBody2D& body, const std::string& key);
    // Every group the body has, with its sub-headings and group buttons.
    // Callers bracket it with BeginTracking/EndTracking.
    void DrawFieldGroups(UIPanel& panel, RigidBody2D& body, const std::string& key);
    void DrawGizmo(const RigidBody2D& body, const Vector2& mouse);
    void DrawStatus();
    void Text(const std::string& text, Vector2 pos, const Color& color);
    float Px(float base) const { return base * m_UIScale; }
    void Flash(const std::string& text, const Color& color);

    bool Down(int keycode) const;
    bool Pressed(int keycode) const;

    ActorRegistry& m_Actors;
    Renderer2D& m_Renderer;
    UserInputService& m_Input;
    ScriptEngine& m_Scripts;
    SceneExplorer& m_Explorer;
    std::unique_ptr<Font> m_Font;

    SceneOverrides m_Overrides;
    std::string m_ScenePath;
    bool m_FileLoaded = false;
    bool m_ReloadRequested = false;

    bool m_Active = false;
    Tool m_Tool = Tool::Move;
    bool m_Simulate = false;
    bool m_Snap = true;
    float m_MoveSnap = 1.0f;       // world units
    float m_RotateSnapDeg = 15.0f;
    float m_ScaleSnap = 0.25f;
    bool m_ColliderFollowsScale = true;
    bool m_AutoSave = true;
    bool m_ApplyOnLoad = true;

    // Free camera. Zoom is an index into a fixed table of clean ratios, so the
    // pixel art never lands between texels.
    Vector2 m_ViewPos;
    int m_ZoomIndex = 2;
    bool m_Panning = false;
    Vector2 m_PanLastMouse;
    float m_UIScale = 1.0f;
    bool m_PanelHasMouse = false;

    std::vector<PlayerActorConfig*> m_MutedActors;

    // Set by the Asset Menu; consumed by the next world click in BeginFrame().
    std::optional<AssetType> m_PendingPlacement;

    Drag m_Drag;

    RigidBody2D* m_TrackBody = nullptr;
    SceneFields::Snapshot m_TrackBefore;
    Step m_Gesture;
    std::vector<Step> m_Undo, m_Redo;

    // Script-driven detection: where this frame's drag put the body, checked
    // against where it is once the scripts are done with it.
    RigidBody2D* m_WroteBody = nullptr;
    Vector2 m_WrotePos;
    int m_SnapBackFrames = 0;
    std::unordered_set<const RigidBody2D*> m_Driven;

    std::string m_Flash;
    Color m_FlashColor;
    float m_FlashTime = 0.0f;
    float m_DiscardArmed = 0.0f; // seconds left to confirm "Discard all"

    struct Keys {
        int q = 0, e = 0, r = 0, c = 0, f = 0, z = 0, y = 0, s = 0;
        int w = 0, a = 0, d = 0, up = 0, down = 0, left = 0, right = 0;
        int shift = 0, ctrl = 0, tab = 0, escape = 0;
    } m_Keys;
};
