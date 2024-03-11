// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/DropletPlayerCharacter.h"

#include "DropletPlayerController.h"
#include "Framework/VeinLogCategories.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputComponent.h"
#include "../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/Interactables/InputInteractableActorComponent.h"
#include "Dialogues/VeinDialogueActorComponent.h"


void ADropletPlayerCharacter::SetMaterialState(EDropletMaterialState eNewMaterialState, bool bIsPlayerInitiated /* = false */)
{
	//If the material state is not none
	if (eNewMaterialState != EDropletMaterialState::EDropletMaterialState_None)
	{
		//Get the material state description from the material state
		TObjectPtr<UDropletMaterialStateDescription> materialStateDescription = UDropletMaterialStateDescription::GetMaterialStateDescriptionFromState(eNewMaterialState);

		//If the material state description is valid
		if (materialStateDescription != nullptr)
		{
			//Change the CharacterMovement MovementMode if the CharacterMovement is valid
			if (TObjectPtr<UCharacterMovementComponent> pCharacterMovement = GetCharacterMovement())
			{
				pCharacterMovement->SetMovementMode(materialStateDescription->GetMovementMode());
			}

			//Apply Character material state description specificities
			materialStateDescription->ApplyCharacterSpecificities();

			//Apply CharacterMovementComponent material state description specificities
			materialStateDescription->ApplyCharacterMovementComponentSpecificities();

			//Bind the move function to the move function in the material state description
			m_MoveFunction.BindUFunction(materialStateDescription, FName("MoveFunction"));

			ChangeInputMappingContext(eNewMaterialState);

			ChangeSkeletalMeshInstance(eNewMaterialState);

			ChangeMeshMaterialInstance(eNewMaterialState);

			//Notify the material state change
			OnMaterialStateChange.Broadcast(eNewMaterialState);
			BPE_OnMaterialStateChanged(eNewMaterialState);

			ChangeStaminaComponent(eNewMaterialState);

			ApplySpeedComponentStateValues(eNewMaterialState);

			// Apply the material states duration and cooldown
			ApplyMaterialStateDurationAndCooldown(eNewMaterialState);

			// If the material state is not solid, cancel slide dashing
			if (eNewMaterialState != EDropletMaterialState::EDropletMaterialState_Solid)
			{
				m_bIsSlideDashing = false;
				m_fTransitionSpeedBoostElapsedTime = 0.f;

				// Also cancel slide dash breaking
				m_bIsSlideDashBreaking = false;
				m_fSlideDashBreakElapsedTime = 0.f;

				// Remove the Breaker marker from the InteractableMarker array of the character
				RemoveInteractableMarker<UBreakerInteractableMarker>();
			}

			// If the material state is not liquid
			if (eNewMaterialState != EDropletMaterialState::EDropletMaterialState_Liquid)
			{
				// Remove the Driller marker from the InteractableMarker array of the character
				RemoveInteractableMarker<UDrillerInteractableMarker>();
			}
			// Else if the previous material state was gazeous (and the new one is Liquid and player initiated)
			else if (bIsPlayerInitiated && m_pDropletPlayerController->GetMaterialState() == EDropletMaterialState::EDropletMaterialState_Gazeous)
			{
				// Add the Driller marker to the InteractableMarker array of the character
				AddInteractableMarker<UDrillerInteractableMarker>();
			}

			m_bJustChangedState = true;

			//Log the new material state
			UE_LOG(LogMaterialStateMachine, Log, TEXT("ADropletPlayerCharacter::SetMaterialState: eNewMaterialState is %s"), *UEnum::GetValueAsString(eNewMaterialState));
		}
	}
	//If the material state is none
	else
	{
		GetCharacterMovement()->DefaultLandMovementMode = EMovementMode::MOVE_None;

		//Unbind the move function
		m_MoveFunction.Unbind();

		UE_LOG(LogMaterialStateMachine, Warning, TEXT("ADropletPlayerCharacter::SetMaterialState: eNewMaterialState is EDropletMaterialState_None"));
	}
}

ADropletPlayerCharacter::ADropletPlayerCharacter()
{
	// Create the InteractableRangeShapeComponent
	pInteractableRangeSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("InteractableRangeShapeComponent"));

	// Then attach it to the root component
	pInteractableRangeSphereComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	pInteractableRangeSphereComponent->bSelectable = true;
	pInteractableRangeSphereComponent->bEditableWhenInherited = true;

	RemoveOwnedComponent(pInteractableRangeSphereComponent);
	pInteractableRangeSphereComponent->CreationMethod = EComponentCreationMethod::Instance;
	AddOwnedComponent(pInteractableRangeSphereComponent);
}

void ADropletPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Reassign the pInteractableRangeSphereComponent cause the pInteractableRangeSphereComponent
	// seems to not be the same as the one created in the constructor
	pInteractableRangeSphereComponent = FindComponentByClass<USphereComponent>();
}

