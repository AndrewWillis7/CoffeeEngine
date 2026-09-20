-- Default color palette for the player -- skin/clothing/leg colors kept
-- in one small, easy-to-edit table, separate from CharacterFactory's
-- shape/tuning tables (DEFAULT_LEG_CONFIG/DEFAULT_TORSO_CONFIG), so
-- recoloring a character is a one-file edit instead of hunting through
-- leg-module and torso-clothing config.
--
-- This is a plain Lua table rather than a .json file: the engine has no
-- JSON decoder bound into Lua (see ScriptBindings.cpp), and every other
-- shared config in this codebase -- Constants, DEFAULT_LEG_CONFIG -- is
-- already a required Lua table, so this stays consistent with that and
-- needs no new file-reading/parsing plumbing. If a real JSON pipeline
-- ever gets added, this file's shape is what a decoded player.json
-- would need to match.
--
-- Any key left nil falls back sensibly (see CharacterFactory.applyPalette
-- in core/character_factory.lua): knee falls back to legging's color,
-- hip falls back to legging's color, skin/undershirt/overshirt/boot fall
-- back to whatever DEFAULT_TORSO_CONFIG/DEFAULT_LEG_CONFIG already had.
--
-- Pass a DIFFERENT palette table (same shape) as the `palette` argument
-- to CharacterFactory.CreatePlayer/CreateNPC to reuse this exact body
-- shape with a different look -- see core/npc_colors.lua for an example.
return {
    skin       = {1.00, 1.00, 1.00, 1.0},  -- torso/collar base color
    undershirt = {0.75, 0.20, 0.25, 1.0},  -- shirt
    overshirt  = {0.36, 0.22, 0.12, 1.0},  -- cloak
    legging    = {0.30, 0.33, 0.50, 1.0},  -- thigh
    knee       = {0.30, 0.33, 0.50, 1.0},
    boot       = {0.14, 0.12, 0.16, 1.0},  -- shin/foot
    hip        = nil,                      -- nil = matches legging
}
