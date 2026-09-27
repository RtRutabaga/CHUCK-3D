# AI Dev Notes — Chuck 3D

Read this before working on the project. It's written for the AI agents on this project (Claude Code running Opus 5.5 and Codex running GPT-6 Astra). It covers what the game is, how the work is split, and the practices that have worked for other people building Unreal games with AI.

**Everything here is guidance, not rigid rules.** If a better approach fits a specific task, use it and briefly note why.

See also: `chuck-3d-resource-guide.md` (free tools, assets, and pipelines).

---

## 1. What We're Building

A 3D remake of **Chuck**, an already-finished SNES-style top-down RPG. The story, setting, characters, quests, and dialogue already exist. **Don't invent or rewrite story content.** Port it from the 2D game.

**Scope:**
- About 1 hour of gameplay
- Visual target: modern real-time 3D at "indie AA" quality (think *It Takes Two*), aiming toward Baldur's Gate 3 polish where it's affordable
- World scale: large areas comparable to *Zelda: Twilight Princess*
- Detailed, fluid **parkour-style movement** is the signature mechanic
- **No leveling, no skill tree, no currency.** Only **two lootable items** in the whole game.

**Stack:** Unreal Engine 5 + Blender. We're in the **prototype** phase.

---

## 2. The Main Character: Chuck

- Anthropomorphic brown rat who walks upright on two legs
- About **knee height to a human NPC**, or slightly taller
- Cream belly fur, pink ears, long pink tail
- Wears an unzipped **purple zip-up jacket**
- Always has a **lit cigarette** in his mouth, with a thin smoke wisp
- Concept art: a harbor dock and a ship deck in a sunny medieval port city, with a castle city on the cliffs behind

**Implementation notes:**
- **Skeleton:** biped body plus a tail bone chain. Ears and whiskers are optional.
- **Animation:** human mocap (Epic's Game Animation Sample, CMU, Mixamo) gets **retargeted** with the IK Retargeter. Expect hand fixes for the shorter limbs, rat feet, and snout.
- **Tail:** procedural motion through Control Rig or physics. It should react to jumps, turns, and wall runs.
- **Fur:** Unreal Groom, kept short for performance.
- **Jacket:** a separate mesh with Chaos cloth physics.
- **Smoke:** a small Niagara particle system attached to the cigarette socket.
- **Scale drives level design.** Because Chuck is small, crates, barrels, rigging, posts, and market stalls are his parkour routes. Build traversal around props, not human-scale architecture.

---

## 3. Unreal + AI: What We Know Works

### C++ first, Blueprints second
Unreal stores Blueprints and assets as binary `.uasset` files. AI agents can't read or edit them directly and have to go through a bridge (an MCP server or editor plugin).
- Put **core gameplay logic in C++** and expose it to Blueprints with `UFUNCTION(BlueprintCallable)` / `UPROPERTY(EditAnywhere, BlueprintReadWrite)`.
- Use Blueprints mainly for **wiring, tuning values, and designer-facing hookups**.
- AI-edited Blueprint graphs tend to turn into tangled "spaghetti." Keep Blueprints thin and put the complexity in C++.

### MCP bridge to the editor
When a task needs the editor (placing actors, editing materials, Blueprint changes, lighting), use the project's configured Unreal MCP server rather than guessing at binary files. Options include StraySpark, unreal-mcp, UnrealCodex, and Unreal Claude / Vibe UE. There's also a Blender MCP server for modeling tasks.

### Destructive operations need approval
**Deleting assets, running console commands that change project state, bulk renames or moves, and force-overwriting levels all need explicit human approval first.** Save levels before large editor operations.

### Be precise
Refer to exact file paths, class names, Outliner actor names, and coordinates. Vague references waste tokens and cause wrong edits.

---

## 4. Division of Labor

| | **Claude Code (Opus 5.5)** | **Codex (GPT-6 Astra)** |
|---|---|---|
| Best at | Ambitious, open-ended features; systems that span several files; visual and feel quality | Narrow, well-specified tasks with clear inputs and outputs |
| Assign | Parkour/traversal controller, camera, combat and enemy AI, animation logic, integrating assets so they look right | Utility functions, data import/conversion, editor tools, save/load serialization, unit tests, isolated bug fixes |
| Watch for | Over-engineering; keep solutions as simple as the prototype needs | Drifting on loosely scoped prompts; reusing outside GitHub projects (see below) |

This split protects both weekly usage limits by keeping the two agents off the same task. Early reports suggest Opus 5.5 does better on open-ended game builds, but the evidence is still thin, so adjust the split as real results come in.

### Original code only
**Don't copy or adapt existing open-source game clones or repos wholesale.** Chuck is original IP. Using established open-source *libraries and plugins* is fine (Ink, ALS-Refactored as reference, and so on). Check their licenses, and note any use in `THIRD_PARTY.md`.

---

## 5. What AI Should Not Try to Do

- **Generate final 3D models, textures, or fur by itself.** Art comes from asset libraries (Megascans, Fab, Poly Haven), image-to-3D tools (Hunyuan3D, TRELLIS), and Blender cleanup. The AI's job is **integrating** that art: importing it, setting up materials, and hooking it up in Blueprints.
- **Hand-write complex shaders from scratch** when an existing material function or a Megascans material already does the job. Shader work is still a weak spot for every current model.
- **Rewrite story or dialogue.** Port it from the 2D game.
- **Add progression systems** (XP, levels, currency, shops, skill trees). They're out of scope on purpose.

---

## 6. Porting From the 2D Game

- Pull **dialogue, quest flags, NPC data, and the two lootable items** straight from the 2D project's data files.
- Put dialogue into **Ink** (or Yarn Spinner) once, and have Unreal read from it.
- Keep a mapping doc (`PORTING.md`) of what's been ported and where it lives in the 3D project.
- If the 2D logic is ambiguous, **ask**. Don't guess about story intent.

---

## 7. Session and Token Habits

- **`.claudeignore` / Codex ignore settings** should exclude `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, and raw `.uasset` / `.umap` files.
- Read **only the files a task needs**. Don't scan whole directories.
- Start a **fresh session** for each unrelated task.
- Break big features into small, testable steps. Compile and test after each one.
- When a task finishes, update the **Current Status** section below in one or two lines, so the next session doesn't have to rediscover the project.
- If something eats unusual amounts of context (such as repeated Blueprint parsing), flag it. A summarized interface or cache layer may be worth building.

---

## 8. Prototype Milestone (Current Goal)

**Vertical slice:**
1. One harbor area (dock, ship deck, market street)
2. Chuck imported, rigged, and moving with basic parkour: run, jump, climb crates, vault, ledge grab, and tail motion
3. Third-person camera suited to a small character
4. One NPC dialogue interaction ported from the 2D game
5. One of the two lootable items, pick-up-able

How long this slice takes is the calibration point for estimating the rest of the game.

---

## 9. Current Status

_Update briefly at the end of each task._

- [ ] Harbor greybox
- [ ] Chuck mesh and rig
- [ ] Locomotion + parkour controller (C++)
- [ ] Camera
- [ ] Dialogue system (Ink) + first NPC
- [ ] First lootable item
