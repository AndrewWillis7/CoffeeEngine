timer = 0.0
position = Vector2.new(400, 300)
velocity = Vector2.new(60, 40)

function Init()
    print("Engine Initialized");
    SetClearColor(0.0, 0.0, 1.0)
end

function Update(deltaTime)
    timer = timer + deltaTime
    local r = (math.sin(timer * 2.0) + 1.0) / 2.0
    SetClearColor(r, 0.2, 0.4)

    position = position + velocity * deltaTime

    if position:GetX() > eWindow:GetWidth() or position:GetX() < 0 then
        velocity = velocity * -1
    end

    DrawDebugQuad(position:GetX(), position:GetY(), 50, 50, timer * 45.0, 1.0, 1.0, 1.0)
end