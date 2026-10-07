#pragma once
#include "CoreMinimal.h"
class UWorld;
// Decorative only: the business lettering sits on a rough, bevelled timber face.
void AddDockTradeSign(UWorld* World,FVector Center,const TCHAR* Words,float Yaw,float Width=0.f);
void ReviewDockTradeSigns(UWorld* World);
