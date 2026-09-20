#pragma once
#include "Core/Math/Color.h"
#include <string>
#include <unordered_set>

class ActorRegistry;
class RigidBody2D;
class UIPanel;

// The actor tree in the debug panel: every body in the registry, optionally
// grouped by what its capability tags make it, each expanding into the live
// values of the components attached to it.
//
// Holds view state only -- which rows are open, which body is selected, which
// kinds are filtered out. Every stored RigidBody2D* is revalidated against the
// registry before it is used, because a script reload frees every body at once
// and a debug panel must not be the thing that turns that into a crash.
class SceneExplorer {
public:
    // What a body reads as. A body usually carries several tags, so the order
    // here doubles as the classification priority, and matches the UITheme
    // Kind* colors one for one.
    enum class Kind { Player, Actor, Camera, Light, Terrain, Collider, Sprite, Plain, Count };
    static constexpr int kKindCount = static_cast<int>(Kind::Count);

    // playerActor is ActorRegistry::GetPlayerActor(): the FIRST body built
    // with a PlayerActorConfig. Every character carries one as a tuning bag,
    // so that pointer, not the tag, is what separates the player from an NPC.
    static Kind Classify(const RigidBody2D& body, const RigidBody2D* playerActor);
    static const char* KindName(Kind kind);
    static const char* KindPlural(Kind kind);
    static Color KindColor(Kind kind);

    // A readable name for a body: whatever Lua called it, else its kind.
    static std::string DisplayName(const RigidBody2D& body, size_t index, Kind kind);

    void DrawTree(UIPanel& panel, ActorRegistry& actors);
    void DrawInspector(UIPanel& panel, ActorRegistry& actors);

    // Null unless something is selected AND that body is still in the registry.
    RigidBody2D* Selected(const ActorRegistry& actors) const;
    void ClearSelection() { m_Selected = nullptr; }

    bool* KindFilter(Kind kind) { return &m_ShowKind[static_cast<int>(kind)]; }
    bool* GroupedFlag() { return &m_Grouped; }

private:
    // One line per component the body actually carries, at the given depth.
    void EmitComponents(UIPanel& panel, const RigidBody2D& body, int depth);

    void SetAllExpanded(const ActorRegistry& actors, bool expanded);
    bool IsExpanded(const RigidBody2D* body) const { return m_Expanded.count(body) != 0; }
    void ToggleExpanded(const RigidBody2D* body);

    // Drops pointers to bodies the registry no longer owns.
    void PruneDeadPointers(const ActorRegistry& actors);

    std::unordered_set<const RigidBody2D*> m_Expanded;
    RigidBody2D* m_Selected = nullptr;

    bool m_Grouped = true;
    bool m_ShowKind[kKindCount] = {true, true, true, true, true, true, true, true};
    bool m_GroupOpen[kKindCount] = {true, true, true, true, true, true, true, true};
};