void ADropletPlayerCharacter::Tick(float fDeltaTime)
{
	Super::Tick(fDeltaTime);

	//If the DropletPlayerController is not registered
	if (m_pDropletPlayerController == nullptr)
	{
		m_pDropletPlayerController = Cast<ADropletPlayerController>(GetController());
		UE_LOG(LogTemp, Warning, TEXT("ADropletPlayerCharacter::Tick: m_pDropletPlayerController is not registered!"));
	}


	// If the interactable debug is enabled
	if (m_bIsInteractablesDebugEnabled)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.0f,
			FColor::White,
			FString::Printf(
				TEXT("InteractableMarker: %s"), BPF_HasBreakerMarker() ?
				TEXT("BREAKER") : BPF_HasDrillerMarker() ? TEXT("DRILLER") : TEXT("NONE")
			)
		);
	}

	if (m_bInteractableMarkerDisplayed)
	{
		BPE_OnInteractableMarkersDisplayed();
	}


	// Check and handle if we have to display interaction actions
	HandleInteractionButtonDisplay();


	// If we are in slide dash breaking
	if (m_bIsSlideDashBreaking)
	{
		m_fSlideDashBreakElapsedTime += fDeltaTime;

		if (m_fSlideDashBreakElapsedTime >= m_fSlideDashBreakerStateDuration)
		{
			m_bIsSlideDashBreaking = false;
			m_fSlideDashBreakElapsedTime = 0.f;

			// Remove the Breaker marker from the InteractableMarker array of the character
			RemoveInteractableMarker<UBreakerInteractableMarker>();
		}
	}

	// If we have the Driller marker
	if (HasInteractableMarker<UDrillerInteractableMarker>())
	{
		m_fDrillerElapsedTime += fDeltaTime;

		if (m_fDrillerElapsedTime >= m_fDrillerStateDuration)
		{
			m_fDrillerElapsedTime = 0.f;
			// Remove the Driller marker from the InteractableMarker array of the character
			RemoveInteractableMarker<UDrillerInteractableMarker>();
		}
	}

	//If the speed component is valid
	if (m_pSpeedComponent != nullptr)
	{
		//Get CharacterMovementComponent
		UCharacterMovementComponent* pCharacterMovementComponent = GetCharacterMovement();

		//If the character movement is NOT valid
		if (pCharacterMovementComponent == nullptr)
		{
			//Log error
			UE_LOG(LogTemp, Error, TEXT("ADropletPlayerCharacter::Tick: pCharacterMovementComponent is nullptr!"));
			return;
		}

		// If we are splashing
		if (m_bIsSplashing)
		{
			if (m_fSplashElapsedTime < m_pSpeedComponent->m_fSplashDuration)
			{
				float fCurveValue = m_pSpeedComponent->m_fCurveSplashSpeedBoost->GetFloatValue(m_fSplashElapsedTime / m_pSpeedComponent->m_fSplashDuration);

				// Compute the speed for this frame
				float fSpeed = m_fSplashStartSpeed + (m_fSplashTargetSpeed * fCurveValue);
				fSpeed = FMath::Max(fSpeed, 0.f);

				pCharacterMovementComponent->Velocity = m_vSplashDirection * fCurveValue * m_fSplashTargetSpeed;

				m_fSplashElapsedTime += fDeltaTime;

				return;
			}
			else
			{
				m_bIsSplashing = false;
				m_fSplashElapsedTime = 0.f;
			}
		}

		//If we are grounded
		if (pCharacterMovementComponent->IsMovingOnGround())
		{
			m_bCanSplash = false;

			// If we are slide dashing continue computing the velocity
			if (m_bIsSlideDashing)
			{
				if (m_fTransitionSpeedBoostElapsedTime < m_pSpeedComponent->m_fLiquidToSolidSpeedBoostDuration)
				{
					float fCurveValue = m_pSpeedComponent->m_fCurveLiquidToSolidSpeedBoost->
						GetFloatValue(m_fTransitionSpeedBoostElapsedTime / m_pSpeedComponent->m_fLiquidToSolidSpeedBoostDuration);

					pCharacterMovementComponent->Velocity = m_vTransitionSpeedBoostStartVelocity +
						pCharacterMovementComponent->Velocity.GetSafeNormal() * fCurveValue * m_fTransitionSpeedBoostTarget;

					m_fTransitionSpeedBoostElapsedTime += fDeltaTime;

					return;
				}
				else
				{
					m_bIsSlideDashing = false;
					m_fTransitionSpeedBoostElapsedTime = 0.f;
				}
			}

			FHitResult hitResult;
			FVector vHitNormal;

			//Adapt speed depending on the slope angle if we are not on a flat surface ---------------------------
			float fMaxAscendingSpeed = 0.f;
			float fMinAscendingSpeed = 0.f;
			float fMaxDescendingSpeed = pCharacterMovementComponent->MaxWalkSpeed;
			float fMaxFlatSpeed = pCharacterMovementComponent->MaxWalkSpeed;
			float fAscendingFactor = 0.f;
			float fAscendingFactorEmptyStamina = 0.f;
			float fDescendingFactor = 0.f;

			//Switch on the material state
			EDropletMaterialState eMaterialState = m_pDropletPlayerController->GetMaterialState();
			switch (eMaterialState)
			{
			case EDropletMaterialState::EDropletMaterialState_Liquid:
				fMaxAscendingSpeed = m_pSpeedComponent->GetSpeedAscendingMaxLiquid();
				fMinAscendingSpeed = m_pSpeedComponent->GetSpeedAscendingMinLiquid();
				fMaxDescendingSpeed = m_pSpeedComponent->GetSpeedDescendingMaxLiquid();
				fMaxFlatSpeed = m_pSpeedComponent->m_fSpeedFlatMaxLiquid;
				fAscendingFactor = m_pSpeedComponent->m_fAscendingFactorLiquid;
				fAscendingFactorEmptyStamina = m_pSpeedComponent->m_fAscendingFactorLiquidEmptyStamina;
				fDescendingFactor = m_pSpeedComponent->m_fDescendingFactorLiquid;
				break;
			case EDropletMaterialState::EDropletMaterialState_Solid:
				fMaxAscendingSpeed = m_pSpeedComponent->GetSpeedAscendingMaxSolid();
				fMaxDescendingSpeed = m_pSpeedComponent->GetSpeedDescendingMaxSolid();
				fMaxFlatSpeed = m_pSpeedComponent->m_fSpeedFlatMaxSolid;
				fAscendingFactor = m_pSpeedComponent->m_fAscendingFactorSolid;
				fDescendingFactor = m_pSpeedComponent->m_fDescendingFactorSolid;
				break;
			case EDropletMaterialState::EDropletMaterialState_Gazeous:
				break;
			default:
			case EDropletMaterialState::EDropletMaterialState_None:
				//Log error
				UE_LOG(LogTemp, Error, TEXT("ADropletPlayerCharacter::Tick: eMaterialState is EDropletMaterialState_None"));
				break;
			}

			// Debug print current MaxWalkSpeed
			if (m_pSpeedComponent->m_bAreDebugMessagesEnabled)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, FString::Printf(TEXT("MaxWalkSpeed: %f"), pCharacterMovementComponent->MaxWalkSpeed));
			}

			//If we are on a slope
			float fSlopeAngle = GetSlopeAngle(hitResult, vHitNormal);
			if (fSlopeAngle >= m_fFlatSurfaceTolerance)
			{
				bool bIsAscending = false;
				bool bIsStaminaEmty = false;
				bool bIsTargetSpeedSuperior = false;

				//Ascending
				if (IsAscending())
				{
					bIsAscending = true;
					// If we are in liquid state
					if (m_pDropletPlayerController->GetMaterialState() == EDropletMaterialState::EDropletMaterialState_Liquid)
					{
						// If the stamina component is valid
						if (UStaminaComponent* staminaComponent = Cast<UStaminaComponent>(GetComponentByClass(UStaminaComponent::StaticClass())))
						{
							//If the stamina is empty AND the slope is inferior to max slope, we can move but slower on the slope
							if (fSlopeAngle < m_fMaxSlopeAngle && staminaComponent->GetCurrentStamina() <= 0.f)
							{
								m_fTargetMaxSpeed = fMinAscendingSpeed;
								bIsStaminaEmty = true;
							}
							// Else if it's not empty
							else
							{
								m_fTargetMaxSpeed = FMath::Max(fMaxAscendingSpeed, fMinAscendingSpeed);
							}
						}
					}
					// In other state we can move at the max speed
					// (NOTE: if no stamina it's handled in the stamina component's TickComponent method)
					else
					{
						m_fTargetMaxSpeed = fMaxAscendingSpeed;
					}


					if (pCharacterMovementComponent->MaxWalkSpeed < m_fTargetMaxSpeed)
					{
						bIsTargetSpeedSuperior = true;
						pCharacterMovementComponent->MaxWalkSpeed = bIsStaminaEmty ? fMinAscendingSpeed : fMaxAscendingSpeed;
					}


					pCharacterMovementComponent->MaxWalkSpeed += bIsStaminaEmty ?
						fAscendingFactor * FMath::Sign(m_fTargetMaxSpeed - pCharacterMovementComponent->MaxWalkSpeed) :
						fAscendingFactorEmptyStamina * FMath::Sign(m_fTargetMaxSpeed - pCharacterMovementComponent->MaxWalkSpeed);


					float fMin = bIsTargetSpeedSuperior ? 0.f : m_fTargetMaxSpeed;
					float fMax = bIsTargetSpeedSuperior ? m_fTargetMaxSpeed : pCharacterMovementComponent->MaxWalkSpeed;

					pCharacterMovementComponent->MaxWalkSpeed = FMath::Clamp(pCharacterMovementComponent->MaxWalkSpeed, fMin, fMax);
				}
				//Descending
				else if (pCharacterMovementComponent->Velocity.Length() > m_fVelocityMovingTolerance)
				{
					m_fTargetMaxSpeed = fMaxDescendingSpeed;

					if (pCharacterMovementComponent->MaxWalkSpeed < m_fTargetMaxSpeed)
					{
						bIsTargetSpeedSuperior = true;
					}
					else
					{
						pCharacterMovementComponent->MaxWalkSpeed = fMaxDescendingSpeed;
					}

					pCharacterMovementComponent->MaxWalkSpeed += fDescendingFactor * FMath::Sign(m_fTargetMaxSpeed - pCharacterMovementComponent->MaxWalkSpeed);

					float fMin = bIsTargetSpeedSuperior ? 0.f : m_fTargetMaxSpeed;
					float fMax = bIsTargetSpeedSuperior ? m_fTargetMaxSpeed : pCharacterMovementComponent->MaxWalkSpeed;

					pCharacterMovementComponent->MaxWalkSpeed = FMath::Clamp(pCharacterMovementComponent->MaxWalkSpeed, fMin, fMax);
				}

			}
			//If we are on a flat surface
			else
			{
				pCharacterMovementComponent->MaxWalkSpeed = fMaxFlatSpeed;
			}
		}
		//If we are NOT grounded
		else
		{
			//If the character is falling
			if (GetCharacterMovement()->IsFalling())
			{
				// If we just changed state and the previous state was not the gazeous state, reapply falling speed
				if (m_bJustChangedState && m_pDropletPlayerController->GetPreviousMaterialState() != EDropletMaterialState::EDropletMaterialState_Gazeous)
				{
					GetCharacterMovement()->Velocity.Z = m_fCurrentFallingSpeed;
				}

				// If the character is far enough from the ground, he can splash
				FHitResult hit;
				m_bCanSplash = !GetHitLineTracedUnder(hit, FVector::ZeroVector, m_fSplashDistanceToGroundThreshold);

				//If the character is falling faster than the max falling speed
				if (GetCharacterMovement()->Velocity.Z < -m_pSpeedComponent->m_fSpeedFallMax)
				{
					//Set it back to the max falling speed
					GetCharacterMovement()->Velocity.Z = -m_pSpeedComponent->m_fSpeedFallMax;
				}

				m_fCurrentFallingSpeed = GetCharacterMovement()->Velocity.Z;
			}
		}

		// If we are under oil effect
		if (m_bIsUnderOilEffect)
		{
			pCharacterMovementComponent->MaxWalkSpeed = pCharacterMovementComponent->MaxWalkSpeed * m_fOilSpeedFactor;
		}

		m_bJustChangedState = false;
	}
	//If the speed component is NOT valid
	else
	{
		//Log warning
		UE_LOG(LogTemp, Warning, TEXT("ADropletPlayerCharacter::Tick: speed componennt is nullptr!"));
	}
}

void ADropletPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//ChangeState
		EnhancedInputComponent->BindAction(ChangeState, ETriggerEvent::Triggered, this, &ADropletPlayerCharacter::ChangeMaterialState);

		// Interact
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Completed, this, &ADropletPlayerCharacter::Interact);
	}
}

void ADropletPlayerCharacter::Move(const FInputActionValue& Value)
{
	//If the move function is NOT bound, return
	if (!m_MoveFunction.IsBound())
	{
		return;
	}

	// If we are slide dashing or splashing, return
	if (m_bIsSlideDashing || m_bIsSplashing)
	{
		return;
	}

	// If the stamina component is valid
	if (UStaminaComponent* staminaComponent = Cast<UStaminaComponent>(GetComponentByClass(UStaminaComponent::StaticClass())))
	{
		//If the stamina is empty
		if (staminaComponent->GetCurrentStamina() <= 0.f)
		{
			//If we are not in gazeous state
			if (m_pDropletPlayerController->GetMaterialState() != EDropletMaterialState::EDropletMaterialState_Gazeous)
			{
				UDropletMaterialStateDescription* pDescription = UDropletMaterialStateDescription::GetMaterialStateDescriptionFromState(m_pDropletPlayerController->GetMaterialState());
				FVector vDirection = pDescription->GetMovementDirection(Value);

				bool bIsTryingToGoDownTheSlope = false;

				FHitResult hitResult;
				FVector vHitNormal;

				//If we are on a slope
				float fSlopeAngle = GetSlopeAngle(hitResult, vHitNormal);
				if (fSlopeAngle >= m_fFlatSurfaceTolerance)
				{
					vHitNormal.Z = 0.f;

					bIsTryingToGoDownTheSlope = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(vHitNormal, vDirection))) < 90.f;
				}

				//If we are not trying to go down a slope
				if (!bIsTryingToGoDownTheSlope)
				{
					// If we are NOT in liquid state
					if (m_pDropletPlayerController->GetMaterialState() != EDropletMaterialState::EDropletMaterialState_Liquid)
					{
						// Log 
						UE_LOG(LogTemp, Log, TEXT("ADropletPlayerCharacter::Move: stamina is empty!"));
						return;
					}
					// Else if we are in liquid state but the slope is superior to max slope angle
					else if (fSlopeAngle >= m_fMaxSlopeAngle)
					{
						// Log 
						UE_LOG(LogTemp, Log, TEXT("ADropletPlayerCharacter::Move: stamina is empty and we are on a slope!"));
						//m_MoveFunction.Execute(Value);
						return;
					}
				}
			}
		}
	}

	//Execute the move function
	m_MoveFunction.Execute(Value);
}

void ADropletPlayerCharacter::Jump()
{
	if (m_bIsSplashing)
	{
		return;
	}

	// If the stamina component is valid
	if (TObjectPtr<UStaminaComponent> pStaminaComponent = Cast<UStaminaComponent>(GetComponentByClass(UStaminaComponent::StaticClass())))
	{
		// If not enough stamina
		if (pStaminaComponent->GetCurrentStamina() < pStaminaComponent->GetJumpStaminaCost() * pStaminaComponent->GetMaxStamina())
		{
			return;
		}
		// Else if we're not holding the jump yet and touching the ground, drain the stamina
		else if (JumpKeyHoldTime == 0.f && GetCharacterMovement()->IsMovingOnGround())
		{
			// If infinite stamina is disabled
			if (!pStaminaComponent->m_bIsInfiniteStaminaEnabled)
			{
				// Drain the stamina
				pStaminaComponent->DrainJumpStamina();
			}
		}
	}
	else
	{
		//Log warning
		UE_LOG(LogTemp, Warning, TEXT("ADropletPlayerCharacter::Jump: stamina component is nullptr!"));
	}

	Super::Jump();
}

