local Player = require("objects.player")
local StaticBody = require("objects.static_body")
local Prop = require("objects.prop")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)

    player = Player.new(400, 300, 50, 50)
    wall = StaticBody.new(600, 300, 30, 200, 0.6, 0.2, 0.2)
    floor = StaticBody.new(400, 580, 800, 40, 0.3, 0.5, 0.3)
    crate = Prop.new(250, 400, 40, 40, "Art/crate.png")

    solids = {floor.body, wall.body, crate.body}
end

function Update(deltaTime)
    player:Update(deltaTime, solids)
    crate:Update(deltaTime, solids)

    player:Draw()
    wall:Draw()
    crate:Draw()
    floor:Draw()
end