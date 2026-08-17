local Player = require("objects.player")
local StaticBody = require("objects.static_body")
local Prop = require("objects.prop")
local Camera = require("objects.camera")
local ArtObject = require("objects.art_object")
local Constants = require("core.constants")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)

    player = Player.new(400, 300, 50, 50)
    wall = StaticBody.new(600, 300, 30, 200, 0.6, 0.2, 0.2)
    floor = StaticBody.new(400, 580, 800, 40, 0.3, 0.5, 0.3)
    crate = Prop.new(250, 400, 40, 40, "Art/crate.png")

    -- Background decoration -- no collision, never simulated, just sits
    -- there. crate.png is natively 40x40; SetScale(3) draws it at 120x120
    -- without touching its logical size (GetSize() still reports 40x40).
    background = ArtObject.new(400, 150, 40, 40, "Art/crate.png")
    background:SetScale(3)

    solids = {floor.body, wall.body, crate.body}

    -- Native pixel-art resolution + aspect, see core.constants -- the
    -- "resolution control" knob; every world pixel draws
    -- (real window width / RESOLUTION_WIDTH) real screen pixels wide,
    -- letterboxed/pillarboxed to stay exactly 16:9 regardless of the
    -- real window's own aspect.
    --
    -- Press F11 to toggle real OS fullscreen -- stays correctly
    -- letterboxed at any window size, never stretches.
    local playerX, playerY = player.body:GetPosition()
    camera = Camera.new(playerX, playerY, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)
    camera:Follow(player.body, 4.0)
    camera.camera:SetTargetAspect(Constants.ASPECT_WIDTH, Constants.ASPECT_HEIGHT)

    -- Frame the camera a bit ABOVE the player instead of dead-centered
    -- on them -- more headroom to see what's coming (platforms, enemies,
    -- the crate above), less wasted space below. Negative Y is up (see
    -- Vector2.h's "+y is down" convention). ~11% of the native vertical
    -- resolution reads as a gentle, not-too-aggressive offset.
    camera.camera:SetFocusOffset(0, -40)

    -- The border (drawn behind everything, filling whatever the fit
    -- above doesn't cover) defaults to a black night sky with sparse
    -- stars up top and dark grey smoke clouds drifting near the bottom
    -- -- nothing to do if that's all you want. To swap it for something
    -- else, write a .frag file (paired with the engine's shared vertex
    -- shader -- see scripts/shaders/*.frag for the uniform/varying
    -- interface each one has to work with) and load it by name:
    --
    --   Actors.LoadShaderFromFile("Border", "scripts/shaders/border_plain.frag")
    --   Actors.GetNamedShader("Border"):SetVec3("u_PlainColor", 0.6, 0.05, 0.05)
    --
    -- For a textured border (tiled pixel art, a photo, whatever), attach
    -- a sprite too -- SetBorderSprite works alongside any shader that
    -- declares `uniform sampler2D u_Texture`:
    --
    --   Actors.SetBorderSprite(Sprite.Load("Art/crate.png"))
    --   Actors.LoadShaderFromFile("Border", "scripts/shaders/border_tiled_sprite.frag")
    --   Actors.GetNamedShader("Border"):SetFloat("u_TileSize", 64.0)
end

function Update(deltaTime)
    if Input.IsKeyPressed(Keys.F11) then
        eWindow:SetFullscreen(not eWindow:IsFullscreen())
    end

    player:Update(deltaTime, solids)
    crate:Update(deltaTime, solids)

    -- Camera reacts AFTER gameplay has moved this frame, so it's chasing
    -- the freshest player position, then gets pushed to the renderer once
    -- (not once per DrawBody() call -- see SyncCamera()'s comment).
    camera:Update(deltaTime)
    SyncCamera()

    background:Draw()
    player:Draw()
    wall:Draw()
    crate:Draw()
    floor:Draw()
end