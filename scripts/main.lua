timer = 0.0

function Init()
    print("Engine Initialized");
    SetClearColor(0.0, 0.0, 1.0)
end

function Update(deltaTime)
    timer = timer + deltaTime
    local r = (math.sin(timer * 2.0) + 1.0) / 2.0
    SetClearColor(r, 0.2, 0.4)
end