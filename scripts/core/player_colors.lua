-- The player's default palette, kept separate from CharacterFactory's shape and
-- tuning tables so recoloring a character is a one-file edit. A plain Lua table
-- rather than JSON: the engine binds no JSON decoder, and every other shared
-- config here is already a required table.
--
-- Any key left nil falls back sensibly (see CharacterFactory.applyPalette).
-- Pass a different table of the same shape to CreatePlayer/CreateNPC to reuse
-- this body shape with another look -- core/npc_colors.lua is an example.
return {
    skin       = {1.00, 1.00, 1.00, 1.0},  -- torso/collar base color
    undershirt = {0.75, 0.20, 0.25, 1.0},  -- shirt
    overshirt  = {0.36, 0.22, 0.12, 1.0},  -- cloak
    legging    = {0.30, 0.33, 0.50, 1.0},  -- thigh
    knee       = {0.30, 0.33, 0.50, 1.0},
    boot       = {0.14, 0.12, 0.16, 1.0},  -- shin/foot
    hip        = nil,                      -- nil = matches legging
}
