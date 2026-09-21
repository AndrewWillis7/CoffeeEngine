#include "SceneOverrides.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace {

// What an unnamed body is keyed by. Deliberately NOT SceneExplorer::Classify:
// that reads collisionShape, which the editor itself can switch on, and a key
// that changes the moment you give something a collider would strand every
// edit made before it. Only tags the editor never adds or removes count here.
const char* TagName(const RigidBody2D& body) {
    if (body.playerConfig) return "Actor";
    if (body.camera)       return "Camera";
    if (body.lightEmitter) return "Light";
    if (body.terrain)      return "Terrain";
    if (body.sprite)       return "Sprite";
    return "Body";
}

std::string BaseName(const RigidBody2D& body) {
    return body.name.empty() ? std::string("@") + TagName(body) : body.name;
}

// The scene itself, as far as the file is concerned: world fields (gravity)
// sit on SceneFields::WorldProxy() under this key.
constexpr const char* kWorldKey = "@World#0";

// Every live body under its key, in one pass over the registry, rather than
// one scan per lookup.
std::unordered_map<std::string, RigidBody2D*> KeyAll(const ActorRegistry& actors) {
    std::unordered_map<std::string, RigidBody2D*> byKey;
    byKey[kWorldKey] = &SceneFields::WorldProxy();
    std::unordered_map<std::string, int> seen;
    for (const auto& owned : actors.GetBodies()) {
        const std::string base = BaseName(*owned);
        const int ordinal = seen[base]++;
        byKey[base + "#" + std::to_string(ordinal)] = owned.get();
    }
    return byKey;
}

std::string NumberText(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
    return buffer;
}

std::string Trimmed(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

} // namespace

std::string SceneOverrides::KeyFor(const ActorRegistry& actors, const RigidBody2D& body) {
    if (&body == &SceneFields::WorldProxy()) return kWorldKey;
    const std::string base = BaseName(body);
    int ordinal = 0;
    for (const auto& owned : actors.GetBodies()) {
        if (owned.get() == &body) break;
        if (BaseName(*owned) == base) ++ordinal;
    }
    return base + "#" + std::to_string(ordinal);
}

RigidBody2D* SceneOverrides::Resolve(const ActorRegistry& actors, const std::string& key) {
    if (key == kWorldKey) return &SceneFields::WorldProxy();
    // Split on the LAST '#', so a name that itself contains one still parses.
    const size_t hash = key.rfind('#');
    if (hash == std::string::npos) return nullptr;
    const std::string base = key.substr(0, hash);
    const int wanted = std::atoi(key.c_str() + hash + 1);

    int ordinal = 0;
    for (const auto& owned : actors.GetBodies()) {
        if (BaseName(*owned) != base) continue;
        if (ordinal++ == wanted) return owned.get();
    }
    return nullptr;
}

SceneOverrides::Entry* SceneOverrides::FindEntryMutable(const std::string& key) {
    for (Entry& entry : m_Entries) {
        if (entry.key == key) return &entry;
    }
    return nullptr;
}

const SceneOverrides::Entry* SceneOverrides::FindEntry(const std::string& key) const {
    for (const Entry& entry : m_Entries) {
        if (entry.key == key) return &entry;
    }
    return nullptr;
}

const SceneOverrides::Edit* SceneOverrides::Find(const std::string& key, int field) const {
    const Entry* entry = FindEntry(key);
    if (!entry) return nullptr;
    for (const Edit& edit : entry->edits) {
        if (edit.field == field) return &edit;
    }
    return nullptr;
}

void SceneOverrides::Set(const std::string& key, const Edit& edit) {
    Entry* entry = FindEntryMutable(key);
    if (!entry) {
        m_Entries.push_back({key, {}, true});
        entry = &m_Entries.back();
    }

    auto it = std::find_if(entry->edits.begin(), entry->edits.end(),
                           [&](const Edit& e) { return e.field >= edit.field; });
    if (it != entry->edits.end() && it->field == edit.field) *it = edit;
    else entry->edits.insert(it, edit);
    m_Dirty = true;
}

void SceneOverrides::Forget(const std::string& key, int field) {
    Entry* entry = FindEntryMutable(key);
    if (!entry) return;
    const size_t before = entry->edits.size();
    entry->edits.erase(std::remove_if(entry->edits.begin(), entry->edits.end(),
                                      [&](const Edit& e) { return e.field == field; }),
                       entry->edits.end());
    if (entry->edits.size() != before) m_Dirty = true;
    if (entry->edits.empty()) ForgetObject(key);
}