void ADropletPlayerCharacter::Interact()
{
	// If the controller is NOT valid, return
	if (!m_pDropletPlayerController)
	{
		UE_LOG(LogInteractable, Error, TEXT("ADropletPlayerCharacter::Interact: m_pDropletPlayerController is nullptr"));

		return;
	}

	if (pInteractableRangeSphereComponent == nullptr)
	{
		if (m_bIsInteractablesDebugEnabled)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("ADropletPlayerCharacter::Interact: pInteractableRangeSphereComponent is nullptr"));
		}

		UE_LOG(LogInteractable, Error, TEXT("ADropletPlayerCharacter::Interact: pInteractableRangeSphereComponent is nullptr"));

		return;
	}

	// Get overlapping actors in range of the InteractableRangeShapeComponent
	TArray<TObjectPtr<AActor>> OverlappingActors;
	pInteractableRangeSphereComponent->GetOverlappingActors(OverlappingActors, AActor::StaticClass());

	// If there are actors in range of the InteractableRangeShapeComponent
	if (!OverlappingActors.IsEmpty())
	{
		TArray<TObjectPtr<UInputInteractableActorComponent>> interactableComponents;

		// For each actor
		for (AActor* pActor : OverlappingActors)
		{
			// Ignore itself
			if (pActor == this)
			{
				continue;
			}

			// If the actor has an InputInteractableActorComponent && its root component is overlapping
			TObjectPtr<UInputInteractableActorComponent> pInputInteractableComponent = pActor->FindComponentByClass<UInputInteractableActorComponent>();
			if (pInputInteractableComponent != nullptr && pInteractableRangeSphereComponent->IsOverlappingComponent(CastChecked<UPrimitiveComponent>(pActor->GetRootComponent())))
			{
				// If we are in the range of the component
				if (pInputInteractableComponent->IsActorInRange(this))
				{
					// Add it to the array
					interactableComponents.Add(pInputInteractableComponent);
				}
				else
				{
					if (m_bIsInteractablesDebugEnabled)
					{
						// Add debug to screen
						GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::Printf(TEXT("ADropletPlayerCharacter::Interact: not in range to interact with %s"), *pActor->GetName()));
					}
				}
			}
		}

		// If we have interactable components in range
		if (!interactableComponents.IsEmpty())
		{
			// Sort the array by distance of their owner's root component to this character
			interactableComponents.Sort([&](const UInputInteractableActorComponent& A, const UInputInteractableActorComponent& B) {
				return FVector::DistSquared(A.GetOwner()->GetRootComponent()->GetComponentLocation(), GetRootComponent()->GetComponentLocation()) < FVector::DistSquared(B.GetOwner()->GetRootComponent()->GetComponentLocation(), GetRootComponent()->GetComponentLocation());
				});


			if (m_bIsInteractablesDebugEnabled)
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::Printf(TEXT("ADropletPlayerCharacter::Interact: interact with %s"), *interactableComponents[0]->GetOwner()->GetName()));

				FVector loc = interactableComponents[0]->GetOwner()->GetActorLocation();
				DrawDebugLine(
					GetWorld(),
					loc,
					loc + FVector::UpVector * 250.f,
					FColor(255, 255, 0),
					false, 1.f, 0,
					20
				);
			}

			// If the first component is active, interact with it
			if (interactableComponents[0]->IsActive())
			{
				interactableComponents[0]->Interact(this);
			}
			// Else interact with the next component if it exists and is active
			else if (interactableComponents.IsValidIndex(1) && interactableComponents[1]->IsActive())
			{
				interactableComponents[1]->Interact(this);
			}
			// Else interact with the first component in the array 
			else
			{
				interactableComponents[0]->Interact(this);
			}
		}
	}
}

void ADropletPlayerCharacter::HandleInteractionButtonDisplay()
{
	if (pInteractableRangeSphereComponent == nullptr)
	{
		if (m_bIsInteractablesDebugEnabled)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("ADropletPlayerCharacter::HandleInteractActionDisplay: pInteractableRangeSphereComponent is nullptr"));
		}

		UE_LOG(LogInteractable, Error, TEXT("ADropletPlayerCharacter::HandleInteractActionDisplay: pInteractableRangeSphereComponent is nullptr"));

		return;
	}

	// Get overlapping actors in range of the InteractableRangeShapeComponent
	TArray<TObjectPtr<AActor>> OverlappingActors;
	pInteractableRangeSphereComponent->GetOverlappingActors(OverlappingActors, AActor::StaticClass());

	// If there are actors in range of the InteractableRangeShapeComponent
	if (!OverlappingActors.IsEmpty())
	{
		TArray<TObjectPtr<UInputInteractableActorComponent>> interactableComponents;
		TArray<TObjectPtr<UInputInteractableActorComponent>> nonInteractableComponents;

		// For each actor
		for (AActor* pActor : OverlappingActors)
		{
			// Ignore itself
			if (pActor == this)
			{
				continue;
			}

			// If the actor has an InputInteractableActorComponent && its root component is overlapping
			TObjectPtr<UInputInteractableActorComponent> pInputInteractableComponent = pActor->FindComponentByClass<UInputInteractableActorComponent>();
			if (pInputInteractableComponent != nullptr && pInteractableRangeSphereComponent->IsOverlappingComponent(CastChecked<UPrimitiveComponent>(pActor->GetRootComponent())))
			{
				// If we are in the range of the component
				// and not both in gazeous state and the component is an input dialogue component
				bool bGazeous = m_pDropletPlayerController->GetMaterialState() == EDropletMaterialState::EDropletMaterialState_Gazeous;
				bool bIsInputDialogueComponent = pActor->FindComponentByClass<UVeinDialogueActorComponent>() != nullptr;
				if (!(bGazeous && bIsInputDialogueComponent) && pInputInteractableComponent->IsActorInRange(this))
				{
					// Add it to the array
					interactableComponents.Add(pInputInteractableComponent);
				}
				else
				{
					nonInteractableComponents.Add(pInputInteractableComponent);
				}
			}
			// If the actor has an InputInteractableActorComponent && its root component is not overlapping
			else if (pInputInteractableComponent != nullptr)
			{
				nonInteractableComponents.Add(pInputInteractableComponent);
			}
		}

		// If we have interactable components in range
		if (!interactableComponents.IsEmpty())
		{
			// Sort the array by distance of their owner's root component to this character
			interactableComponents.Sort([&](const UInputInteractableActorComponent& A, const UInputInteractableActorComponent& B) {
				return FVector::DistSquared(A.GetOwner()->GetRootComponent()->GetComponentLocation(), GetRootComponent()->GetComponentLocation()) < FVector::DistSquared(B.GetOwner()->GetRootComponent()->GetComponentLocation(), GetRootComponent()->GetComponentLocation());
				});

			// Check for the first interactable component active in range
			int iFirstActiveIndex = -1;
			for (int i = 0; i < interactableComponents.Num(); ++i)
			{
				if (interactableComponents[i]->IsActive())
				{
					// Add debug to screen
					if (m_bIsInteractablesDebugEnabled)
					{
						GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::Printf(TEXT("ADropletPlayerCharacter::HandleInteractActionDisplay: interact with %s"), *interactableComponents[iFirstActiveIndex]->GetOwner()->GetName()));
					}

					iFirstActiveIndex = i;
					break;
				}
			}

			if (m_bIsInteractablesDebugEnabled)
			{
				FVector loc = interactableComponents[0]->GetOwner()->GetActorLocation();
				DrawDebugLine(
					GetWorld(),
					loc,
					loc + FVector::UpVector * 200.f,
					FColor(0, 255, 0),
					false, -1.f, 0,
					12.333
				);
			}

			// Register the character to the first active interactable component if any
			if (iFirstActiveIndex > -1 && iFirstActiveIndex < interactableComponents.Num() &&
				m_pDropletPlayerController->GetMaterialState() != EDropletMaterialState::EDropletMaterialState_Gazeous)
			{
				interactableComponents[iFirstActiveIndex]->RegisterCharacterForInteraction(this);
			}

			// Unregister the character from the other interactable components
			for (int i = 0; i < interactableComponents.Num(); ++i)
			{
				if (i == iFirstActiveIndex && m_pDropletPlayerController != nullptr &&
					m_pDropletPlayerController->GetMaterialState() != EDropletMaterialState::EDropletMaterialState_Gazeous)
				{
					continue;
				}

				interactableComponents[i]->UnregisterCharacter(this);
			}
		}

		// Unregister the character from the non-interactable components
		for (UInputInteractableActorComponent* pNonInteractableComponent : nonInteractableComponents)
		{
			pNonInteractableComponent->UnregisterCharacter(this);
		}
	}
}

