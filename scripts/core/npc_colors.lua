-- A second color palette, same shape as core/player_colors.lua, used to
-- demo/prove that CharacterFactory.CreateNPC can recolor a character
-- without touching its shape (legConfig/torsoConfig stay whatever's
-- passed in, or the shared defaults) -- see main.lua's CreateNPC call.
return {
    skin       = {0.92, 0.85, 0.75, 1.0},
    undershirt = {0.22, 0.45, 0.28, 1.0},  -- green
    overshirt  = {0.30, 0.30, 0.32, 1.0},  -- slate-grey cloak
    legging    = {0.35, 0.28, 0.20, 1.0},  -- brown
    knee       = {0.35, 0.28, 0.20, 1.0},
    boot       = {0.10, 0.08, 0.08, 1.0},
    hip        = nil,
}