void SceneOverrides::ForgetObject(const std::string& key) {
    const size_t before = m_Entries.size();
    m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(),
                                   [&](const Entry& e) { return e.key == key; }),
                    m_Entries.end());
    if (m_Entries.size() != before) m_Dirty = true;
}

void SceneOverrides::Clear() {
    if (!m_Entries.empty()) m_Dirty = true;
    m_Entries.clear();
}

bool SceneOverrides::HasSpawn(const std::string& key) const {
    const size_t hash = key.rfind('#');
    if (hash == std::string::npos) return false;
    const std::string name = key.substr(0, hash);
    const int wanted = std::atoi(key.c_str() + hash + 1);

    int ordinal = 0;
    for (const Spawn& spawn : m_Spawns) {
        if (spawn.name != name) continue;
        if (ordinal++ == wanted) return true;
    }
    return false;
}

void SceneOverrides::AddSpawn(const Spawn& spawn) {
    m_Spawns.push_back(spawn);
    m_Dirty = true;
}

void SceneOverrides::RemoveSpawn(const std::string& key) {
    // `key` is a body key ("Debug Quad#2"), exactly as KeyFor produces once the
    // spawn has a live body. Ordinals are recomputed the same way KeyAll does,
    // among spawns sharing the name -- which is safe only because nothing but
    // a spawn ever creates a body under one of these names (see Spawn's
    // comment in the header).
    const size_t hash = key.rfind('#');
    if (hash == std::string::npos) return;
    const std::string name = key.substr(0, hash);
    const int wanted = std::atoi(key.c_str() + hash + 1);

    int ordinal = 0;
    for (size_t i = 0; i < m_Spawns.size(); ++i) {
        if (m_Spawns[i].name != name) continue;
        if (ordinal++ == wanted) {
            m_Spawns.erase(m_Spawns.begin() + static_cast<long>(i));
            m_Dirty = true;
            return;
        }
    }
}

size_t SceneOverrides::FieldCount() const {
    size_t count = 0;
    for (const Entry& entry : m_Entries) count += entry.edits.size();
    return count;
}

int SceneOverrides::Apply(ActorRegistry& actors) {
    const auto byKey = KeyAll(actors);
    int applied = 0;
    std::string missing;

    for (Entry& entry : m_Entries) {
        const auto found = byKey.find(entry.key);
        entry.resolved = found != byKey.end();
        if (!entry.resolved) {
            missing += (missing.empty() ? "" : ", ") + entry.key;
            continue;
        }

        RigidBody2D& body = *found->second;
        bool regenerate = false;
        for (Edit& edit : entry.edits) {
            const SceneFields::Field& field = SceneFields::At(edit.field);
            // Checked live, edit by edit: collider.enabled earlier in the same
            // entry is what makes collider.offset present by the time it runs.
            if (!SceneFields::IsPresent(field, body)) continue;

            edit.baseline = SceneFields::Get(field, body);
            edit.hasBaseline = true;
            SceneFields::Set(field, body, edit.value, actors, /*regenerate=*/false);
            regenerate = regenerate || field.regenerates;
            ++applied;
        }
        // Once per body, however many generation inputs it had.
        if (regenerate) {
            SceneFields::RegenerateTerrain(body);
            SceneFields::RegenerateDebugQuad(body, actors);
        }
    }

    if (!missing.empty()) {
        std::cerr << "Engine Warning: scene edits kept for objects the scripts no longer build: "
                  << missing << "\n";
    }
    return applied;
}

