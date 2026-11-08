# ENGAGE

Notes for building a touchscreen mod for **Fire Emblem Engage**. Everything here was learned on the Three Houses mod in this
repo (`claude_touchmod`). It is written for whoever (or whichever Claude) starts the Engage mod.

> **The engines are different.** Three Houses runs on Koei Tecmo's in-house engine. Engage was built by Intelligent Systems in
> **Unity, compiled with IL2CPP**. No address, vtable, struct offset or object layout from Three Houses carries over. What
> carries over is the mod skeleton, the build, the debug channel, the testing workflow and the input design ("instant
> navigation", double-tap confirm, replayed taps). IL2CPP also makes reverse engineering much *easier* than it was here
> (see "Engine: Unity / IL2CPP").
>
> Statements marked *(verify)* are from general knowledge, not from work on Engage. Check them before relying on them.

---

## 1. What carries over unchanged

### Mod skeleton (a bare `subsdk9`, no framework)
- The mod is one freestanding AArch64 module, `exefs/subsdk9`, loaded by the game's own rtld. There is no libnx, no exlaunch and
  no skyline. Files: `src/start.S` (MOD0 header), `src/link.ld` (3 PT_LOADs + dynamic), `src/elf2nso.py` (ELF -> NSO).
- **Input hook by symbol interposition:** the module *defines* `nn::hid::GetNpadState(NpadFullKeyState*, const u32&)` (and the
  Handheld / JoyDual variants). rtld binds the game's imports to our definitions. Each one calls the real *plural*
  `GetNpadStates(..., 1, id)` through the GOT (`IMPORT` macro in `main.c`), runs `process()` and ORs injected buttons into the
  state.
  - **First check which hid functions Engage actually imports** (dump main's dynamic symbol table). Unity's input layer may
    call `GetNpadStates` (plural), a different style set or `nn::hid::GetNpadStyleSet` first. Hook whatever it really calls.
- **Touch input:** `nn::hid::InitializeTouchScreen` + `GetTouchScreenStates<1>` from inside the same hook. Coordinates arrive
  in **1280x720** whatever the render resolution. Keep all tap logic in 720p space.
- **Finding main:** `find_main(lr)` takes the hook's return address and calls `svcQueryMemory`. The region base is main's .text
  base. All game addresses are written as `g_main + offset`.
- Release vs debug: `-DTM_DEBUG` compiles in the debug channel (`dbg.h`), the per-poll perf counters and the `[touchmod]` log
  lines. Release builds carry none of it.

### Build (Windows, no devkitPro)
- `tools/build_local.sh [debug]` uses the pip package **`ziglang`** (`python -m ziglang clang` / `ld.lld`). Flags that matter:
  `--target=aarch64-none-elf -ffreestanding -nostdlib -fPIC -fno-stack-protector -mno-outline-atomics -ffixed-x18 -fno-plt`,
  linked `-shared -z now --unresolved-symbols=ignore-all -soname subsdk9`.
- **Install:** `%APPDATA%\Ryujinx\mods\contents\<TitleID>\<ModName>\exefs\subsdk9` (+ `main.npdm` if patched). Engage's
  title ID is `0100A6301214E000` *(verify)*.
- **Every new build needs a real emulation restart**: Stop Emulation, then start the game again. A soft reset keeps the old
  module loaded. Before restarting, write a harmless command (`info`) into the debug channel, otherwise a stale `call` in
  `cmd.txt` runs on the next boot.
- Check that `subsdk9` isn't already taken by another loader. Engage modders commonly use skyline/Cobalt *(verify)*, which may
  occupy `subsdk9`; if so, pick a free `subsdkN`.

### `main.npdm` (permissions)
- `tools/mknpdm.py IN OUT` patches the game's own npdm, in both the ACID and the ACI0 blocks:
  - the **SdCard filesystem bit (21)**, needed for the debug channel and the settings file;
  - **SVCs 0x73/0x74/0x75/0x77/0x78**, needed to write hooks into the game's code;
  - **SVCs 0x40/0x43**, needed to obtain a real handle to our own process.
- Code patching (`src/patch.h`):
  1. Map the code pages RW at a free VA with `svcMapProcessMemory` on our own process.
  2. Write the instructions.
  3. `dc cvau` / `ic ivau`, so that Ryujinx drops its recompiled block.
  4. Unmap.
  The pseudo-handle `0xFFFF8001` is refused by those SVCs. A real handle comes from sending it to ourselves in an IPC message
  (CreateSession + ReplyAndReceive on a helper thread), the way libnx and exlaunch do it.
- If Engage modding already uses skyline/Cobalt, it may be simpler to use its hooking. The bare approach above works and has no
  dependencies.

### Debug channel (the most valuable thing here; port it first)
- **How it works:** the debug build polls `sd:/touchmod/cmd.txt` every 10 input polls (sequence number on line 1, then commands
  separated by `;` or newlines). It writes the results to `out.txt`, headed `seq N`. `tools/cmd.sh "<cmds>" [timeout]` writes a
  command and waits for the matching `seq`.
- **Commands worth porting as-is (`src/dbg.h`, `dbg_ext` in `main.c`):**

  | Command | What it does |
  |---|---|
  | `base` | Key roots: the game objects found from main's static pointers. |
  | `dump a n` | Hex and float dump of memory. |
  | `w8` / `w32` | Write memory. |
  | `call f a0..a3` | Call a game function; f below 0x10000000 is a main offset. Extremely useful with IL2CPP method addresses. |
  | `btn mask frames` | Inject buttons. Presses need roughly 8–20 frames to register. |
  | `tap x y` | Run the tap handler at a 720p point. Same path as a real touch. |
  | `drag` | Synthetic finger drag. |
  | `ls` / `rs` | Override the left / right stick. |
  | `snap`/`chg`/`inc`/`dec`/`filt` | Cheat-Engine-style search inside a `range lo hi`. |
  | `msnap` / `mdiff` | Snapshot a block and diff it after an action. Best for finding fields. |
  | `scan64 v` | Find pointers to an object, e.g. the static that holds it. |
  | `sstr` | Search memory for a UTF-8 string. |
  | `rec` / `recd` | Record 4 fields every poll, optionally with a tap: timing bugs, animations. |
  | `perf` / `perfsec` | Cost of the mod per poll. |
  | `mfile` | Dump memory to `sd:/touchmod/mem.bin`. |

- **Hard-won rules:**
  - Read `cmd.txt` with an explicit byte count. nn::fs aborts the game (ResultFsOutOfRange) when the file is rewritten between
    GetFileSize and ReadFile.
  - Full-memory scans can drop Ryujinx to 1 FPS or crash it. Narrow them with `range` and prefer `msnap`/`mdiff`.
  - `scan64` steps 8 bytes and misses 4-aligned objects.
- **Workflow for a new screen:**
  1. Open the screen. Find the object that owns the cursor; here that was the common UI object's "hand owner".
  2. Read its index and count (`wl`).
  3. Step the cursor once with an injected d-pad press and diff, to get the row pitch.
  4. Write the handler, rebuild, restart, then test with `tap x y`.

### Ryujinx setup
- **Turn "Direct Mouse Access" OFF**, so mouse clicks arrive as touches. Keep a keyboard mapping for injected-input testing.
- **Take tap coordinates from game memory, not screenshots.** The Ryujinx window can stretch the image, and screenshot-to-720p
  estimates were off by up to 30 px. Read the cursor's or the widget's own position. Ryujinx's F8 screenshot writes a
  full-resolution PNG to `%APPDATA%\Ryujinx\screenshots`, which beats desktop captures (no overlays).
- Driving the emulator from Claude Code:
  - Request computer-use access for `E:\emulators\Ryujinx\Ryujinx.exe` (and `textinputhost.exe` if clicks get eaten).
  - Never `open_application` while it runs: that opens a second instance.
  - Launch it with PowerShell `Start-Process`.
- Static analysis tools: `tools/nso.py` (decompresses the NSO segments with lz4 into `main.bin`) and `tools/grepasm.py`
  (capstone full disassembly, then regex search). A full `main.asm` was about 2.9 M lines and took 7 s. Ghidra was also used
  (`work/`). For Engage, Il2CppDumper output replaces most of this (below).

---

## 2. Input design that worked (port the ideas, not the offsets)

The user's standing rule: **navigation by tap must be instant.**
- When a tap moves a selection, cursor, tab or page, set the target directly: the game's own SetIndex / select function, or its
  index field. Then let the game run its own update: **at most one** real d-pad step or a single A.
- Never walk there with repeated d-pad presses (`hand_chase` in `touch.h` is the last-resort fallback), and never wait out
  animations when a direct write works.

Patterns, all in `src/touch.h`:
- **Action queue** (`q_push`): injected presses with hold/gap frames, so the game sees clean edges.
- **Double tap to confirm** (`dt_arm` / `list_pick`):
  - The first tap on a row selects it and arms it.
  - A second tap on the same row presses A.
  - A tap on the row that's already selected confirms immediately.
  - Users expect the first tap never to confirm anything.
- **Replay a tap when the focus changes** (`retap_on_change`): for "tap a list while the cursor is in another pane". Press
  A/B/L/R to move the focus, then re-run the same tap on the poll the focus owner changes. Guard with a recursion depth
  (`rt_depth`).
  - **Watch out:** if the replay lands on the row that just took the cursor, `list_pick` presses A. For row 0 use the instant
    path instead: SetIndex, then A, then arm the double tap.
- **Generic list recognition:** every list in Three Houses derived from one cursor base class (index +0x4c, count +0x54, first
  visible row +0x6a). Two vtable slots were always the same base methods, so `is_list()` recognised *any* list without knowing
  the menu.
  - In Engage the equivalent is the IL2CPP class hierarchy: walk `klass->parent` until you reach the menu base class.
- **Row pitch:** a per-class seed table (`seeds[]`: pitch, column pitch, cursor-to-row-middle offset). For unknown lists, the
  pitch is learned at runtime by stepping the cursor once (probe step) and watching it move.
  - In Unity you can probably skip all of this and **hit-test the real RectTransforms** of the menu items (world corners ->
    screen) *(verify)*. Exact bounds, no pitch tables. The user explicitly preferred "taps register against the bounds of the
    actually visible boxes".
- **Prompt-bar buttons** (the "(X) Details"-style hints in the corners): read which buttons the bar shows (codes and labels) and
  map taps in their boxes to the button. Match on the label text when the same button set means different things on different
  screens.
- **Swallow taps you don't understand** on a known screen instead of letting them fall through to A. A stray A confirmed a
  house choice once.
- **Real controller input switches touch mode off** (`g_touch_mode=0` on any button), so the cursor reappears for pad play.
- **Drag = right stick** for the camera, scaled by user sliders. The camera follows the finger only while it moves and must
  not keep turning after it lifts.
- **Mod options inside the game's own Options screen:** an extra tab with IDs injected by code patches, persisted to
  `sd:/touchmod/settings.txt` (`src/opts.h`). Users liked this more than an external config.

### Battle map (Three Houses specifics; the method should transfer)
- **Tile picking:** project the tap through the camera's view-projection matrix onto the grid plane. A "Normal" mode
  hit-tests the drawn tile boxes using their real heights: the box top comes from the game's own height query, drawn
  range/target tiles win over raised geometry, and plain floor occludes boxes behind it. A "Flat" option keeps the plain grid.
  - Engage's maps are flatter, but they're 3D with a tilted, zoomable camera *(verify)*. Start with plane projection through
    the camera matrices; Unity's `Camera.ScreenPointToRay` may do it for you.
- **Set the cursor instantly:** call the game's own SetCursor with snapping, then play the cursor sound function so it feels
  native.
- **Target selection:** call the game's own "set target" with the tapped unit, then its refresh functions. Otherwise the
  forecast and the area markers (AoE, gambits) don't update.
- **Enemy tap:** the first tap only moves the cursor; the second shows the danger range (user request).
- **Pitfall:** a real held shoulder button (L) made targets "cycle rapidly". It looked like a mod bug; check the raw buttons
  before debugging your own code.

---

## 3. Engine: Unity / IL2CPP (what is different for Engage)

- **Get the metadata first.** Dump the game (exefs `main` + romfs). Run **Il2CppDumper** (or Cpp2IL) on `main` +
  `global-metadata.dat` (romfs, under `Data/Managed/Metadata/` *(verify)*). You get every class, field offset and method
  address, with names.
  - That replaces the vtable-guessing and `msnap` hunting that took most of the Three Houses time.
  - Import the generated `script.json` / header into Ghidra.
- **Calling managed methods:** IL2CPP methods are plain functions `ret f(this, args..., const MethodInfo*)`; pass NULL for the
  MethodInfo in most cases. Use `call` from the debug channel to try them live before writing code.
- **Finding live objects:** static fields live in `klass->static_fields`. Singletons (the game's `*Manager`-style classes)
  usually hang off a static field, so read the class pointer through its metadata usage slot (TypeInfo pointer) and
  dereference. Objects move with the GC, so never cache raw pointers across frames without re-reading them.
- **Class identity:** check an object's class with `obj->klass` (offset 0) against the TypeInfo pointer, not a vtable address.
  Use the parent chain for "is a list / is a menu".
- **Strings** are `System.String` (UTF-16, length at +0x10, chars at +0x14). Match prompt labels that way.
- **Engage's UI:** community bindings name classes like `BasicMenu` / `BasicMenuItem` (menus), `ProcInst` (the game's
  process/sequence system), `MapMind` / map cursor, and `GameUserData` *(verify, from the Engage modding community's
  Rust bindings)*. A common menu base with a selected index and an item list is the analogue of the Three Houses cursor base
  class: find its "select index" method and you have instant navigation for every menu.
- **Unity's own touch path:** check whether the game's uGUI EventSystem / input module would accept touches if fed. If the UI
  is pad-only, a translation layer like the one here is still needed.
- **Hook carefully:** IL2CPP generic sharing means one compiled body can serve many classes. Hook the specific method, then
  filter on `klass`.

---

## 4. Working with the user (standing preferences from the Three Houses project)

- Fix exactly the reported bug. **Don't add interactions they didn't ask for** (e.g. tappable portraits).
- For "ideas" or analysis questions, answer from code and offer a test. Don't launch the game unasked.
- The user may be playing in the same emulator: **ask before stopping emulation**. Close the game at the end of your own test
  sessions. In a battle, **bookmark first** (map menu -> Bookmark -> Yes) before any stop. Build, install, and tell them a
  restart is needed rather than restarting their session.
- **Only save slot 1 matters**; other slots are test saves. Never save or confirm anything in their game during tests. Back up
  the save folder before testing; Three Houses autosaved by itself at certain points and overwrote things twice.
- **The user is colorblind:** describe UI elements by shape and position ("the arrows over the tiles"), not by color.
- Commit only when asked. Keep `HANDOFF.md` (gitignored) for session notes and `OFFSETS.md` for addresses: one table row each
  with a brief description.
- Hand screens over to the user to test rather than going deep into the game yourself; they report bugs one screen at a time
  while playing.

---

## 5. Suggested order for Engage

1. Port the skeleton (`start.S`, `link.ld`, `elf2nso.py`, the hid hooks) and confirm `[touchmod] init main=` appears in the
   Ryujinx log.
2. Port the debug channel and `cmd.sh`. Patch the npdm for SdCard access.
3. Run Il2CppDumper and find the menu base class, its select-index method, and the map cursor and camera.
4. Do generic menus first (one handler for every menu via the base class). Then the battle map (tile tap, move confirm,
   targets), then the screen-specific prompts as the user reports them.
5. Add the in-game options tab last.
