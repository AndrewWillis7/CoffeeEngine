# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

CoffeeEngine is a from-scratch C++20 2D game engine (custom OS window/GL context layer, no SDL/GLFW) with a Lua 5.4 scripting layer on top. Gameplay (the current test scene: player, terrain, IK-animated legs, lighting, camera) is written entirely in Lua under `scripts/`; the C++ side (`src/`) is the engine: windowing, an OpenGL 2D renderer, physics/collision, a lighting system, a heightmap terrain system, and the Lua bindings that expose all of it.

## Build

Windows builds happen from an **MSYS2 UCRT64 shell** (VS Code's default build task shells out to `C:/msys64/usr/bin/make.exe` with `C:/msys64/ucrt64/bin` on PATH — see `.vscode/tasks.json`).

```sh
make                # builds ./engine_test.exe (Windows) or ./engine_test (Linux)
make clean           # removes build/ and the binary
./engine_test.exe    # run it (must be launched with the repo root as CWD -- it loads scripts/main.lua and scripts/keycodes.lua by relative path)
```

`PLATFORM` auto-detects from `$(OS)` but can be forced: `make PLATFORM=linux`. There is no test suite and no lint target — this is a from-scratch engine project, correctness is checked by running the scene.

### Static Lua library

The build fails immediately with a clear error if `src/Core/lua/win64/liblua.a` (Windows) or `src/Core/lua/liblua.a` (Linux) is missing — these are prebuilt and checked in, not built by `make`. If one needs regenerating:

- Windows: `src/OS_/Tools/winlua.sh`, run from an MSYS2 UCRT64 shell (or `winlua.bat`). Can also cross-compile from Linux with `CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar`.
- Linux: `src/OS_/Tools/build_lua.sh`.

Both build Lua 5.4.6 from source; `src/Core/lua/include` headers are shared between platforms and must stay at that same Lua version.

## Architecture

### C++ engine / Lua game split

`main.cpp` owns the whole subsystem lifecycle in one fixed order (window → graphics context → `Renderer2D` → `ActorRegistry` → `UserInputService` → `LightingSystem`/`TerrainSystem`), bundles raw pointers to all of it into an `EngineContext` (`src/Core/EngineContext.h`), and hands that to `ScriptEngine::Init("scripts/main.lua", engineContext)`. From there, all actual gameplay logic lives in Lua — the C++ main loop each frame just calls `scriptEngine.Update(deltaTime)`, which calls Lua's global `Update(deltaTime)` function if `scripts/main.lua` (or whatever it `require`s) defines one. `Init()`/`Update(dt)` are the two lifecycle hooks the engine looks for by name in the loaded script (see `ScriptEngine.cpp`'s `CallIfExists`).

`ActorRegistry` (`src/Core/ActorRegistry.*`) is the single owner of every engine-side object Lua can create at runtime — `RigidBody2D`, `Shader`, `CollisionShape2D`, `PlayerActorConfig`, `Camera2D`, `LightEmitterConfig`, `TerrainChunk`, `PixelSprite`. Lua never owns engine memory directly; it holds pointers (userdata) into these pools. `ActorRegistry::Clear()` wipes the Lua-ephemeral pools (bodies, shapes, configs, cameras, terrain chunks, generated sprites) on hot-reload but deliberately leaves expensive-to-decode assets alone (`m_NamedShaders`, `m_PixelSprites` loaded from disk, the border sprite pointer) so a reload doesn't force re-decoding PNGs or recompiling shaders.

### Lua bindings (`src/Core/Scripting/`)