void ADropletPlayerCharacter::ChangeMaterialState(const FInputActionValue& Value)
{
	//If the controller is NOT valid, return
	if (!m_pDropletPlayerController)
	{
		return;
	}

	if (Value.IsNonZero())
	{
		FVector VectorValue = Value.Get<FVector>();

		//Change the material state
		m_pDropletPlayerController->PlayerChangeMaterialState(VectorValue.X > 0 ? false : true);
	}
}

void ADropletPlayerCharacter::PossessedBy(AController* pNewController)
{
	Super::PossessedBy(pNewController);

	//If the controller is a ADropletPlayerController
	if (ADropletPlayerController* castedController = CastChecked<ADropletPlayerController>(pNewController))
	{
		m_pDropletPlayerController = castedController;
	}
}

void ADropletPlayerCharacter::Splash(const FHitResult& Hit)
{
	FVector vHitNormal = Hit.ImpactNormal;

	// If the CharacterMovementComponent is valid
	TObjectPtr<UCharacterMovementComponent> pCharacterMovement = GetCharacterMovement();
	if (pCharacterMovement != nullptr)
	{
		// Get the angle between the hit normal and the character velocity
		float fAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(vHitNormal, -pCharacterMovement->Velocity.GetSafeNormal())));

		// Get the character direction
		FVector vCharacterDirection = pCharacterMovement->Velocity;
		// Remove the Z coordinate
		vCharacterDirection.Z = 0.f;

		// Get the angle between the character direction and the hit normal
		float fAngleBetweenCharacterDirectionAndHitNormal = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(vHitNormal, vCharacterDirection.GetSafeNormal())));

		// Compute the splash direction to which the character will be boosted
		m_vSplashDirection = vCharacterDirection.RotateAngleAxis(90.f - fAngleBetweenCharacterDirectionAndHitNormal, FVector::UpVector.Cross(vCharacterDirection).GetSafeNormal());
		m_vSplashDirection = m_vSplashDirection.GetSafeNormal();

		if (m_bIsSplashDebugDrawLineEnabled)
		{
			// Draw a debug line to show the splash direction
			DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + m_vSplashDirection * 100.f, FColor::Green, false, 3.f, 0, 12.333f);
			DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + vCharacterDirection * 100.f, FColor::Blue, false, 3.f, 0, 12.333f);
			DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + FVector::UpVector.Cross(vCharacterDirection) * 1000.f, FColor::Purple, false, 3.f, 0, 12.333f);
		}

		// If the SpeedComponent is valid
		if (m_pSpeedComponent != nullptr)
		{
			bool bIsAscending = m_vSplashDirection.Z > 0.f;

			// If the angle is within the the failure range or we are ascending
			if (fAngle < 90.f - m_pSpeedComponent->m_fSplashAngleFailureThreshold || bIsAscending)
			{
				m_fSplashTargetSpeed = pCharacterMovement->Velocity.Size() - pCharacterMovement->Velocity.Size() * m_pSpeedComponent->m_fSplashSpeedBoostFactor;

				m_bIsSplashing = true;

				if (m_pSpeedComponent->m_bAreDebugMessagesEnabled)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Splash angle: %f"), 90.f - fAngle));
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Splash speed boost: %f"), m_fSplashTargetSpeed));
				}
			}
			// Else if the angle is within the success range
			else if (fAngle >= 90.f - m_pSpeedComponent->m_fSplashAngleSuccessThreshold)
			{
				m_fSplashStartSpeed = pCharacterMovement->Velocity.Size();
				m_fSplashTargetSpeed = pCharacterMovement->Velocity.Size() + pCharacterMovement->Velocity.Size() * m_pSpeedComponent->m_fSplashSpeedBoostFactor;

				m_bIsSplashing = true;

				if (m_pSpeedComponent->m_bAreDebugMessagesEnabled)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Splash angle: %f"), 90.f - fAngle));
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Splash speed boost: %f"), m_fSplashTargetSpeed));
				}
			}
			else if (m_pSpeedComponent->m_bAreDebugMessagesEnabled)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("Splash angle: %f"), 90.f - fAngle));
			}
		}
	}
}

void ADropletPlayerCharacter::ChangeInputMappingContext(EDropletMaterialState eNewMaterialState)
{
	UInputMappingContext* pInputMappingContext = nullptr;

	//Switch on the material state
	switch (eNewMaterialState)
	{
		//If the material state is solid
	case EDropletMaterialState::EDropletMaterialState_Solid:
	{
		//Change the input mapping context to solid
		pInputMappingContext = SolidMappingContext;
		break;
	}
	//If the material state is liquid
	case EDropletMaterialState::EDropletMaterialState_Liquid:
	{
		//Change the input mapping context to liquid
		pInputMappingContext = LiquidMappingContext;
		break;
	}
	//If the material state is gas
	case EDropletMaterialState::EDropletMaterialState_Gazeous:
	{
		//Change the input mapping context to gazeous
		pInputMappingContext = GazeousMappingContext;
		break;
	}
	//If the material state is none
	default:
	case EDropletMaterialState::EDropletMaterialState_None:
	{
		//Change the input mapping context to default
		pInputMappingContext = DefaultMappingContext;
		break;
	}
	}

	//If the droplet player controller is valid
	if (m_pDropletPlayerController != nullptr)
	{
		//If the local player is valid
		if (ULocalPlayer* localPlayer = m_pDropletPlayerController->GetLocalPlayer())
		{
			//If the subsystem is valid
			if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(localPlayer))
			{
				//Change the input mapping context
				subsystem->ClearAllMappings();
				subsystem->AddMappingContext(pInputMappingContext, 0);
			}
		}
	}
	else
	{
		UE_LOG(LogMaterialStateMachine, Error, TEXT("ADropletPlayerCharacter::ChangeInputMappingContext: m_pDropletPlayerController is nullptr"));
	}
}

