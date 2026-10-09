# Late sewer sprint-leap rupture

October 9 visual revision: Astral side walls now block the camera channel, so the existing spring arm retracts as it does at solid walls. They still ignore pawn movement and Visibility traversal probes. The panels are planar along the opening, removing the folded fin shown in the user's screenshot. The user explicitly requested limited visual/camera checks and accepts a possible wall-run bypass; the full sprint-only traversal suite is not rerun for this revision. The focused receipt records that scope.

The October 9 request adds a second full-width Astral break after the wide zombie chamber, farther along the tunnel and before the end. The earlier narrow wall-run rupture remains. Zombies, rats, the sewer checkpoint, the slide and the character/controller are unchanged.

The new opening occupies route samples 306–310, about **2.60 m** along the route. Both banks and the stream are interrupted. The lower side walls are torn away across the break and about 3.25 m on each approach, preventing a continuous wall-run crossing. The upper cave arch remains; noncolliding recessed purple Astral veils fill the side tears. Existing materials and lighting are reused. The floor lips are flattened across the spline bend so curved bank geometry cannot fold into the hole. NPC-only invisible floor still spans the opening; Chuck falls through it.

Tap run (Shift / Xbox LB), move straight, engage four-legged sprint (Left Ctrl / left-stick click), and allow at least **1 m** of straight sprint run-up before jumping (Space / Xbox A) near the lip. The existing sprint leap covers roughly 2.88 m; ordinary running jumps fall short. Falling uses the existing post-chamber checkpoint before the earlier wall-run break.

`-ChuckLeapRiftTest` runs six real-controller trials: one successful four-legged sprint leap, three ordinary running jumps at different takeoff distances, and wall-assisted attempts on both sides. Failure cases must actually start their running jump or wall run and fall without reaching the landing. `-ChuckLeapRiftMotionCapture` records their motion; `-ChuckLeapRiftCapture` provides three fixed views. The default package verifier requires these six trials and the separate floor/wall collision gate before accepting the package. Existing regressions and thresholds remain.

There are now **33** Astral openings: 13 large and 20 small. `-ChuckSewerTest` was updated to attempt this leap during the optional whole-route walkthrough, but that walkthrough has not been rerun for this change. See the latest HANDOFF for actual packaged checks and visual review. Automated input is not a physical-controller or player comfort test.

No new dependencies, binary assets, imports, animation changes or movement tuning.