`LuaBinding.h` is a small template layer that turns "bind this C++ member function/field" into one line, instead of hand-written `lua_push*`/`luaL_check*` glue per binding. Two binding shapes:
- `LuaBinding::Class<T>(L, metatableName)...Method<&T::Foo>("Foo")...Finish()` — binds a metatable for a pointer- or value-owned type (`Property`/`Vec2Property`/`PtrProperty`/`ScaledProperty` cover common field-accessor shapes; `Raw` is the escape hatch for anything irregular).
- `LuaBinding::Table(L)...Function<&T::Foo>("Foo", ctxPtr)...Finish("GlobalName")` — binds a global table whose functions close over a captured engine object (e.g. `Actors`, `Input`, `Physics`, or a type's own `.new` factory closing over `ActorRegistry*`).

Every bound pointer type needs a `MetatableOf<T>` specialization (`Coffee.<TypeName>` string) next to its `Register()` function; value types (e.g. `Vector2`) additionally opt into `IsValueType<T>`. `ScriptBindings::RegisterAll()` (`ScriptBindings.cpp`, organized into clearly labeled `// ===` sections, one per bound type/table) is the single call site — add new engine-facing API there, not scattered across headers.

The `IK` table (near the bottom of `ScriptBindings.cpp`) is the odd one out: pure stateless numeric kernels (`IK.SolveTwoBone`, `IK.GaitPose`, `IK.SolveLegFrame`, ...) with no metatable/ownership, written to be fused, high-frequency math the leg IK solver in `scripts/objects/leg_rig.lua` calls every frame. `scripts/api/IK.txt` documents its argument/return contract in detail — read it before touching either side of that boundary.

### Lua-side game code (`scripts/`)

- `scripts/main.lua` — the scene: defines global `Init()`/`Update(dt)`, wires up the player, terrain, lights, camera, and static bodies for the current test scene.
- `scripts/core/Class.lua` — a minimal hand-rolled single-inheritance class helper (`local Foo = Class(Base)`, `setmetatable(self, Foo)`) that every game object module uses instead of a third-party OOP library. Read its header comment for the two usage patterns (with/without inheritance).
- `scripts/core/constants.lua` — shared tunables (native resolution, player size, etc.) referenced across object modules.
- `scripts/objects/*.lua` — one module per game object type (`player`, `camera`, `terrain`, `campfire`, `spotlight`, `static_body`, `prop`, `art_object`), each built on `Class` and the engine bindings above. `leg_rig.lua` is the procedural two-bone IK leg animator driving the player's legs; it's the primary consumer of the `IK.*` bindings and is under active iteration (see git history / `scripts/api/IK.txt`).
- `scripts/shaders/*.frag` + `quad.vert` — the engine's shader library; every fragment shader pairs with the same shared vertex shader. Swappable at runtime via `Actors.LoadShaderFromFile("Border", "path/to.frag")`.
- `scripts/api/coffee_api.lua` — reference/annotation stub of the full Lua API surface exposed by the engine (used by the Lua language server via `.luarc.json`'s `workspace.library`), not code that runs.
- `ScriptEngine::Init` prepends `scripts/?.lua;scripts/?/init.lua` to Lua's `package.path`, so `require("objects.player")` resolves to `scripts/objects/player.lua` — all game modules are required relative to `scripts/`, not the repo root.

### Scene editor and `.scene` files (`src/Core/Engine/`)

The debug menu's edit mode (backtick, then F2) moves, rotates, scales and re-shapes objects and edits their constants, and those edits **persist**: `SceneOverrides` saves them to `<entry script>.scene` (`scripts/main.scene`) and re-applies them after every script `Init()` â€” startup, F5 reload, every run â€” via `ScriptEngine::SetOnLoaded`. So an object's live value is the script's value *unless* that file overrides it; check the file before assuming `main.lua` is the whole story. Objects are keyed by `SetName` name plus creation order among same-named bodies (`[Wall#0]`), so reordering creation in the scripts can retarget an edit.

- `SceneFields.cpp` is the single table of editable fields (key, type, get/set, inspector hints). Adding an entry makes a field editable, undoable and persisted â€” nothing else needs touching.
- `SceneEditor` records edits by snapshotting a body's fields before and after its own code runs (gizmo drag, inspector widget, button) and diffing, so new tools are persisted for free. Freecam is `Renderer2D::SetViewOverride`, which never touches the game's camera body.
- Lua constants reach the inspector through `body:Expose(name, table, key, opts)` (a `ScriptTunable` on the body; saved as `script.<name>`, re-applied after `Init()`), so gameplay tuning stays in Lua. Give `opts.onChange` for anything baked at construction â€” `LegRig:ExposeTunables` rebuilds its canvases that way.
- `body:SetPartOf(owner)` marks a body drawn for another (leg/coat canvases): clicking it selects the owner, and its transform shows locked. Any new sub-body a script positions every frame should declare it.

### Coordinate/units conventions worth knowing

- +Y is down (see `Vector2.h`).
- World units are "texels" (pixel-art texels), distinct from real window pixels — `Constants.RESOLUTION_WIDTH/HEIGHT` is the native pixel-art resolution; the camera/renderer scale that up to fill the real window, letterboxed/pillarboxed to a fixed aspect ratio.
- Terrain is a per-chunk heightmap, not a box collider — placing anything on the ground means querying `Terrain:SurfaceYAt(x)`, not a hardcoded Y.

## Platform notes

- `.gitignore` excludes `build/`, `*.o`/`*.d`, and the `engine_test`/`engine_test.exe` binaries — these are build artifacts, not committed sources, even though a stale `engine_test.exe` may exist locally.
- Windows-only and Linux-only sources are filtered by the Makefile (`WINDOWS_ONLY_SRC`/`LINUX_ONLY_SRC` in `src/OS_/`) rather than `#ifdef`'d in shared files — when adding a new OS-specific window/graphics-context implementation, register it there.
