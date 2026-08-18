local Player = require("objects.player")
local StaticBody = require("objects.static_body")
local Prop = require("objects.prop")
local Camera = require("objects.camera")
local ArtObject = require("objects.art_object")
local Campfire = require("objects.campfire")
local Spotlight = require("objects.spotlight")
local Constants = require("core.constants")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)
    SetPixelScale(1);

    -- Player is the base unit everything else on this floor is laid out
    -- relative to -- see Constants.PLAYER_WIDTH/HEIGHT's comment. Spawns
    -- above the floor's open left end and falls onto it.
    player = Player.new(118, 90, Constants.PLAYER_WIDTH, Constants.PLAYER_HEIGHT)

    -- Floor is 100x15 texels -- NOT meant to span the whole native
    -- viewport (320 wide); it's a single starting platform with open
    -- world beyond both ends, the same way a real level's first screen
    -- would be. Centered on the stage; everything below sits ON it (see
    -- the comments at each object for how their Y was derived from this
    -- floor's own top edge, floor_top = 163 - 15/2 = 155.5).
    floor = StaticBody.new(160, 163, 100, 15, 0.3, 0.5, 0.3)

    -- 16x48 -- one player-width wide, one and a half player-heights
    -- tall, so it visibly reads as taller than the player rather than
    -- just a same-size box with a different color. Sits at the floor's
    -- right end: center_y = floor_top - 48/2 = 131.5.
    wall = StaticBody.new(198, 131.5, 16, 48, 0.6, 0.2, 0.2)

    -- crate.png is a real 40x40 PNG asset -- RigidBody2D::SetSprite
    -- forces body.size to the SPRITE's native pixel dimensions the
    -- moment it's attached (see ScriptBindings.cpp), so passing smaller
    -- w/h here wouldn't actually shrink it, and scaling it down visually
    -- with SetScale (like `background` below does) would just make a
    -- smoothly-minified miniature of a 40x40 image rather than genuine
    -- chunky pixel art on the SAME texel grid as the 16x32 player --
    -- it'd stop reading as "on the grid" the moment it's smaller than
    -- native. Left at its real size and just repositioned for now
    -- (floor_top - 40/2 = 135.5) -- a deliberately oversized crate is a
    -- reasonable art choice, but if you want it grid-consistent instead,
    -- that means re-exporting crate.png itself at ~16x16, not scaling
    -- the existing file. Happy to help with that either way.
    --crate = Prop.new(146, 135.5, 40, 40, "Art/crate.png")

    -- Casts a hard shadow -- light_blocking bodies stop a light ray dead
    -- at their first solid pixel instead of letting it pass through (see
    -- LightingSystem.h). Everything else (floor, crate, the player) is
    -- lit-but-not-blocking by default: light passes through them but
    -- still tints whatever solid pixels it touches along the way.
    wall.body:SetLightBlocking(true)

    -- Sits in the open gap between the crate and the wall (floor_top -
    -- 16/2 = 147.5, its own default 16x16 size matches PLAYER_WIDTH
    -- exactly). Walk the player toward it from the spawn point and
    -- watch the floor/wall/crate/player's own pixels nearest it warm up
    -- to orange, live, every frame (nothing here is baked) -- see
    -- Campfire.lua for why its radius is 60, not the old 140.
    campfire = Campfire.new(178, 147.5)

    -- A second, cooler light source up in the top-right corner of the
    -- 320x180 stage, aimed down-left across the whole floor -- see
    -- Spotlight.lua. Its radius (300) is deliberately much bigger than
    -- the campfire's (60): the two lights are meant to OVERLAP everywhere
    -- the player can stand, not take turns. Walk left, away from the
    -- fire, and the mix (see PixelSprite::AccumulateLightTint) shifts
    -- from campfire-orange toward this light's violet as the campfire's
    -- short-range contribution fades out while this one's long, gentle
    -- falloff barely changes -- stand between them and both colors mix
    -- into the same pixels at once, rather than one replacing the other.
    -- 140 degrees points from this corner down toward the floor/campfire
    -- area (0 = +X/right, 90 = +Y/down -- see Spotlight.lua's comment).
    spotlight = Spotlight.new(300, 10, 140)

    -- Background decoration -- no collision, never simulated, just sits
    -- there. crate.png is natively 40x40; SetScale(1.5) draws it at
    -- 60x60 without touching its logical size (GetSize() still reports
    -- 40x40) -- repositioned to sit in-frame on the new 320x180 stage
    -- (it used to be at x=400, entirely off the left edge of this
    -- resolution).
    --background = ArtObject.new(160, 55, 40, 40, "Art/crate.png")
    --background:SetScale(1.5)

    solids = {floor.body, wall.body}

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
    -- resolution reads as a gentle, not-too-aggressive offset -- was -40
    -- against the old 360-tall viewport, rescaled to -20 against the
    -- new 180-tall one to keep that same proportion.
    camera.camera:SetFocusOffset(0, -20)

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

    -- Constants.RESOLUTION_WIDTH/HEIGHT doubles as the play area's
    -- bounds here (texels, not real window pixels -- see Player:Update's
    -- comment) because this level fits entirely within one camera
    -- frame. A level that scrolls beyond what the camera shows at once
    -- would need its own, separate level-bounds concept instead of
    -- reusing the camera's native resolution for this.
    player:Update(deltaTime, solids, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)

    -- Camera reacts AFTER gameplay has moved this frame, so it's chasing
    -- the freshest player position, then gets pushed to the renderer once
    -- (not once per DrawBody() call -- see SyncCamera()'s comment).
    camera:Update(deltaTime)
    SyncCamera()

    -- Recomputes every light's per-pixel tint fresh THIS frame -- never
    -- baked, so a light that moved (or the player walking past one)
    -- shows up immediately. Runs once a frame, same convention as
    -- SyncCamera() -- see UpdateLighting()'s comment in ScriptBindings.cpp.
    UpdateLighting(deltaTime)

    -- Environment first, player on top of it -- with the crate/floor/
    -- wall now sized close enough to the player to actually sit flush
    -- against it (see their spawn positions above), drawing the player
    -- BEFORE them meant an adjacent crate could paint right over it.
    -- Campfire drawn last so its glow reads as sitting in front of
    -- whoever's standing next to it.
    floor:Draw()
    wall:Draw()
    player:Draw()
    campfire:Draw()
end