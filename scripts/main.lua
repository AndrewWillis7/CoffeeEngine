local Player = require("objects.Player")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)

    -- Engine-wide gravity (px/s^2), C++-applied to every non-static
    -- (mass > 0) body's Integrate() call. Defaults to zero if never set --
    -- this is what opts the level into being a sidescroller.
    Physics.SetGravity(0, 980)

    player = Player.new(400, 300, 50, 50)

    wall = RigidBody2D.new(600, 300, 30, 200)
    wall:SetColor(0.6, 0.2, 0.2, 1.0)
    wall:SetCollisionShape(CollisionShape2D.NewBox(15, 100))
    wall:SetMass(0)

    crate = RigidBody2D.new(250, 400, 40, 40)
    crateSprite = Sprite.Load("Art/crate.png")
    crate:SetSprite(crateSprite)
    crate:SetCollisionShape(CollisionShape2D.NewBox(20, 20))
    crate:SetMass(1)

    -- Floor for the player (and crate) to land on -- mass 0, same
    -- "immovable" convention the wall above already uses. Sized to span
    -- the window width and sit right at its bottom edge (800x600 window).
    floor = RigidBody2D.new(400, 580, 800, 40)
    floor:SetColor(0.3, 0.5, 0.3, 1.0)
    floor:SetCollisionShape(CollisionShape2D.NewBox(400, 20))
    floor:SetMass(0)

    -- What the player resolves collisions against each frame. Plain array
    -- so adding a new piece of level geometry is a one-line change here,
    -- not a new player:ResolveCollisionWith(...) call in Update() below.
    solids = {floor, wall, crate}
end

function Update(deltaTime)
    player:Update(deltaTime, solids)

    -- Crate is also mass > 0, so it falls under the same C++-driven
    -- gravity as the player -- this is what "gravity applies to all
    -- non-static physics objects" means in practice: any body whose
    -- Integrate() gets called feels it, no per-body opt-in code needed.
    crate:Integrate(deltaTime)
    crate:ResolveWindowBounds(eWindow:GetWidth(), eWindow:GetHeight())
    crate:ResolveCollisionWith(floor)
    crate:ResolveCollisionWith(wall)

    player:Draw()
    DrawBody(wall)
    DrawBody(crate)
    DrawBody(floor)
end