void ADropletPlayerCharacter::ChangeSkeletalMeshInstance(EDropletMaterialState eNewMaterialState)
{
	USkeletalMesh* pSKInstance = nullptr;

	//Switch on the material state
	switch (eNewMaterialState)
	{
		//If the material state is solid
	case EDropletMaterialState::EDropletMaterialState_Solid:
	{
		//Change the material instance to solid
		pSKInstance = m_pSolidSKInstance;
		break;
	}
	default:
		//If the material state is none
	case EDropletMaterialState::EDropletMaterialState_None:
		//If the material state is liquid
	case EDropletMaterialState::EDropletMaterialState_Liquid:
	{
		//Change the material instance to liquid
		pSKInstance = m_pLiquidSKInstance;
		break;
	}
	//If the material state is gas
	case EDropletMaterialState::EDropletMaterialState_Gazeous:
	{
		//Change the material instance to gazeous
		//pSKInstance = m_pGazeousSKInstance;

		//Do not change the mesh
		//Make it invisible
		this->GetMesh()->SetVisibility(false);

		break;
	}
	}


	//Return if in gazeous state
	if (eNewMaterialState == EDropletMaterialState::EDropletMaterialState_Gazeous)
	{
		return;
	}

	//Change Character mesh
	//If the mesh is valid
	if (USkeletalMeshComponent* pMeshComponent = this->GetMesh())
	{
		//If the mesh instance is valid
		if (pSKInstance != nullptr)
		{
			//Change the mesh
			pMeshComponent->SetSkeletalMesh(pSKInstance);

			//Make it visible
			pMeshComponent->SetVisibility(true);
		}
		else
		{
			UE_LOG(LogMaterialStateMachine, Error, TEXT("ADropletPlayerCharacter::ChangeSkeletalMeshInstance: pSKInstance is nullptr"));
		}
	}
	else
	{
		UE_LOG(LogMaterialStateMachine, Error, TEXT("ADropletPlayerCharacter::ChangeSkeletalMeshInstance: pMeshComponent is nullptr"));
	}
}

void ADropletPlayerCharacter::ChangeMeshMaterialInstance(EDropletMaterialState eNewMaterialState)
{
	UMaterialInstance* pMaterialInstance = nullptr;

	//Switch on the material state
	switch (eNewMaterialState)
	{
		//If the material state is solid
	case EDropletMaterialState::EDropletMaterialState_Solid:
	{
		//Change the material instance to solid
		pMaterialInstance = m_pSolidMaterialInstance;
		break;
	}
	default:
		//If the material state is none
	case EDropletMaterialState::EDropletMaterialState_None:
		//If the material state is liquid
	case EDropletMaterialState::EDropletMaterialState_Liquid:
	{
		//Change the material instance to liquid
		pMaterialInstance = m_pLiquidMaterialInstance;
		break;
	}
	//If the material state is gas
	case EDropletMaterialState::EDropletMaterialState_Gazeous:
	{
		//Change the material instance to gazeous
		pMaterialInstance = m_pGazeousMaterialInstance;
		break;
	}
	}

	//Change Character mesh's material
	//If the mesh is valid
	if (USkeletalMeshComponent* pMeshComponent = this->GetMesh())
	{
		//If the material instance is valid
		if (pMaterialInstance != nullptr)
		{
			//Change the material instance
			pMeshComponent->SetMaterial(0, pMaterialInstance);
		}
		else
		{
			UE_LOG(LogMaterialStateMachine, Error, TEXT("ADropletPlayerCharacter::ChangeMeshMaterialInstance: pMaterialInstance is nullptr"));
		}
	}
	else
	{
		UE_LOG(LogMaterialStateMachine, Error, TEXT("ADropletPlayerCharacter::ChangeMeshMaterialInstance: pMeshComponent is nullptr"));
	}
}

void ADropletPlayerCharacter::ChangeStaminaComponent(EDropletMaterialState eNewMaterialState)
{
	UStaminaComponent* pStaminaComponent = Cast<UStaminaComponent>(GetComponentByClass(UStaminaComponent::StaticClass()));
	UStaminaComponent* pNewStaminaComponent = nullptr;

	float fStamina = 100.f;
	bool bIsInfiniteStaminaEnabled = false;
	bool bAreDebugMessagesEnabled = false;
	bool bShowDebugStaminaBar = false;

	// If StaminaComponent is nullptr, log error
	if (pStaminaComponent == nullptr)
	{
		UE_LOG(LogStamina, Warning, TEXT("ADropletPlayerCharacter::ChangeStaminaComponent: staminaComponent is nullptr"));
	}
	// If StaminaComponent is not nullptr, register current stamina and debug flags, then destroy it
	else
	{
		fStamina = pStaminaComponent->GetCurrentStamina();

		bIsInfiniteStaminaEnabled = pStaminaComponent->m_bIsInfiniteStaminaEnabled;
		bAreDebugMessagesEnabled = pStaminaComponent->m_bAreDebugMessagesEnabled;
		bShowDebugStaminaBar = pStaminaComponent->m_bShowDebugStaminaBar;

		pStaminaComponent->DestroyComponent();
	}

	switch (eNewMaterialState)
	{
	case EDropletMaterialState::EDropletMaterialState_Liquid:
		pNewStaminaComponent = CastChecked<UStaminaComponent>(AddComponentByClass(LiquidStaminaComponent, false, FTransform::Identity, false));
		break;
	case EDropletMaterialState::EDropletMaterialState_Solid:
		pNewStaminaComponent = CastChecked<UStaminaComponent>(AddComponentByClass(SolidStaminaComponent, false, FTransform::Identity, false));
		break;
	case EDropletMaterialState::EDropletMaterialState_Gazeous:
		pNewStaminaComponent = CastChecked<UStaminaComponent>(AddComponentByClass(GazeousStaminaComponent, false, FTransform::Identity, false));
		break;
	default:
	case EDropletMaterialState::EDropletMaterialState_None:
		// Log error
		UE_LOG(LogStamina, Error, TEXT("ADropletPlayerCharacter::ChangeStaminaComponent: eNewMaterialState is NONE!"));
		break;
	}

	// If new stamina component is not nullptr, set debug flags
	if (pNewStaminaComponent != nullptr)
	{
		pNewStaminaComponent->SetCurrentStamina(fStamina);

		pNewStaminaComponent->m_bIsInfiniteStaminaEnabled = bIsInfiniteStaminaEnabled;
		pNewStaminaComponent->m_bAreDebugMessagesEnabled = bAreDebugMessagesEnabled;
		pNewStaminaComponent->m_bShowDebugStaminaBar = bShowDebugStaminaBar;
	}
}

