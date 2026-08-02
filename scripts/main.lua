square = nil
glow = nil

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)

    square = RigidBody2D.new(400, 300, 50, 50)
    square:SetColor(1.0, 1.0, 1.0, 1.0)
    square:SetVelocity(60, 40)
    square:SetAngularVelocity(45) -- degrees/sec, same spin rate as before

    glow = Shader.CreateGlow()
    glow:SetVec3("u_GlowColor", 0.3, 0.8, 1.0)
    glow:SetFloat("u_GlowIntensity", 1.4)
    square:SetShader(glow)
end

function Update(deltaTime)
    square:Integrate(deltaTime)

    local x, y = square:GetPosition()
    if x > eWindow:GetWidth() or x < 0 then
        local vx, vy = square:GetVelocity()
        square:SetVelocity(-vx, vy)
    end

    DrawBody(square)
end