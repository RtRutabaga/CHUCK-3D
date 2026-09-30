# Docks soundtrack

`waterdeep_docks.wav` was supplied by the user from `C:/Users/ashsm/Downloads/waterdeep_docks.wav` for this prototype. Copied without modification. No original CHUCK-game repository files were accessed or changed. This is user-provided music, not a claim of a third-party stock license.

SHA-256: `2D856346F46EF5F1830A427E91B3C8848CFE332AE4EA9FBDACEC65B47CDE9DEA`.
Stereo PCM, 16-bit, 48,000 Hz, 158.4 seconds, 30,426,640 bytes.

Import with Unreal 5.7.4 using `Tools/import_dock_music.py` as a Python commandlet with `-AllowCommandletAudio` (required to initialize the decoder). Destination: `/Game/Art/Audio/SW_WaterdeepDocks`; included by the existing `/Game/Art` cooking rule. Source WAV and imported asset use Git LFS, roughly 49 MB total. Remote account allowance is unknown; no paid storage was purchased.

Music starts automatically, non-spatial, at 45% volume with a 1.5-second fade-in. It loops the complete supplied track, retaining its authored beginning/end; no seamless-loop edit or crossfade was fabricated. Position resets do not restart it. Runtime smoke checks start two seconds before the end and check playback four seconds beyond the boundary. This verifies engine playback state, not subjective loudness or audibility on the user's speakers.
