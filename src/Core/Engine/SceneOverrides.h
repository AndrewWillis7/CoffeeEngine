#pragma once
#include "SceneFields.h"
#include "Core/Math/Vector2.h"
#include <string>
#include <vector>

class ActorRegistry;
class RigidBody2D;

// The scene editor's edits, kept apart from the objects they change so they
// can outlive them. Scripts rebuild the whole scene on every load, so an edit
// cannot live on a RigidBody2D: it lives here, under a key the next load will
// reproduce for the same object, and Apply() lays it back over whatever the
// scripts just built. Save()/Load() carry it across runs as a plain text
// .scene file that reads -- and diffs -- like the tunables it holds:
//
//   [Wall#0]
//   position = 198 128        # script: 198 130
//   collider.halfExtents = 8 30
//
class SceneOverrides {
public:
    struct Edit {
        int field = -1;             // index into SceneFields
        SceneFields::Value value;   // what the editor set
        // What the scripts built, for "was" readouts and for reverting without
        // a reload. Refreshed on every Apply(), which sees the scripts' value
        // right before replacing it.
        SceneFields::Value baseline;
        bool hasBaseline = false;
    };

    struct Entry {
        std::string key;
        std::vector<Edit> edits;    // kept in field-table order
        bool resolved = false;      // matched a live body on the last Apply()
    };

    // An editor-placed asset that scripts never create -- a light, a debug
    // quad, a spawned character. Nothing rebuilds these on load the way
    // Init() rebuilds a scripted body, so the file has to say they exist at
    // all. Recreated in this order by SceneEditor::SpawnPersistedAssets()
    // before Apply() runs, which is what lets `name` line up with KeyFor's
    // Name#n scheme and pick up any saved field edits on top.
    struct Spawn {
        std::string type;   // "Light" | "Rectangle" | "Character"
        std::string name;   // base name; ordinal among same-type-and-name spawns
        Vector2 position;   // where it was placed; later moves ride the normal Edit path
    };

    // A key the next load reproduces for the same object: the name the
    // scripts gave it, plus which of the bodies sharing that name it is, in
    // creation order ("Wall#0", "Leg Canvas (front)#1"). Unnamed bodies fall
    // back to what they are ("@Light#0"). The one thing it relies on is the
    // scripts creating things in the same order each load -- a condition the
    // file states at the top, since breaking it is the one way an edit lands
    // on the wrong object.
    static std::string KeyFor(const ActorRegistry& actors, const RigidBody2D& body);

    // The live body a key names, or nullptr.
    static RigidBody2D* Resolve(const ActorRegistry& actors, const std::string& key);

    // Adds or replaces one edit exactly as given, baseline and all -- the
    // editor works out the baseline, since only it knows what the value was
    // before its own gesture began, and undo hands back a saved copy verbatim.
    void Set(const std::string& key, const Edit& edit);
    void Forget(const std::string& key, int field);
    void ForgetObject(const std::string& key);
    void Clear();

    const Edit* Find(const std::string& key, int field) const;
    const Entry* FindEntry(const std::string& key) const;

    // Placed assets, in creation order -- the order SpawnPersistedAssets()
    // must recreate them in.
    const std::vector<Spawn>& Spawns() const { return m_Spawns; }
    // True if `key` (a body key, as KeyFor produces once it has a live body)
    // names one of the placed assets in Spawns() -- what the inspector checks
    // to offer "Delete asset" only on something the editor itself created.
    bool HasSpawn(const std::string& key) const;
    void AddSpawn(const Spawn& spawn);
    // By name: a placed asset's key never has a live body to resolve when this
    // is called (deleting drops the whole thing, not one field).
    void RemoveSpawn(const std::string& name);

    // Lays every edit over the live scene, in field-table order per object.
    // Returns how many fields were applied; keys with no live body are kept,
    // flagged unresolved, and reported once in the log.
    int Apply(ActorRegistry& actors);

    // Missing file loads as empty and succeeds: no edits yet is not an error.
    bool Load(const std::string& path);
    bool Save(const std::string& path, const std::string& scriptPath);

    const std::vector<Entry>& Entries() const { return m_Entries; }
    size_t FieldCount() const;
    bool Empty() const { return m_Entries.empty(); }

    // True once anything changed since the last Load() or Save().
    bool IsDirty() const { return m_Dirty; }

private:
    Entry* FindEntryMutable(const std::string& key);

    std::vector<Entry> m_Entries;
    std::vector<Spawn> m_Spawns;
    bool m_Dirty = false;
};
