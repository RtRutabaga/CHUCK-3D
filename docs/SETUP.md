# Computer and setup assessment

Inspected 2026-09-24, before installations or version selection.

| Item | Observed |
| --- | --- |
| OS | Windows 11 Home, 64-bit, 10.0.26200 |
| CPU | AMD Ryzen 5 7600, 6 cores / 12 logical processors |
| RAM | 16,154,386,432 bytes reported usable physical memory, about 15.0 GiB (16 GB class) |
| GPU | NVIDIA GeForce RTX 4060, 8,188 MiB dedicated VRAM; driver 595.95 |
| Other adapters | AMD integrated graphics and Virtual Desktop Monitor |
| C: capacity/free | 952.7 GiB total / 329.5 GiB free at inspection |
| Git | 2.55.0.windows.2 |
| Git LFS | 3.7.1 installed |
| Other development tools | GitHub Desktop 3.6.2; dotnet runtime command exists, but no SDK reported |
| Not detected | Epic Games Launcher, Unreal Editor, Blender, Visual Studio/MSVC, Windows Kits 10 SDK, CMake, VS Code, Python on PATH |

Evidence: read-only CIM hardware queries, NVIDIA's nvidia-smi (WMI's 32-bit VRAM field was inaccurate), installed-program registry entries, PATH commands, Program Files directories, Epic manifest locations, user Programs location, Unreal build registry and dotnet SDK listing. This is not an exhaustive scan of arbitrary portable installations. No other fixed volume was reported. Disk type was not established.

Assessment: suitable for attempting a small primitive-based prototype; performance is not yet measured. RAM is below Epic's 32 GB recommendation, and the GPU meets its 8 GB VRAM recommendation. Start with modest viewport settings, simple lighting, no required Lumen/Nanite/ray tracing, and close Blender while profiling Unreal. A RAM upgrade may help but is not required before trying the graybox.

Source checked: [Epic hardware/software requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-and-software-specifications-for-unreal-engine). Requirements vary by engine release; consult the selected release's toolchain matrix before installing a compiler.

## Installation sequence

1. Install Epic Games Launcher and one stable, supported Unreal binary release for Windows. This supplies the editor, templates and runtime prerequisites. Avoid a source-engine build, optional platform SDKs and large sample projects for this milestone. Record the actual engine patch and launcher install-size estimate before downloading.
2. Begin with a Blueprint project; C++ is not required to evaluate movement, cameras and interactions. If C++ or a plugin requires compilation, install the matching Visual Studio Game Development with C++ workload, MSVC toolset and Windows SDK specified by that engine release. Verify packaging prerequisites with an early empty Windows package rather than assuming editor play proves deployment works.
3. Install a supported stable/LTS Blender Windows build for authoring custom models after camera/scale tests. Record the exact version and prove a 30.48 cm reference object's export/import scale. Blender is not needed for primitive blockout.
4. Pin versions only after these compatibility checks. No tools were installed and no versions selected during this setup.

Planning allowance, not vendor download sizes: reserve 100–150 GiB for editor, caches, project growth and packaging, keeping at least 100 GiB free. Recheck the launcher's actual selection size and disk space before installation. Store installations and caches outside the repository.

The workspace is under OneDrive. Avoid concurrent sync/edit of Unreal binary assets; Git is the history source. If synchronization causes locks, arrange a separate nonsynced checkout rather than moving this project or the original without discussion.

## Git, binary assets, and deployment

This repository uses its own main branch and local LFS hooks. No remote is configured. Unreal .uasset/.umap, Blender sources and common binary asset imports are assigned to LFS in .gitattributes before any are added. No binary assets are committed at this stage.

Before the first binary commit: check git check-attr, git lfs status and git lfs ls-files; confirm pointers rather than raw blobs in the index. Before publishing, choose a separate remote with LFS support and confirm storage/bandwidth limits and locking support. Validate a fresh clone plus git lfs pull. Installed LFS alone does not prove remote availability or provide automatic backups; avoid simultaneous editing of the same binary asset.

Keep .uproject, Config, Content, source assets and any Source code under version control. Ignore Binaries, DerivedDataCache, Intermediate, Saved, IDE state and packaged Builds. Retain Build source resources such as icons and any deliberately supplied plugin binaries by explicit review rather than globally ignoring all DLLs.

Package only to this project's ignored Builds/Windows directory. No original-game deployment scripts or GitHub Pages configuration are copied. A future distribution destination must belong to CHUCK 3D.
