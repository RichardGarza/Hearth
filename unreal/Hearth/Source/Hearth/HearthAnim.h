// Tiny locomotion helper: swap between an idle and a walk clip based on speed. Works for any
// ACharacter with a mesh set to single-node animation mode. Replace with a real AnimBP later.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

namespace HearthAnim
{
	inline const TCHAR* BodyMeshPath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP");
	inline const TCHAR* IdlePath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle");
	inline const TCHAR* WalkPath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd");

	/** Call from Tick. Starts the right looping clip when the moving/still state flips. */
	inline void Update(ACharacter* C, UAnimSequence* Idle, UAnimSequence* Walk, bool& bWalking, float WalkClipSpeed = 200.f)
	{
		USkeletalMeshComponent* M = C ? C->GetMesh() : nullptr;
		if (!M || !Idle || !Walk) { return; }
		const float Speed = C->GetVelocity().Size2D();
		const bool bNowWalking = Speed > 20.f;
		if (bNowWalking != bWalking || !M->IsPlaying())
		{
			bWalking = bNowWalking;
			M->PlayAnimation(bWalking ? Walk : Idle, true);
		}
		if (bWalking)
		{
			// scale the clip so feet roughly match ground speed instead of sliding
			M->SetPlayRate(FMath::Clamp(Speed / WalkClipSpeed, 0.6f, 3.0f));
		}
		else
		{
			M->SetPlayRate(1.f);
		}
	}
}
