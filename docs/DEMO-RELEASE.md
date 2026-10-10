# Windows friends demo

The user authorized a downloadable GitHub progress demo on October 9, 2026. Use the current verified Development Windows build, including cooked game files and the native executable. This is a portable ZIP, not a single-file executable or an installer. The player extracts it and opens the root `Chuck3D.exe`; no editor or source checkout is needed.

`Tools/Package-FriendsDemo.ps1 -PackageRoot <verified Windows folder>` checks the executable against its build receipt, copies required runtime files while excluding Saved output, debug symbols and build manifests, adds the player guide/credits and the installed engine's Microsoft VC++ x64 redistributable, then produces a ZIP and SHA-256 sidecar in ignored `Builds/Releases`. It does not cook assets or install software. Extract and verify that archive separately before uploading it.

The demo removes the timber-shop orange placeholder panes and fills their cladding patches. Open-sky falls in the cellar end on a plain black screen with Escape to exit. Other Astral falls keep their existing recovery. This endpoint is temporarily bypassed only by the established traversal smoke harness; the isolated demo-ending gate runs the normal player path and is required by Verify-Package.

Publish the verified archive and checksum as assets of a GitHub prerelease tagged for the demo date and tied to its runtime commit. Keep binaries out of source Git and retain the prior root-launcher package. Do not publish logs, screenshots, local receipts or machine-specific paths inside the archive. Record the release URL, byte size, checksum and actual extraction/startup checks in HANDOFF.
