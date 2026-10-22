#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;

// The temple trap run (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md): beyond
// the snake chamber's choked arch, a climb up the rubble and a wall-jump
// chimney to an upper level, then a parkour run of spike pits (wall runs),
// swinging axes, fire jets and saws, ending in a gauntlet of all four.
// Frame coordinates as in ChultTempleInterior.h: S toward the temple, T
// across, Z above the plaza (ChultTempleInteriorWorldOf converts).
namespace TempleRun
{
    constexpr float Floor = -1460;   // the halls' floor
    constexpr float UZ = -860;       // the upper level's floor
    constexpr float Top = UZ + 400;  // its usual ceiling (the plaza is 4.6 m above it)

    // 0. The collapse: the snake chamber's +S arch, the rubble heap climbed in six mantles, a chimney.
    constexpr float ArchS = 67900, ExitT = -7950, ExitHalf = 110;
    constexpr float CollapseS1 = 68310, CollapseTop = Floor + 480;
    constexpr float StepS0 = 67985, StepDepth = 54, StepRise = 34;
    constexpr int32 Steps = 6;
    constexpr float RubbleTop = Floor + Steps * StepRise;   // the chimney's floor
    constexpr float ShaftT0 = -8000, ShaftT1 = -7900;         // 1 m between its walls (the docks' cargo chimney)

    // 1. The pit gallery, along +T: 1.8 m wide so the chained wall run reaches the far wall.
    constexpr float BS0 = 68310, BS1 = 68490, BT0 = ShaftT0, BT1 = -4710;
    constexpr float PitDepth = 120;
    struct FPit { float T0, T1; };
    constexpr FPit Pits[] = {{-7300, -7080}, {-6560, -6100}, {-5600, -5140}};
    // The third pit's -S wall is broken back into an alcove for its first part: start on the +S wall.
    constexpr float BreakT0 = -5600, BreakT1 = -5380, BreakDeep = 70;

    // 2. The axe hall, along +S: a causeway between spiked trenches; the turn rooms at either end are part of it.
    constexpr float CS0 = BS0, CS1 = 71470, CT0 = -4710, CT1 = -4210, CTop = UZ + 420;
    constexpr float CausewayS0 = 69090, CausewayS1 = 70690, CausewayT0 = -4540, CausewayT1 = -4380, TrenchDepth = 150;
    constexpr float AxeArm = 315, AxePivotZ = UZ + 385, AxeAmplitude = 36, AxePeriod = 2.2f;
    constexpr float AxeS[] = {69390, 69990, 70190, 70390};

    // 3. The fire hall, along -T.
    constexpr float DS0 = 71080, DS1 = 71380, DT0 = -7700, DT1 = CT0;
    constexpr float FirePeriod = 3.f, FireWarn = .7f, FireBurn = 1.3f, FireReach = 200, FireZ = UZ + 38;
    struct FJetSpot { float T; bool bPlusWall; float Reach; };
    constexpr FJetSpot Jets[] = {{-5300, false, FireReach},
        {-5900, true, FireReach}, {-6150, false, FireReach}, {-6400, true, FireReach}, {-6650, false, FireReach},
        {-7050, true, 160}, {-7050, false, 160}, {-7350, true, 160}, {-7350, false, 160}};

    // 4. The saw passage, along +S, under a low ceiling; its ends are the turns from the fire hall and into the gauntlet.
    constexpr float ES0 = DS0, ES1 = 73740, ET0 = -7940, ET1 = DT0, ETop = UZ + 240;
    constexpr float SawRadius = 62, SawAxleZ = UZ + 92, SawLift = 80;
    struct FSawRow { float S; bool bDrops; };
    constexpr FSawRow SawRows[] = {{71780, false}, {72180, false}, {72400, false}, {72840, true}, {73060, true}, {73280, true}};

    // 5. The gauntlet, along +T, all four on one beat; 6. the antechamber with the way on (choked).
    constexpr float FS0 = 73560, FS1 = 73740, FT0 = ET1, FT1 = -3800, FTop = UZ + 400;
    constexpr float GauntletBeat = 2.4f;
    constexpr FPit GPits[] = {{-7400, -7180}, {-4500, -4040}};
    constexpr float GAxeS0 = 73400, GAxeS1 = 73900, GAxeT0 = -6800, GAxeT1 = -6100;   // a widened causeway chamber
    constexpr float GAxeT[] = {-6560, -6360};
    constexpr float GSawT0 = -6100, GSawT1 = -5300, GSawTop = UZ + 240, GSawRadius = 52;
    struct FGSaw { float T; bool bDrops; };
    constexpr FGSaw GSaws[] = {{-5900, false}, {-5600, true}};
    constexpr float GFireT0 = -5300, GFireT1 = -4600;
    constexpr float GJetT[] = {-5050, -4800};
    constexpr float AnteS0 = 73400, AnteS1 = 73900, AnteT0 = FT1, AnteT1 = -3200, AnteTop = UZ + 420;
}

// -ChuckTempleRunTest (in a Chult run): the real character through each section of the trap run.
void ReviewChultTempleRun(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
