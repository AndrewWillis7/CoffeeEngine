local Player = require("objects.player")
local StaticBody = require("objects.static_body")
local Prop = require("objects.prop")
local Camera = require("objects.camera")
local ArtObject = require("objects.art_object")
local Campfire = require("objects.campfire")
local Spotlight = require("objects.spotlight")
local Constants = require("core.constants")
local Terrain = require("objects.terrain")

function Init()
    print("Engine Initialized")
    SetClearColor(0.05, 0.05, 0.08)
    Physics.SetGravity(0, 980)
    SetPixelScale(1);

    -- Player is the base unit everything else on this floor is laid out
    -- relative to -- see Constants.PLAYER_WIDTH/HEIGHT's comment. Spawns
    -- above the floor's open left end and falls onto it.
    player = Player.new(118, 90, Constants.PLAYER_WIDTH, Constants.PLAYER_HEIGHT, {
        legging = { width = 4, height = 11, color = {0.36, 0.24, 0.52} },
        knee    = { width = 5, height = 3,  color = {0.22, 0.14, 0.30} },
        boot    = { width = 6, height = 4,  color = {0.10, 0.09, 0.12} },
        stride  = 22,
        stepHeight = 4,
    })

    -- The ground. Replaces the old flat 1000x15 StaticBody floor
    -- entirely: this is a real, noise-generated terrain chunk -- uneven
    -- dirt with per-pixel grass growing out of it, all of it lit by the
    -- same LightingSystem pass everything else goes through, and all of
    -- it collided against as a heightmap rather than a box.
    --
    -- Geometry: 320 wide (exactly the native stage width) x 48 tall,
    -- centered at (160, 156) -- so its top edge is y=132 and its bottom
    -- sits flush on the bottom of the 180-tall stage. surfaceOffset 16
    -- puts the MEAN surface at y=148, and the amplitude below swings it
    -- roughly y=142..154 -- around a third of a player-height of relief,
    -- which is enough to read as real ground rather than a decorated
    -- line, while still being walkable everywhere (maxStepHeight
    -- defaults to 6). That leaves the grass (up to 5 texels tall)
    -- comfortably inside the sprite's own top edge, which is the one
    -- constraint terrain sizing has -- see Terrain.new's comment.
    --
    -- Change `seed` and you get a completely different, equally valid
    -- piece of ground; change nothing and it regenerates identically
    -- across hot-reloads.
    terrain = Terrain.new(160, 156, 320, 48, {
        seed = 1,
        surfaceAmplitude = 9,
        surfaceOffset = 16,
        surfaceFrequency = 0.03,
        grassDensity = 0.85,
    })

        -- Its Y is no longer a hand-computed constant: the ground is uneven
    -- now, so "sitting on the floor" means asking the terrain where its
    -- surface actually is at this X and centering on that. This is the
    -- normal way to place anything on terrain -- see Terrain:SurfaceYAt.
    local wallX, wallHeight = 198, 48
    wall = StaticBody.new(wallX, terrain:SurfaceYAt(wallX) - wallHeight / 2, 16, wallHeight, 0.6, 0.2, 0.2)

    -- Casts a hard shadow -- light_blocking bodies stop a light ray dead
    -- at their first solid pixel instead of letting it pass through (see
    -- LightingSystem.h). Everything else (floor, crate, the player) is
    -- lit-but-not-blocking by default: light passes through them but
    -- still tints whatever solid pixels it touches along the way.
    wall.body:SetLightBlocking(true)

    local campfireX, campfireSize = 178, 16
    campfire = Campfire.new(campfireX, terrain:SurfaceYAt(campfireX) - campfireSize / 2, campfireSize)

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
    spotlight2 = Spotlight.new(-300, 10, 140)
    spotlight2.light:SetColor(1.0, 0.0, 0.0, 1.0)

    -- Background decoration -- no collision, never simulated, just sits
    -- there. crate.png is natively 40x40; SetScale(1.5) draws it at
    -- 60x60 without touching its logical size (GetSize() still reports
    -- 40x40) -- repositioned to sit in-frame on the new 320x180 stage
    -- (it used to be at x=400, entirely off the left edge of this
    -- resolution).
    --background = ArtObject.new(160, 55, 40, 40, "Art/crate.png")
    --background:SetScale(1.5)

    solids = {terrain, wall}

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

    UpdateTerrain(deltaTime)

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
    terrain:Draw()
    wall:Draw()
    player:Draw()
    campfire:Draw()
end