void ADropletPlayerCharacter::ApplySpeedComponentStateValues(EDropletMaterialState eNewMaterialState)
{
	// If SpeedComponent is nullptr, add it
	if (m_pSpeedComponent == nullptr)
	{
		m_pSpeedComponent = CastChecked<USpeedComponent>(AddComponentByClass(SpeedComponent, false, FTransform::Identity, false));
		UE_LOG(LogSpeed, Warning, TEXT("ADropletPlayerCharacter::ApplySpeedComponentStateValues: SpeedComponent added"));
	}

	UCharacterMovementComponent* pCharacterMovement = GetCharacterMovement();

	// Switch on the material state to apply the correct values
	switch (eNewMaterialState)
	{
	case EDropletMaterialState::EDropletMaterialState_Liquid:
		pCharacterMovement->MaxWalkSpeed = m_pSpeedComponent->m_fSpeedFlatMaxLiquid;
		pCharacterMovement->MaxAcceleration = m_pSpeedComponent->m_fAccelerationFlatLiquid;
		break;
	case EDropletMaterialState::EDropletMaterialState_Solid:
		pCharacterMovement->MaxWalkSpeed = m_pSpeedComponent->m_fSpeedFlatMaxSolid;
		pCharacterMovement->MaxAcceleration = m_pSpeedComponent->m_fAccelerationFlatSolid;
		m_bIsSlideDashing = true;
		break;
	case EDropletMaterialState::EDropletMaterialState_Gazeous:
		pCharacterMovement->MaxFlySpeed = m_pSpeedComponent->m_fSpeedMaxGazeous;
		pCharacterMovement->MaxAcceleration = m_pSpeedComponent->m_fAccelerationGazeous;
		m_bIsGazeousDashing = true;
		break;
	default:
	case EDropletMaterialState::EDropletMaterialState_None:
		// Log error
		UE_LOG(LogSpeed, Error, TEXT("ADropletPlayerCharacter::ApplySpeedComponentStateValues: eNewMaterialState is NONE!"));
		break;
	}

	// If the character is on the ground and is in slide dash
	if (m_bIsSlideDashing && pCharacterMovement->IsMovingOnGround())
	{
		// Prepare the slide dash values

		m_vTransitionSpeedBoostStartVelocity = pCharacterMovement->Velocity.Length() <= m_fVelocityMovingTolerance ?
			GetCapsuleComponent()->GetForwardVector() * m_pSpeedComponent->m_fLiquidToSolidSpeedBoostImpulseFactorNoMovement :
			pCharacterMovement->Velocity;

		m_fTransitionSpeedBoostTarget = pCharacterMovement->Velocity.Length() + pCharacterMovement->Velocity.Length() * 0.5f;
		m_fTransitionSpeedBoostElapsedTime = 0.f;

		// Add Breaker marker
		m_bIsSlideDashBreaking = true;
		AddInteractableMarker<UBreakerInteractableMarker>();
	}
	// Else if the character is in gazeous dash
	else if (m_bIsGazeousDashing)
	{
		// Do the gazeous dash
		pCharacterMovement->AddImpulse(
			FVector::UpVector * (pCharacterMovement->IsMovingOnGround() ? m_pSpeedComponent->m_fLiquidToGazeousSpeedBoostImpulseFactor :
				pCharacterMovement->Velocity.Z < 0.f ? m_pSpeedComponent->m_fLiquidToGazeousSpeedBoostImpulseFactor * 3 :
				m_pSpeedComponent->m_fLiquidToGazeousSpeedBoostImpulseFactor)
		);
		m_bIsGazeousDashing = false;
	}
	// Else cancel the slide dash
	else
	{
		m_bIsSlideDashing = false;
	}
}

void ADropletPlayerCharacter::ApplyMaterialStateDurationAndCooldown(EDropletMaterialState eNewMaterialState)
{
	if (eNewMaterialState == EDropletMaterialState::EDropletMaterialState_Liquid || 
		eNewMaterialState == EDropletMaterialState::EDropletMaterialState_None)
	{
		return;
	}

	m_fCurrentStateDuration = eNewMaterialState == EDropletMaterialState::EDropletMaterialState_Solid ?
		m_fSolidStateDuration : m_fGazeousStateDuration;

	m_fCurrentStateChangeCooldown = eNewMaterialState == EDropletMaterialState::EDropletMaterialState_Solid ?
		m_fSolidStateCooldown : m_fGazeousStateCooldown;
}

float ADropletPlayerCharacter::GetSlopeAngle(FHitResult& Hit, FVector& vGlobalSlopeNormal) const
{
	float fSlopeAngle = 0.0f;

	//Debug purpose vector (global slope normal)
	vGlobalSlopeNormal = FVector::UpVector;

	// Check several points around the player to get the slope angle -------------------------
	float fNewTestSlopeAngle = 0.f;

	FVector vOffset = FVector::ZeroVector;

	uint8 uiTestNumber = 8;

	uint8 uiCountFlatDetected = 0;
	uint8 uiCountSlopeDetected = 0;
	uint8 uiCountNothingDetected = 0;

	for (int i = 0; i < uiTestNumber; ++i)
	{
		vOffset.X = FMath::Cos(i * PI / 4) * GetCapsuleComponent()->GetScaledCapsuleRadius();
		vOffset.Y = FMath::Sin(i * PI / 4) * GetCapsuleComponent()->GetScaledCapsuleRadius();

		//If the line trace hit something
		if (GetHitLineTracedUnder(Hit, vOffset))
		{
			//If the slope debug line trace is enabled
			if (m_bSlopeDetectionDebugDrawLineEnabled)
			{
				//Draw the impact normal
				FVector start = GetCapsuleComponent()->GetComponentLocation() + vOffset;
				FVector end = start + Hit.ImpactNormal * 200.f;
				DrawDebugLine(
					GetWorld(),
					start,
					end,
					FColor(0, 255, 0),
					false, -1.f, 0,
					12.333
				);
			}

			//Get the slope angle
			fNewTestSlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector)));

			//Increment the counter if the slope angle is greater than the threshold
			if (fNewTestSlopeAngle > m_fSlopeDetectionThreshold)
			{
				uiCountSlopeDetected++;
			}
			//Else increment the flat counter
			else
			{
				uiCountFlatDetected++;
			}
		}
		else
		{
			uiCountNothingDetected++;
			UE_LOG(LogMaterialStateMachine, VeryVerbose, TEXT("ADropletPlayerCharacter::GetSlopeAngle: GetHitLineTracedUnder number %i didn't hit something"), i + 1);
		}

		//Register the global slope normal
		vGlobalSlopeNormal = fNewTestSlopeAngle > fSlopeAngle ? Hit.ImpactNormal : vGlobalSlopeNormal;
		//THEN register the slope angle
		fSlopeAngle = fNewTestSlopeAngle > fSlopeAngle ? fNewTestSlopeAngle : fSlopeAngle;
	}
	// ---------------------------------------------------------------------------------------

	//If the line traces detected more flat than slope reset the slope angle
	if (uiCountFlatDetected > uiCountSlopeDetected)
	{
		fSlopeAngle = 0.f;
		vGlobalSlopeNormal = FVector::UpVector;
	}

	//If the slope debug line trace is enabled
	if (m_bSlopeDetectionDebugDrawLineEnabled)
	{
		//Draw the global slope normal
		FVector start = GetCapsuleComponent()->GetComponentLocation();
		FVector end = start + vGlobalSlopeNormal * 300.f;
		DrawDebugLine(
			GetWorld(),
			start,
			end,
			fSlopeAngle == 0.f ? FColor::Green : fSlopeAngle >= m_fMaxSlopeAngle ? FColor::Red : FColor::Yellow,
			false, -1.f, 0,
			12.333
		);
	}

	return fSlopeAngle;
}

