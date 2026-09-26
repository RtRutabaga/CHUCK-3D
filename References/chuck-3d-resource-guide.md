# Chuck 3D — Free Resources & Compute-Saving Guide

Directional guide for building the 3D version of Chuck (Unreal Engine 5 + Blender). The goal: lean on free and open-source tools and assets wherever possible, and reserve Claude Code and Codex usage for the hard, custom systems work.

**Note:** everything below is a suggestion, not a rigid rule. Swap tools, skip steps, or take a different approach whenever it makes sense for the project.

---

## Core Principles

1. **Buy/borrow before building.** If a free asset, animation, or plugin gets us 80% of the way, use it and customize, rather than generating from scratch.
2. **AI coding is for systems, not art.** Claude Code and Codex handle gameplay code, tooling, and integration. Art comes from asset libraries, AI 3D generators, and Blender cleanup.
3. **Reuse the 2D game.** Story, quests, dialogue, item logic, and flags already exist. Port that data; do not regenerate it.
4. **Prototype first.** Build one area + Chuck's parkour controller as a vertical slice before scaling up.
5. **Protect weekly limits.** Route small, repetitive tasks to local models or manual work; save frontier-model sessions for complex problems.

---

## Engine & Core Tools

| Tool | Cost | Use |
|---|---|---|
| **Unreal Engine 5** | Free until $1M revenue | Main engine. Lumen, Nanite, Groom, Niagara, Control Rig built in. |
| **Blender** | Open source | Modeling, retopology, rigging, UVs, cleanup of AI-generated meshes. |

Godot is fully open source but not a fit for the target visual quality. Stay on Unreal.

---

## Movement & Parkour

| Resource | Cost | Notes |
|---|---|---|
| **Epic's Game Animation Sample Project** | Free (not open source) | Motion matching setup plus hundreds of mocap animations, including traversal (vault, mantle). Best starting foundation for parkour. |
| **ALS-Refactored** (GitHub) | Open source | C++ rewrite of Advanced Locomotion System; strong reference for locomotion logic. |
| **CMU Motion Capture Database** | Free | Large library of raw mocap clips for extra moves. |
| **Mixamo** | Free (not open source) | Quick humanoid animations, useful for NPCs. |

### ⚠️ Chuck-Specific Caveat
Chuck is a knee-high anthropomorphic rat, not a human. Human mocap must be **retargeted** to his skeleton and will need hand cleanup (shorter limbs, rat feet, long snout, tail).
- Use **Unreal's IK Retargeter** to map human animations to Chuck's rig.
- Use **Control Rig** for procedural tail motion and secondary movement so the tail reacts naturally to jumps, wall runs, and turns.
- Budget real time for animation polish. This is likely the single hardest part of making parkour feel good.

---

## Characters

| Resource | Cost | Use |
|---|---|---|
| **Hunyuan3D** (Tencent) | Open weights | Image-to-3D base meshes from concept art. Runs locally with a strong GPU. |
| **TRELLIS** (Microsoft) | Open source (MIT) | Alternative image-to-3D generator. |
| **MetaHuman** | Free with Unreal | Human NPCs only. Not usable for Chuck. |
| **MakeHuman** | Open source | Quick base human bodies for NPCs. |

### Chuck Pipeline
1. Generate base mesh from concept art (Hunyuan3D or TRELLIS).
2. Resculpt and **retopologize in Blender**. AI meshes are messy and deform badly when animated without this step.
3. Rig in Blender (biped + tail chain + ears/whiskers if desired).
4. **Fur:** Unreal Groom system. Keep fur shorter and simpler than the concept art for performance.
5. **Jacket:** separate mesh with Chaos cloth physics for flapping during runs.
6. **Cigarette smoke:** small Niagara particle effect.

### Visual Target
"Indie AA real-time" quality (reference: *It Takes Two*), not pre-rendered film quality. One hero character can get close to the concept art; everything else prioritizes consistency over maximum detail.

---

## Environments & Textures

| Resource | Cost | Use |
|---|---|---|
| **Quixel Megascans** (via Fab) | Free for Unreal | Photoscanned rocks, foliage, terrain, surfaces. Main visual workhorse. |
| **Poly Haven** | CC0 | Textures, HDRIs, models. Free for any use. |
| **ambientCG** | CC0 | PBR materials and textures. |
| **Fab marketplace** | Free monthly assets + paid | Medieval town/harbor kits, props, ships. Check free monthly offerings regularly. |

Harbor, ships, crates, barrels, and castle city are all well served by existing kits. Chuck's small size means props (crates, rigging, barrels, posts) double as parkour routes. Design levels around that.

---

## Story & Dialogue

| Resource | Cost | Use |
|---|---|---|
| **Ink** (inkle) | Open source (MIT) | Branching dialogue scripting with Unreal integrations. |
| **Yarn Spinner** | Open source | Alternative dialogue system. |

Port the existing 2D game's dialogue, quest flags, and item logic into one of these **once**. Do not have AI rewrite story content.

---

## Audio

- **Freesound.org** — Creative Commons sound effects (check each license).
- **Sonniss GDC Game Audio Bundles** — large free royalty-free SFX packs released yearly.

---

## Saving AI Compute (Claude Code + Codex)

### Use frontier models for
- Parkour/traversal controller logic and state machines
- Camera system
- Combat and enemy AI
- Save system, interaction system, UI framework
- Debugging complex engine issues

### Offload elsewhere
- **Local coding models** (e.g., Qwen Coder via Ollama): boilerplate, simple scripts, data conversion, formatting.
- **Manual/editor work:** level layout, lighting passes, asset placement, playtesting.

### Session habits
- Keep a **project notes file** (e.g., `CLAUDE.md` / `AGENTS.md`) with architecture, conventions, folder layout, and current goals, so each session doesn't re-read the whole codebase.
- Use `/clear` between unrelated tasks to avoid carrying large context.
- Point the AI at **specific files**, not whole directories.
- Prefer small, well-scoped tasks over "build the whole system" prompts.
- Split work between the tools: e.g., Claude Code on core gameplay systems, Codex on tooling, data import, and editor utilities, to avoid both limits draining on the same task.

---

## Suggested Prototype Milestone

**Vertical slice:** one harbor area + Chuck fully rigged with basic parkour (run, jump, climb crates, vault, ledge grab) + one dialogue interaction ported from the 2D game.

Timing this slice gives the most reliable estimate for the full ~1-hour game.
