local CharacterFactory = require("core.character_factory")
local StaticBody = require("objects.static_body")
local Camera = require("objects.camera")
local Campfire = require("objects.campfire")
local Spotlight = require("objects.spotlight")
local Constants = require("core.constants")
local Terrain = require("objects.terrain")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)
    SetPixelScale(1)

    -- The player is the unit everything else here is laid out against. Spawns
    -- above the floor's open left end and falls onto it. Leg shape and gait come
    -- from CharacterFactory.DEFAULT_LEG_CONFIG; colors come from
    -- core/player_colors.lua, applied over whatever shape config is passed in.
    -- This scene overrides the torso config only to demo the overshirt.
    player = CharacterFactory.CreatePlayer(118, 90, Constants.PLAYER_WIDTH, Constants.PLAYER_HEIGHT, nil, {
        undershirt = { enabled = true, neckline = 0.28, hem = 1.0, width = 1.0 },
        overshirt  = { enabled = false },
    })

    -- Same body shape, different palette -- recoloring a character is just
    -- another palette table. Created AFTER the player, because Actors.GetPlayer()
    -- resolves to the first body carrying a PlayerActorConfig.
    npc = CharacterFactory.CreateNPC(140, 90, Constants.PLAYER_WIDTH, Constants.PLAYER_HEIGHT,
        nil, { undershirt = { enabled = true }, overshirt = { enabled = true } },
        require("core.npc_colors"))

    -- The ground: a noise-generated chunk of uneven dirt with per-pixel grass,
    -- lit by the same pass as everything else and collided as a heightmap.
    -- 320 wide (the native stage width) x 48 tall, centred so its bottom sits
    -- flush with the stage. The surface swings roughly y=142..154, about a third
    -- of a player-height of relief -- enough to read as ground while staying
    -- walkable everywhere (maxStepHeight defaults to 6). Change `seed` for
    -- completely different but equally valid ground; leave it and the chunk
    -- regenerates identically across hot-reloads.
    terrain = Terrain.new(160, 156, 320, 48, {
        seed = 76,
        surfaceAmplitude = 5,
        surfaceOffset = 28,
        surfaceFrequency = 0.02,
        grassDensity = 0.75,
        grassMaxHeight = 7
    })

    -- Placing anything on uneven ground means asking the terrain where its
    -- surface actually is at that X, never a hand-computed constant.
    local wallX, wallHeight = 198, 48
    wall = StaticBody.new(wallX, terrain:SurfaceYAt(wallX) - wallHeight / 2, 16, wallHeight, 0.6, 0.2, 0.2)

    -- Casts a hard shadow: a blocking body stops a light ray at its first solid
    -- pixel. Everything else is lit but not blocking -- light passes through and
    -- still tints the solid pixels it touches on the way.
    wall.body:SetLightBlocking(true)
    wall.body:SetName("Wall")

    local campfireX, campfireSize = 178, 16
    campfire = Campfire.new(campfireX, terrain:SurfaceYAt(campfireX) - campfireSize / 2, campfireSize)

    -- Cooler, much wider light (radius 300 against the campfire's 60) so the two
    -- OVERLAP everywhere the player can stand rather than taking turns. Walking
    -- away from the fire shifts the mix from orange toward violet as the
    -- campfire's short-range contribution drops and this one's barely changes.
    -- 140 degrees aims from the corner down across the floor (0 = right, 90 = down).
    spotlight = Spotlight.new(300, 10, 140)
    spotlight2 = Spotlight.new(-300, 10, 140)
    spotlight2.light:SetColor(1.0, 0.0, 0.0, 1.0)

    -- Names are a debug-menu nicety only: the explorer lists bodies by them
    -- instead of by index. Nothing in the engine reads them back.
    spotlight.body:SetName("Spotlight (violet)")
    spotlight2.body:SetName("Spotlight (red)")

    solids = {terrain, wall}

    -- Native pixel-art resolution and aspect (see core.constants): every world
    -- pixel draws (window width / RESOLUTION_WIDTH) screen pixels wide,
    -- letterboxed to stay 16:9 whatever the window's own aspect is. F11 toggles
    -- OS fullscreen and stays correctly letterboxed, never stretched.
    local playerX, playerY = player.body:GetPosition()
    camera = Camera.new(playerX, playerY, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)
    camera:Follow(player.body, 4.0)
    camera.camera:SetTargetAspect(Constants.ASPECT_WIDTH, Constants.ASPECT_HEIGHT)

    -- Framed above the player for headroom to see what's coming. Negative Y is
    -- up; ~11% of the native vertical resolution is a gentle offset.
    camera.camera:SetFocusOffset(0, -20)

    -- The border filling whatever the fit doesn't cover defaults to a night sky
    -- with stars and drifting smoke. To replace it, write a .frag against the
    -- shared vertex stage and load it by name:
    --
    --   Actors.LoadShaderFromFile("Border", "scripts/shaders/border_plain.frag")
    --   Actors.GetNamedShader("Border"):SetVec3("u_PlainColor", 0.6, 0.05, 0.05)
    --
    -- Attach a sprite too for a textured border, with any shader declaring
    -- `uniform sampler2D u_Texture`:
    --
    --   Actors.SetBorderSprite(Sprite.Load("Art/crate.png"))
end

function Update(deltaTime)
    if Input.IsKeyPressed(Keys.F11) then
        eWindow:SetFullscreen(not eWindow:IsFullscreen())
    end

    -- The native resolution doubles as the play area's bounds (in texels) because
    -- this level fits in one camera frame. A scrolling level would need its own
    -- level-bounds concept instead of reusing the camera's resolution.
    player:Update(deltaTime, solids, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)
    npc:Update(deltaTime, solids, Constants.RESOLUTION_WIDTH, Constants.RESOLUTION_HEIGHT)

    -- Camera reacts AFTER gameplay has moved, so it chases this frame's position,
    -- then is pushed to the renderer once rather than once per DrawBody().
    camera:Update(deltaTime)
    SyncCamera()

    -- Terrain before lighting: the grass pixels it writes must be lit this frame
    -- rather than next. Lighting is recomputed fresh every frame, never baked.
    UpdateTerrain(deltaTime)
    UpdateLighting(deltaTime)

    -- Environment first, characters on top: scenery sized close enough to sit
    -- flush against the player would otherwise paint over it. Campfire last, so
    -- its glow reads as being in front of whoever stands next to it.
    terrain:Draw()
    wall:Draw()
    player:Draw()
    npc:Draw()
    campfire:Draw()
end
