// Tiny locomotion helper: swap between an idle and a walk clip based on speed. Works for any
// ACharacter with a mesh set to single-node animation mode. Replace with a real AnimBP later.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

namespace HearthAnim
{
	// Epic's Manny/Quinn mannequins + unarmed locomotion, copied from the engine's template resources
	// into Content/Characters/Mannequins (see unreal/README.md).
	inline const TCHAR* MannyPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	inline const TCHAR* QuinnPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	inline const TCHAR* IdlePath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
	inline const TCHAR* WalkPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd");
	inline const TCHAR* JogPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd");
	constexpr float CapsuleRadius = 42.f;
	constexpr float CapsuleHalfHeight = 96.f;
	constexpr float WalkClipSpeed = 150.f;   // roughly the ground speed the walk cycle was authored at
	constexpr float JogClipSpeed = 380.f;
	constexpr float JogThreshold = 300.f;

	/** Apply the mannequin body to a character's mesh. */
	inline void SetupBody(ACharacter* C, USkeletalMesh* Mesh)
	{
		if (!C || !Mesh) { return; }
		C->GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
		C->GetMesh()->SetSkeletalMesh(Mesh);
		C->GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
		C->GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		C->GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}

	/** Call from Tick. Idle / walk / jog clip chosen from ground speed; play rate matched to speed. State: 0 idle, 1 walk, 2 jog. */
	inline void Update(ACharacter* C, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Jog, int32& State)
	{
		USkeletalMeshComponent* M = C ? C->GetMesh() : nullptr;
		if (!M || !Idle || !Walk) { return; }
		const float Speed = C->GetVelocity().Size2D();
		const int32 Now = Speed < 20.f ? 0 : (Speed < JogThreshold || !Jog) ? 1 : 2;
		// A single-node instance with no clip still reports IsPlaying()==true, so check the asset itself.
		const UAnimSingleNodeInstance* Single = M->GetSingleNodeInstance();
		const bool bNothingLoaded = !Single || Single->GetAnimationAsset() == nullptr;
		if (Now != State || bNothingLoaded)
		{
			State = Now;
			M->PlayAnimation(Now == 0 ? Idle : Now == 1 ? Walk : Jog, true);
		}
		M->SetPlayRate(Now == 0 ? 1.f : FMath::Clamp(Speed / (Now == 1 ? WalkClipSpeed : JogClipSpeed), 0.7f, 1.8f));
	}
}
