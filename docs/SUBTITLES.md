# NPC subtitles — October 9, 2026

The user requested larger, background-free subtitles inspired by the supplied Fable 1 screenshot, sentence cues for longer recordings, and an opening `... ` for the blacksmith's pause.

`ADockHUD` now draws centred warm-white Roboto (Unreal's bundled Regular face), 30 pt at 1080p, scaled with viewport height. A thin dark outline and shadow keep it legible without a panel. Speaker names and the existing F / Y close/advance hint remain smaller. Wrapping uses the same font measurements as rendering; row spacing comes from the font's measured height. Long unbroken words wrap too, and an unusually tall block scales down to fit the bottom third of the viewport. No font installation, import or new binary asset.

`DockSubtitles.h` shares sentence segmentation and time selection between direct conversations and overheard tavern dialogue. It preserves the supplied words and divides unusually long sentences at word boundaries (110 characters). Cues advance automatically with the current recording; F / Y retains its existing conversation behavior. When a direct recording ends, its last sentence stays until the conversation closes, rather than reverting to the whole paragraph. Text-only short lines retain their existing manual advance.

The smith shows `... ` while resting his hammer and during the recording's opening sigh/pause, then his three sentences. The first words are timed at 1.8 s into his existing audio. His existing hammer rest, voice delay, audio and return to work are unchanged.

Sentence timing is approximate, proportional to text length over the recording duration, matching the earlier tavern approach. These are not manually transcribed word timestamps. The blacksmith has the explicit opening-pause adjustment. Listening feedback may justify finer per-sentence timings later.

Review mode: use `-ChuckTalkCapture=<NPC tag> -ChuckSubtitleCapture` for a 20-second actual-conversation capture with HUD and cue/fit telemetry. Existing close-up captures retain their original behavior without the subtitle flag. `-ChuckTavernNightCapture -ChuckSubtitleCapture` logs the overheard cues too. These flags do not change ordinary player cameras.

Actual package/launcher evidence is recorded in the latest HANDOFF and Codex handoff. No claim of manual listening, physical-controller playtesting or the supplied character art target being achieved.