bool ADropletPlayerCharacter::IsAscending() const
{
	// Check if we are NOT moving
	if (GetCharacterMovement()->Velocity.Size() <= m_fVelocityMovingTolerance)
	{
		return false;
	}

	bool bAscending = false;

	FHitResult hit;

	FVector vOffset = FVector::ZeroVector;
	for (int i = 0; i < 8; ++i)
	{
		vOffset.X = FMath::Cos(i * PI / 4) * GetCapsuleComponent()->GetScaledCapsuleRadius();
		vOffset.Y = FMath::Sin(i * PI / 4) * GetCapsuleComponent()->GetScaledCapsuleRadius();

		if (GetHitLineTracedUnder(hit, vOffset))
		{
			float fAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(hit.ImpactNormal, GetCharacterMovement()->Velocity.GetSafeNormal())));

			bAscending = bAscending || fAngle > 90.f;
		}
	}

	return bAscending;
}

bool ADropletPlayerCharacter::IsOnFlat() const
{
	FHitResult hit;
	FVector vHitNormal;
	float fSlopeAngle = GetSlopeAngle(hit, vHitNormal);

	return fSlopeAngle <= m_fFlatSurfaceTolerance;
}

void ADropletPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// If the DropletPlayerController is not valid return
	if (m_pDropletPlayerController == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("ADropletPlayerCharacter::Landed: m_pDropletPlayerController is nullptr"));
		return;
	}

	// If the material state is liquid
	if (m_pDropletPlayerController->GetMaterialState() == EDropletMaterialState::EDropletMaterialState_Liquid)
	{
		// If the character is not splashing and can splash
		if (!m_bIsSplashing && m_bCanSplash)
		{
			Splash(Hit);
		}

		BPE_OnLanded(EDropletMaterialState::EDropletMaterialState_Liquid);

		m_bCanSplash = false;
	}
	// Else if the material state is solid
	else if (m_pDropletPlayerController->GetMaterialState() == EDropletMaterialState::EDropletMaterialState_Solid)
	{
		BPE_OnLanded(EDropletMaterialState::EDropletMaterialState_Solid);
	}
}

#pragma region InterctableMarkers
TArray<UInteractableMarker*> ADropletPlayerCharacter::GetInteractableMarkers() const
{
	return m_InteractableMarkers;
}

template <typename T>
void ADropletPlayerCharacter::AddInteractableMarker()
{
	// If the interactable marker is not already in the array
	if (!m_InteractableMarkers.ContainsByPredicate([this](UInteractableMarker* pInteractableMarker)
		{ return Cast<T>(pInteractableMarker); }))
	{
		// Add the interactable marker to the array
		m_InteractableMarkers.Add(NewObject<T>(this));

		// Log the interactable marker is added
		UE_LOG(LogInteractable, Warning, TEXT("ADropletPlayerCharacter::AddInteractableMarker: Added %s"), *m_InteractableMarkers.Last()->GetName());
	}
}

template <typename T>
void ADropletPlayerCharacter::RemoveInteractableMarker()
{
	// If the interactable marker is not already in the array
	if (!m_InteractableMarkers.ContainsByPredicate([this](UInteractableMarker* pInteractableMarker)
		{ return Cast<T>(pInteractableMarker); }))
	{
		return;
	}

	// For each interactable marker
	for (UInteractableMarker* pInteractableMarker : m_InteractableMarkers)
	{
		// If the interactable marker is valid
		if (pInteractableMarker != nullptr)
		{
			// If the interactable marker is of the type to remove
			if (Cast<T>(pInteractableMarker))
			{
				m_InteractableMarkers.Remove(pInteractableMarker);
				break;
			}
		}
	}
}

template <typename T>
bool ADropletPlayerCharacter::HasInteractableMarker() const
{
	// For each interactable marker
	for (UInteractableMarker* pInteractableMarker : m_InteractableMarkers)
	{
		// If the interactable marker is valid
		if (pInteractableMarker != nullptr)
		{
			// If the interactable marker is of the type to remove
			if (Cast<T>(pInteractableMarker))
			{
				return true;
			}
		}
	}

	return false;
}

bool ADropletPlayerCharacter::BPF_HasBreakerMarker() const
{
	return HasInteractableMarker<UBreakerInteractableMarker>();
}

bool ADropletPlayerCharacter::BPF_HasDrillerMarker() const
{
	return HasInteractableMarker<UDrillerInteractableMarker>();
}

void ADropletPlayerCharacter::ClearInteractableMarkers()
{
	m_InteractableMarkers.Empty();
}
#pragma endregion

bool ADropletPlayerCharacter::GetHitLineTracedUnder(FHitResult& Hit, FVector vOffset /* = FVector::ZeroVector */, float fOvverideLineTraceVLength /* = -1.f */) const
{
	FVector start = GetCapsuleComponent()->GetComponentLocation() + vOffset;
	// Use either the override length if it's positive or the default length otherwise
	FVector end = start + (fOvverideLineTraceVLength >= 0.f ? fOvverideLineTraceVLength : m_fLineTraceVLength) * FVector::DownVector;
	FName profileName = TEXT("BlockAll");

	if (fOvverideLineTraceVLength >= 0.f && m_bIsSplashDebugDrawLineEnabled)
	{
		DrawDebugLine(
			GetWorld(),
			start,
			end,
			FColor::Purple,
			false,
			-1.f,
			0,
			12.333
		);
	}
	else if (m_bSlopeDetectionDebugDrawLineEnabled)
	{
		DrawDebugLine(
			GetWorld(),
			start,
			end,
			FColor::Red,
			false,
			-1.f,
			0,
			12.333
		);
	}

	return GetWorld()->LineTraceSingleByProfile(Hit, start, end, profileName, GetIgnoreCharacterLineTraceQueryParams());
}

FCollisionQueryParams ADropletPlayerCharacter::GetIgnoreCharacterLineTraceQueryParams() const
{
	FCollisionQueryParams queryParams;

	TArray<AActor*> characterChildren;
	GetAllChildActors(characterChildren);
	queryParams.AddIgnoredActors(characterChildren);
	queryParams.AddIgnoredActor(this);

	return queryParams;
}
