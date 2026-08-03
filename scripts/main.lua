local KEY_W = 25
local KEY_A = 38
local KEY_S = 39
local KEY_D = 40

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)

    square = RigidBody2D.new(400, 300, 50, 50)
    square:SetColor(1.0, 1.0, 1.0, 1.0)
    square:SetCollisionShape(CollisionShape2D.NewBox(25, 25))

    playerCfg = PlayerActorConfig.new()
    playerCfg:SetMoveSpeed(250)
    square:SetPlayerConfig(playerCfg)

    -- Something to collide into: a static wall, mass 0 => never moves
    wall = RigidBody2D.new(600, 300, 30, 200)
    wall:SetColor(0.6, 0.2, 0.2, 1.0)
    wall:SetCollisionShape(CollisionShape2D.NewBox(15, 100))
    wall:SetMass(0)

    -- Something to push around: normal mass
    crate = RigidBody2D.new(250, 400, 40, 40)
    crate:SetColor(0.7, 0.6, 0.2, 1.0)
    crate:SetCollisionShape(CollisionShape2D.NewBox(20, 20))
    crate:SetMass(1)
end

function Update(deltaTime)
    local player = Actors.GetPlayer()
    if player then
        local dx, dy = 0, 0
        if Input.IsKeyDown(KEY_W) then dy = dy - 1 end
        if Input.IsKeyDown(KEY_S) then dy = dy + 1 end
        if Input.IsKeyDown(KEY_A) then dx = dx - 1 end
        if Input.IsKeyDown(KEY_D) then dx = dx + 1 end

        local len = math.sqrt(dx * dx + dy * dy)
        if len > 0 then dx, dy = dx / len, dy / len end

        local speed = player:GetPlayerConfig():GetMoveSpeed()
        player:SetVelocity(dx * speed, dy * speed)
        player:Integrate(deltaTime)

        player:ResolveCollisionWith(wall)
        player:ResolveCollisionWith(crate)
    end

    DrawBody(square)
    DrawBody(wall)
    DrawBody(crate)
end