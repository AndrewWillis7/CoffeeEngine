local Player = require("objects.player")
local StaticBody = require("objects.static_body")
local Prop = require("objects.prop")
local Camera = require("objects.camera")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)

    player = Player.new(400, 300, 50, 50)
    wall = StaticBody.new(600, 300, 30, 200, 0.6, 0.2, 0.2)
    floor = StaticBody.new(400, 580, 800, 40, 0.3, 0.5, 0.3)
    crate = Prop.new(250, 400, 40, 40, "Art/crate.png")

    solids = {floor.body, wall.body, crate.body}

    -- Matches the window's own size for now (800x600) -- a 1:1 "no visual
    -- change yet" viewport, so this first test isolates "does the lerp
    -- follow work" from "does shrinking the viewport look right". Once
    -- that's confirmed, try Camera.new(x, y, 320, 180) (or leave the args
    -- off entirely -- 320x180 is Camera2D's own default) for the actual
    -- pixel-perfect blown-up-pixel look; the level was authored at
    -- 800x600 scale so a much smaller viewport will zoom in hard and clip
    -- most of the scene, which is expected -- that's the camera doing its
    -- job, not a bug.
    local playerX, playerY = player.body:GetPosition()
    camera = Camera.new(playerX, playerY, 800, 600)
    camera:Follow(player.body, 4.0)
end

function Update(deltaTime)
    player:Update(deltaTime, solids)
    crate:Update(deltaTime, solids)

    -- Camera reacts AFTER gameplay has moved this frame, so it's chasing
    -- the freshest player position, then gets pushed to the renderer once
    -- (not once per DrawBody() call -- see SyncCamera()'s comment).
    camera:Update(deltaTime)
    SyncCamera()

    player:Draw()
    wall:Draw()
    crate:Draw()
    floor:Draw()
end