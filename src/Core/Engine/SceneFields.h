#pragma once
#include "Core/Math/Vector2.h"
#include "Core/Math/Color.h"
#include <cstddef>
#include <string>
#include <vector>

class RigidBody2D;
class ActorRegistry;
struct ScriptTunable;

// Every per-body property the scene editor can show, change, save to disk and
// lay back over a freshly loaded scene. It is one table (SceneFields.cpp), so
// a field becomes editable, undoable and persisted by adding exactly one entry:
// the inspector draws from it, the editor diffs with it, and SceneOverrides
// reads and writes the .scene file through it.
//
// Three kinds of entry share the table:
//   - editable fields, with a get and a set;
//   - Info rows, read-only by nature (velocity, a sprite's size), which the
//     inspector shows behind a padlock with the reason they can't change;
//   - world fields (gravity), present only on WorldProxy().
// Plus one kind that is not in the table at all: script fields, one per value
// a Lua script offered with body:Expose(). They are interned the first time a
// body or a .scene file mentions them, and behave like any other field after.
namespace SceneFields {

enum class Type { Float, Angle, Int, Bool, Vec2, Color, Enum, Info };

// Big enough for the widest type, a Color. Angles are held in radians the way
// the engine stores them, and only become degrees in the file and the UI.
struct Value {
    float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    static Value Of(float a)            { Value x; x.v[0] = a; return x; }
    static Value Of(int a)              { return Of(static_cast<float>(a)); }
    static Value Of(bool a)             { return Of(a ? 1.0f : 0.0f); }
    static Value Of(const Vector2& a)   { Value x; x.v[0] = a.x; x.v[1] = a.y; return x; }
    static Value Of(const Color& a)     { Value x; x.v[0] = a.r; x.v[1] = a.g; x.v[2] = a.b; x.v[3] = a.a; return x; }

    float F() const   { return v[0]; }
    int I() const     { return static_cast<int>(v[0] < 0.0f ? v[0] - 0.5f : v[0] + 0.5f); }
    bool B() const    { return v[0] != 0.0f; }
    Vector2 V2() const { return {v[0], v[1]}; }
    Color C() const   { return {v[0], v[1], v[2], v[3]}; }

    // Exact on purpose: the editor diffs to learn what IT just wrote, and a
    // tolerance would swallow a small but deliberate nudge.
    bool operator==(const Value& o) const {
        return v[0] == o.v[0] && v[1] == o.v[1] && v[2] == o.v[2] && v[3] == o.v[3];
    }
    bool operator!=(const Value& o) const { return !(*this == o); }
};

// Inspector sections, in the order they are drawn.
enum class Group { Transform, Body, Collider, Light, Actor, Script, Camera, Terrain, Asset, World, Count };
const char* GroupName(Group group);

struct Field {
    const char* key;    // name in the .scene file, e.g. "light.radius"
    const char* label;  // what the inspector calls it
    Group group;
    Type type;
    // A faint sub-heading the inspector draws when it changes inside a group,
    // so a long group (terrain) reads in parts. Null continues the last one.
    const char* section = nullptr;

    // Inspector hints: value per dragged pixel, shown decimals (which is also
    // what a drag rounds to), and a clamp when minValue < maxValue.
    float speed = 0.1f;
    int decimals = 1;
    float minValue = 0.0f;
    float maxValue = 0.0f;

    // Enum only: the tokens the file and the UI use, indexed by the value.
    const char* const* enumNames = nullptr;
    int enumCount = 0;

    // Null means every body has this field. Evaluated live, so switching a
    // collider on makes the collider.* fields appear from then on.
    bool (*present)(const RigidBody2D&) = nullptr;
    Value (*get)(const RigidBody2D&) = nullptr;
    // Takes the registry so a field can allocate what it needs -- switching a
    // collider on creates one. Null on an Info row.
    void (*set)(RigidBody2D&, const Value&, ActorRegistry&) = nullptr;

    // Info rows: the text shown, and why it can't be edited.
    std::string (*describe)(const RigidBody2D&) = nullptr;
    const char* lockReason = nullptr;

    // Editable in general, but not on this body right now -- a leg canvas's
    // position, which its owner rewrites every frame. Empty means unlocked.
    std::string (*lockedWhy)(const RigidBody2D&) = nullptr;

    // Terrain generation input: meaningless until the chunk is regenerated.
    bool regenerates = false;
    // Present on WorldProxy() and nothing else.
    bool world = false;
    // Interned from a ScriptTunable rather than declared in the table.
    bool script = false;
};

// Ids: table entries are 0..Count()-1; script fields are interned above that.
size_t Count();
const Field& At(int id);
// -1 for an unknown key. A "script.*" key is interned on first sight, so a
// .scene file can name a script field before any body has exposed it.
int IndexOf(const std::string& key);

// Every field id that can apply to this body: the whole table (presence is
// still per field) plus one per value its scripts exposed.
std::vector<int> FieldsOf(const RigidBody2D& body);

bool IsPresent(const Field& field, const RigidBody2D& body);
bool IsEditable(const Field& field);
// Why this field can't be changed on this body right now, or empty.
std::string LockReason(const Field& field, const RigidBody2D& body);

Value Get(const Field& field, const RigidBody2D& body);
// field.set, plus the terrain regenerate a generation input needs unless the
// caller batches that itself (SceneOverrides::Apply does, once per body).
void Set(const Field& field, RigidBody2D& body, const Value& value, ActorRegistry& actors,
         bool regenerate = true);
void RegenerateTerrain(RigidBody2D& body);
// Same idea for a placed debug quad: resizes its sprite/body size to match
// its cellsX/cellsY and redraws the grid texture.
void RegenerateDebugQuad(RigidBody2D& body, ActorRegistry& actors);

// The script tunable a script field reads through on this body, or null.
const ScriptTunable* TunableFor(const Field& field, const RigidBody2D& body);

// Stands in for the scene itself: world fields (gravity) live on it, so the
// editor's tracking, undo and saving work on them unchanged. Never in the
// registry; keyed "@World#0" in the .scene file.
RigidBody2D& WorldProxy();

// File form: "198 128", "true", "box", angles in degrees. Parse is the exact
// inverse, and fails on anything it does not recognise.
std::string Format(const Field& field, const Value& value);
bool Parse(const Field& field, const std::string& text, Value& out);

// Short readable form for the panel: "198.0, 128.0", "45 deg", "cone".
std::string Describe(const Field& field, const Value& value);

// Every editable field of one body. The editor captures one right before its
// own code touches a body and one right after, so the difference is exactly
// what the editor did, never what the scripts or physics did.
struct Snapshot {
    struct Entry {
        int field;
        bool present;
        Value value;
    };
    std::vector<Entry> entries;
};
Snapshot Capture(const RigidBody2D& body);

} // namespace SceneFields
