# NPC voices, lip sync and body language: plan

Status: **started 2026-10-05: the dwarf speaks** (see Progress below; first planned (user, 2026-10-04: "go with the lighter recommended route … don't start yet, just build the md"). Owner: Claude (NPCs, rig, animation). Chuck stays silent (AGENTS.md); only NPCs speak.

## Progress

**2026-10-05, the dwarf (vertical slice of phases 1–3).** The user supplied his ElevenLabs line (voice "Paul - Calm, Relaxed and Deep", eleven_v4) with new text: "Ach, away wi' ye, ye mangy wee bastard—blow yer smoke somewhere else." It replaces his two old text lines.

- **Face rig (phase 1), dwarf only:** `jaw`, `lid_upper_l/r` and `brow_l/r`, built by `add_face_rig` in `Tools/build_npc_humans.py` for humans with `face: true`.
  - Bones are placed from his landmarks and skinned on the face before the clothing is cut. The beard under his mouth keeps part of the jaw's weight, and the brow and lash cards are re-skinned from the face.
  - Importing him merged the five bones into the shared `SKEL_Human`. The other humans and the clips don't carry them and were not re-imported, so the plan's full `-Clean` rebuild was not needed. Lip bones were left out: the bushy beard hides his lips.
- **Lip sync:** loudness-driven for now. Rhubarb was approved, but its download was stopped by the user, so it is not installed. `Tools/build_npc_voice.py` (Blender's `aud` + numpy, no new dependency) writes:
  - each line's WAV (`SourceAssets/NPCs/Voice/<NPC>/<id>.wav`);
  - a 60 fps loudness envelope in the generated `Source/Chuck3D/NPCVoiceData.h`.
  The jaw opens with it (up to 9°), and the brows lift on its peaks. Rhubarb mouth shapes remain the plan for NPCs whose mouths show.
- **Voices (phase 3):**
  - `SourceAssets/NPCs/dialogue.json` holds the lines; the user's source MP3 is kept in `Voice/<NPC>/source`.
  - `Tools/import_npc_voice.py` imports the WAVs to `/Game/Art/Audio/Voice/<NPC>/VO_<NPC>_<id>` (always cooked).
  - `ADockNPC::SetupVoice` sets the NPC's `Lines` to the voiced text, and `TickVoice` speaks each line Chuck reaches (spatial, from his head; it fades if Chuck leaves).
  - Blinks every 2–6 s, and one when he starts.
**2026-10-05, the woman gate guard.** The user supplied her line ("Blondie - Conversational", eleven_v4): "Stick to the docks, rat."
- Her face rig opens her visible lips. The split follows the traced lip contact (MPFB `lips` group): a smooth curve, each lip vertex assigned by facing near the line, soft toward the corners.
- Jaw range 7°.
- `-ChuckTalkCapture=<tag>` films any voiced NPC speaking.

**2026-10-05, the other two guards.** The plaza gate guard says "Move along, rat!" and the sewer-hatch guard "Stick to the docks, rat!" (user ElevenLabs clips). Both have face bones. The jaw no longer takes neck skin.

- **Not yet:** barks (scratch/notice), subtitles replacing the text box (the existing dialogue text still shows the line), music ducking, Rhubarb, other NPCs' voices, the zombie's sounds, loudness-driven gestures.

## Goal

The NPCs speak their lines aloud instead of showing a text box, with:
- the mouth moving in time;
- blinks, brows and nods on stressed words;
- gestures that follow the line.

They also react aloud to things: being scratched, or noticing the rat. The target is believable at the game's camera distance (Chuck looks up at people from 65 cm), not film close-ups.

Rejected alternative: MetaHumans, with Unreal's audio-driven faces. They would mean a different character system for every NPC, they're heavy on this 16 GB machine, and they clash with the MakeHuman look. Revisit only if close-up conversation becomes central.

## What exists (2026-10-04)

- **NPCs:** `ADockNPC`, which plays MPFB humans on the shared `SKEL_Human` skeleton (MPFB `game_engine` rig: 53 bones).
  - Each body is posed bone by bone at runtime through a `UPoseableMeshComponent`.
  - Motion-capture idle clips, a Talk clip, a React clip (the scratch), a head look-at and a body turn.
- **Speaking NPCs:**
  - Guard, GuardWoman and SideGuard: "Stick to the docks, rat."
  - Market woman: "No handouts here. If you're hungry, you should check the sewer for scraps."
  - Blacksmith (Codex): "Mind the sparks, rat." and "Go on. I've a hinge to finish."
- **Non-speaking NPCs:**
  - The dock worker has no lines (user's call).
  - The zombie is silent; its tell is visual only, a known flaw.
- **Faces: none yet.** The heads have only `neck_01` and `head`: no jaw, no face bones and no shape keys (checked in `SK_Guard.fbx`). The eyes, brows and lashes are separate meshes fitted by MPFB.

## Pipeline

### 1. Lines: `SourceAssets/NPCs/dialogue.json` (new)

Per NPC:
- `talk`: the lines Chuck gets with F/Y, in order (as `Lines` now).
- `barks`: short reactions, picked at random with cooldowns:
  - `scratched`: when Chuck's slash catches them; plays with the existing React clip.
  - `noticed`: optional, the first time the rat comes close.
- `voice`: the ElevenLabs voice name or ID, and settings notes.

The zombie gets non-verbal `groan` (idle), `tell` (the windup before a lunge; this fixes the visual-only tell) and `hurt` / `death`. Possibly use ElevenLabs sound-effect generation for these rather than a voice.

Line text and voice choices are the user's.

### 2. Voices: ElevenLabs, generated offline

- **Files:** generated once and committed as audio, `SourceAssets/NPCs/Voice/<NPC>/<id>.wav` (or .mp3). No runtime API calls.
- **Who generates:** the user generates them in the ElevenLabs web app (or Claude scripts the API if the user sets `ELEVENLABS_API_KEY` in their own environment; Claude never types a key). Then `Tools/import_npc_voice.py` imports them as `SoundWave`s.
- **Licence:** ElevenLabs commercial use needs a paid plan, so check the plan before any release. Use designed or library voices, not clones of real people.
- **Playback:** spatial, with the same falloff as other world sounds, and ducking the music slightly while someone speaks.

### 3. Lip sync: Rhubarb Lip Sync (offline)

- **Tool:** Rhubarb Lip Sync (Daniel Wolf, MIT licence, free, offline command-line tool). The user agreed to install it. Planned at version 1.14.x as a portable zip in `%LOCALAPPDATA%\Programs\CHUCK-Tools\rhubarb`, outside the repository; record the version and SHA-256 in `docs/SETUP.md` at install.
- **Why Rhubarb, not ElevenLabs timings:** ElevenLabs' timestamps are per letter of the script, and English spelling maps poorly to mouth shapes. Rhubarb analyses the audio itself (phonetic recognition), takes the script as a hint (`--dialogFile`), and outputs a mouth-shape track. Its shapes are A–F (basic) plus G, H and X (extended).
- **Run:** `Tools/build_npc_visemes.py` runs Rhubarb on each line and writes `SourceAssets/NPCs/Voice/<NPC>/<id>.visemes.json` (mouth shape over time), with a loudness envelope from the audio itself (for nods and brows).

### 4. Face rig: a few face bones on the shared skeleton

Bones, not shape keys, because the runtime poses with `UPoseableMeshComponent`, which drives bones reliably; morph targets would need a component change, which is a risk to check first. Added in `Tools/build_npc_humans.py` after MPFB builds each body:

| Bone | Parent | Drives |
|---|---|---|
| `jaw` | head | opens the mouth (the main lip-sync motion) |
| `lip_upper`, `lip_lower` | head, jaw | lips together (M/B/P), F/V, rounded O/U |
| `lip_corner_l`, `lip_corner_r` | head | wide (E/I) vs narrow (O/U), a faint smile or set mouth |
| `lid_upper_l`, `lid_upper_r` | head | blinks (skinned to the lids and the eyelash cards) |
| `brow_l`, `brow_r` | head | brow raise and furrow on stressed words and reactions |

- **Skinning:** each bone is placed from MPFB landmarks on that body (mouth corners, chin, lids, brows) and skinned with distance falloff on the face, so it fits every NPC.
- **Teeth and tongue:** MPFB can fit them (it ships teeth/tongue assets), so an open mouth isn't hollow; worth adding at the same time.
- **Skeleton change:** this changes the shared skeleton. Every human is rebuilt and re-imported (`Import-NPCHumans.ps1 -Clean`). The motion-capture clips don't carry these bones, so they rest at neutral and the runtime poses them. Also rebuild the mocap FBXs so their bind pose matches.

### 5. Runtime: speaking in `ADockNPC` (or a small `UNPCVoiceComponent`)

- **Talk:** F/Y on an NPC plays the line's sound, shows a small subtitle, and blends to the Talk clip, as now. The next press goes to the next line or skips.
- **Mouth:** each Rhubarb shape maps to a jaw, lip and corner pose. Cross-fade over about 60 ms (coarticulation), so it doesn't flap; ease the jaw.
- **Face life:**
  - Blinks every 2–6 s, and on surprise.
  - Brows lift on loudness peaks.
  - A small head nod or tilt on stressed syllables, layered on the look-at.
  - Eyes keep looking at Chuck.
- **Body:** Talk-clip gesture intensity follows the loudness envelope (bigger on emphasis, settling in pauses); a final beat (a shrug or turn-away) when the line ends.
- **Barks:**
  - `TakeScratch` plays a `scratched` bark with the React clip (cooldown about 3 s, never over a talk line).
  - Optional `noticed` once per approach.
  - The zombie's `tell` plays at the start of the windup.
- **Subtitles:** one small line near the bottom (speaker name and text) that fades with the line. They replace the text box; optional to switch off later.

### 6. Tests and evidence

- **Smoke tests:**
  - Every line and bark has its sound and visemes loaded.
  - During a line the jaw opens and closes many times (it actually moves), and is at rest between lines.
  - A scratch plays a bark once within its cooldown.
  - The subtitle shows and clears.
  - The zombie tell sound fires with the windup.
- **Captures:** face close-ups per NPC at a few times in a line (open and closed mouth, blink), and a short frame sequence of one talk.
- **Verification:** package verification as usual. Listening and the subjective lip-sync check are the user's.

## Phases (each its own session; see the session note in HANDOFF)

1. **Face rig:** bones, skinning, teeth/tongue, re-import. Check the poseable mesh drives them; neutral-pose captures. *Biggest piece.*
2. **Rhubarb:** install and record it; `build_npc_visemes.py`; a test line (a placeholder voice is fine) to tune the mouth poses and timing.
3. **Voices:** the user generates the ElevenLabs lines and barks from `dialogue.json`; import; subtitles; talk playback; barks on scratch.
4. **Body language and polish:** loudness-driven gestures, nods, brows, blinks, the zombie's voice, captures, package and promote.

## Open questions for the user

- **Lines:** the dialogue for each NPC, including whether the dock worker speaks and what each says when scratched (a few short variants each).
- **Voices:** picks per NPC from the ElevenLabs library or voice design (the guards could share one voice, or differ).
- **Generation:** whether the user generates the audio by hand, or provides `ELEVENLABS_API_KEY` in their own environment for a script.
- **Licence:** whether the ElevenLabs plan covers commercial use, if this ships.

## Mix (2026-10-06)

User: "make all npc dialogue louder in relation to the music". `build_npc_voice.py` levels every line to -18 dB speech RMS (peaks capped at -1 dBFS), and `ADockGameMode::UpdateMusicDuck` lowers the score to 0.35 of its level while any NPC speaks. New lines get both automatically.
