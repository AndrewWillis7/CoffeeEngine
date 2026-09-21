-- Objects the scene editor's Asset Menu placed at runtime, not scripts/main.lua's
-- own Init(). The engine only ever calls DrawBody()/Update() through Lua (see
-- CLAUDE.md), so anything the editor spawns still needs a Lua-side owner to
-- call those every frame -- exactly what main.lua already does for the player,
-- the NPC it hand-places, the terrain, and everything else in the scene.
--
-- SceneEditor (src/Core/Engine/SceneEditor.cpp) calls into this module by
-- name, through ScriptEngine::CallSpawn(fnName, x, y, name): once right when
-- an asset is placed, and again for every persisted asset when the scene
-- (re)loads. `name` is the exact name the editor's SceneOverrides keys the
-- object's saved position/fields by, so it must be set on the body before
-- anything else touches it.
--
-- Deleting an asset (the inspector's "Delete asset" button) sets its body's
-- `destroyed` flag from C++ rather than tearing it down here -- the body isn't
-- freed until the next reload -- so every list below drops a destroyed entry
-- the next time it's walked instead of calling into a half-torn-down object.

local CharacterFactory = require("core.character_factory")
local Constants = require("core.constants")

local SpawnedNPCs = {}
local SpawnedRectangles = {}

function SpawnNPCAt(x, y, name)
    local npc = CharacterFactory.CreateNPC(x, y, Constants.PLAYER_WIDTH, Constants.PLAYER_HEIGHT)
    npc.body:SetName(name)
    table.insert(SpawnedNPCs, npc)
end

function SpawnRectangleAt(x, y, name)
    local body = Actors.CreateDebugQuad(x, y)
    body:SetName(name)
    table.insert(SpawnedRectangles, body)
end

-- Called once per frame from main.lua's Update(), after the hand-placed NPC
-- has already moved -- `solids` is the same global list every mover uses.
function UpdateSpawnedAssets(deltaTime)
    for i = #SpawnedNPCs, 1, -1 do
        local npc = SpawnedNPCs[i]
        if npc.body:IsDestroyed() then
            table.remove(SpawnedNPCs, i)
        else
            npc:Update(deltaTime, solids, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)
        end
    end

    for i = #SpawnedRectangles, 1, -1 do
        if SpawnedRectangles[i]:IsDestroyed() then table.remove(SpawnedRectangles, i) end
    end
end

-- Called once per frame from main.lua's Update(), alongside the other Draw()
-- calls. Rectangles have no Update of their own -- DrawBody() is all they need.
function DrawSpawnedAssets()
    for _, npc in ipairs(SpawnedNPCs) do npc:Draw() end
    for _, body in ipairs(SpawnedRectangles) do DrawBody(body) end
end
