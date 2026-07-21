timer = 0.0

function Init()
    print("LuaBridge3: Engine Initialized Successfully!")
    print("Window Width from C++: " .. engineWindow:GetWidth())
end 

function Update(deltaTime)
    timer = timer + deltaTime
    local r = (math.sin(timer * 2.0) + 1.0) / 2.0

    SetClearColor(r, 0.2, 0.4)
end