bool SceneOverrides::Load(const std::string& path) {
    std::ifstream file(path);
    m_Entries.clear();
    m_Spawns.clear();
    m_Dirty = false;
    if (!file) return true;

    std::string line;
    int lineNumber = 0;
    Entry* current = nullptr;
    int rejected = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string text = Trimmed(line);
        if (text.empty() || text[0] == '#' || text[0] == ';') continue;

        if (text.compare(0, 6, "spawn ") == 0) {
            const std::string rest = Trimmed(text.substr(6));
            const size_t space = rest.find(' ');
            bool ok = space != std::string::npos;
            float x = 0.0f, y = 0.0f;
            std::string name;
            if (ok) {
                char* afterX = nullptr;
                x = std::strtof(rest.c_str() + space, &afterX);
                ok = afterX != rest.c_str() + space;
                if (ok) {
                    char* afterY = nullptr;
                    y = std::strtof(afterX, &afterY);
                    ok = afterY != afterX;
                    if (ok) name = Trimmed(std::string(afterY));
                }
            }
            if (!ok || name.empty()) {
                std::cerr << "Engine Warning: " << path << ":" << lineNumber << ": bad spawn line '" << text << "'\n";
                ++rejected;
                continue;
            }
            m_Spawns.push_back({rest.substr(0, space), name, Vector2(x, y)});
            continue;
        }

        if (text[0] == '[') {
            const size_t close = text.rfind(']');
            if (close == std::string::npos || close < 2) {
                std::cerr << "Engine Warning: " << path << ":" << lineNumber << ": bad section '" << text << "'\n";
                current = nullptr;
                ++rejected;
                continue;
            }
            const std::string key = text.substr(1, close - 1);
            current = FindEntryMutable(key);
            if (!current) {
                m_Entries.push_back({key, {}, false});
                current = &m_Entries.back();
            }
            continue;
        }

        const size_t equals = text.find('=');
        if (!current || equals == std::string::npos) {
            std::cerr << "Engine Warning: " << path << ":" << lineNumber << ": ignored '" << text << "'\n";
            ++rejected;
            continue;
        }

        const std::string name = Trimmed(text.substr(0, equals));
        std::string valueText = text.substr(equals + 1);
        // Values never contain '#', so everything after one is a note.
        if (const size_t hash = valueText.find('#'); hash != std::string::npos) valueText.resize(hash);

        const int index = SceneFields::IndexOf(name);
        SceneFields::Value value;
        if (index < 0 || !SceneFields::Parse(SceneFields::At(index), valueText, value)) {
            std::cerr << "Engine Warning: " << path << ":" << lineNumber << ": "
                      << (index < 0 ? "unknown field '" : "unreadable value for '") << name << "'\n";
            ++rejected;
            continue;
        }

        Edit edit;
        edit.field = index;
        edit.value = value;
        auto it = std::find_if(current->edits.begin(), current->edits.end(),
                               [&](const Edit& e) { return e.field >= index; });
        if (it != current->edits.end() && it->field == index) *it = edit;
        else current->edits.insert(it, edit);
    }

    // An empty section is harmless but pointless; drop it now rather than
    // carry it forward into the next save.
    m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(),
                                   [](const Entry& e) { return e.edits.empty(); }),
                    m_Entries.end());

    std::cout << "Scene edits: loaded " << FieldCount() << " field(s) on " << m_Entries.size()
              << " object(s) and " << m_Spawns.size() << " placed asset(s) from " << path
              << (rejected ? " (" + std::to_string(rejected) + " line(s) skipped)" : std::string()) << "\n";
    return true;
}

bool SceneOverrides::Save(const std::string& path, const std::string& scriptPath) {

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        std::cerr << "Engine Warning: could not write scene edits to '" << path << "'\n";
        return false;
    }

    file << "# CoffeeEngine scene edits for " << scriptPath << "\n"
         << "#\n"
         << "# Written by the Scene Editor in the debug menu. After the scripts' Init()\n"
         << "# builds the scene, each line below replaces the value the scripts gave that\n"
         << "# field. [Name#n] is the nth object (from 0, in creation order) the scripts\n"
         << "# named Name; unnamed objects go by what they are, as [@Light#0]. Keep the\n"
         << "# scripts creating things in the same order or an edit lands on a different\n"
         << "# object. '# script:' notes record what the scripts built, for reference\n"
         << "# only. Delete a line to drop that edit.\n"
         << "#\n"
         << "# 'spawn <type> <x> <y> <name>' lines are objects the EDITOR created, not the\n"
         << "# scripts -- the Asset Menu's placed lights, rectangles and characters. They\n"
         << "# are recreated at that position before the edits above are applied, so a\n"
         << "# [name#n] section below can go on to move/resize/restyle them exactly like\n"
         << "# any scripted object. Delete a spawn line to remove that asset for good.\n";

    for (const Spawn& spawn : m_Spawns) {
        file << "spawn " << spawn.type << " " << NumberText(spawn.position.x) << " "
             << NumberText(spawn.position.y) << " " << spawn.name << "\n";
    }

    for (const Entry& entry : m_Entries) {
        file << "\n[" << entry.key << "]\n";
        for (const Edit& edit : entry.edits) {
            const SceneFields::Field& field = SceneFields::At(edit.field);
            std::string line = std::string(field.key) + " = " + SceneFields::Format(field, edit.value);
            if (edit.hasBaseline && edit.baseline != edit.value) {
                if (line.size() < 40) line.append(40 - line.size(), ' ');
                line += "  # script: " + SceneFields::Format(field, edit.baseline);
            }
            file << line << "\n";
        }
    }

    if (!file) {
        std::cerr << "Engine Warning: writing scene edits to '" << path << "' failed partway\n";
        return false;
    }
    m_Dirty = false;
    return true;
}
