// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Player/VeinPlayerCharacter.h"
#include "Delegates/DelegateCombinations.h"
#include "MaterialStateDescription/DropletMaterialStateDescription.h"
#include "../Components/StaminaComponent.h"
#include "../Components/SpeedComponent.h"
#include "../Components/Interactables/InteractableMarker.h"
#include "Components/SphereComponent.h"

#include "DropletPlayerCharacter.generated.h"

class ADropletPlayerController;

//Delegate for player movement
DECLARE_DYNAMIC_DELEGATE_OneParam(FMoveFunction, const FInputActionValue&, Value);

//Delegate for player state change
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStateChanged, EDropletMaterialState, eNewMaterialState);


/**
 * The DropletPlayerCharacter class to represent the player character in the game
 */
UCLASS(config=Game)
class ADropletPlayerCharacter : public AVeinPlayerCharacter
{
	GENERATED_BODY()



#pragma region Input Public Variables
	/** ChangeState Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Input", meta = (AllowPrivateAccess = "true", DisplayAfter = "Look"))
	class UInputAction* ChangeState;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Input", meta = (AllowPrivateAccess = "true"))
	class UInputAction* InteractInputAction;

	/** LiquidMappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropletPlayerCharacter|Input", meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* LiquidMappingContext;

	/** SolidMappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropletPlayerCharacter|Input", meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* SolidMappingContext;

	/** GazeousMappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropletPlayerCharacter|Input", meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* GazeousMappingContext;
#pragma endregion



	/** LiquidStaminaComponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Stamina", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UStaminaComponent> LiquidStaminaComponent;

	/** SolidStaminaComponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Stamina", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UStaminaComponent> SolidStaminaComponent;

	/** GazeousStaminaComponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Stamina", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UStaminaComponent> GazeousStaminaComponent;

	/** SpeedComponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Speed", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<USpeedComponent> SpeedComponent;

	/** SphereComponent to determine in which range the player can interact with InputInteractableActorComponents */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> pInteractableRangeSphereComponent;

public:
	friend class UDropletMaterialStateDescription;

public:
	/** Assigns the right modifications depending on the DropletMaterialState passed as argument */
	void SetMaterialState(EDropletMaterialState eNewMaterialState, bool bIsPlayerInitiated = false);

	/** Permits to assign the wanted MaterialState at the start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "Initial Material State"))
	EDropletMaterialState m_eInitialMaterialState = EDropletMaterialState::EDropletMaterialState_None;

	/** Permits to enable slope detection debug draw line */
	UPROPERTY(BlueprintReadOnly, meta = (DisplayName = "Slope Detection Debug Draw Line"))
	bool m_bSlopeDetectionDebugDrawLineEnabled = false;

	/** Permits to enable or disable state duration before going back to liquid state */
	bool m_bIsNoTimeLimitCheatEnabled = false;

	/** Permits to enable or disable splash debug draw lines */
	bool m_bIsSplashDebugDrawLineEnabled = false;

	/** Permits to enable or disable interactables debug */
	UPROPERTY(BlueprintReadOnly, meta = (DisplayName = "bInteractableDebug"))
	bool m_bIsInteractablesDebugEnabled = false;

	/** Permits to enable or disable interactable marker display */
	UPROPERTY(BlueprintReadOnly)
	bool m_bInteractableMarkerDisplayed = false;

	bool m_bIsInDialogue = false;
	
	UPROPERTY(BlueprintAssignable)
	FStateChanged OnMaterialStateChange;



	// ----------------------------------- Game design settings -----------------------------------------------------------


	// ----------------------------------- Material State related settings ------------------------------------------------

	/** Cooldown duration between two state changes in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "State Change Cooldown"))
	float m_fCurrentStateChangeCooldown = 2.0f;
	/** Transition duration of a state change in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "State Change Transition Duration"))
	float m_fBaseStateChangeDuration = 0.2f;
	/** State duration when different of basic state in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "State Duration"))
	float m_fCurrentStateDuration = 8.0f;

	/** Solid state duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "Solid State Duration"))
	float m_fSolidStateDuration = 8.0f;
	/** Gazeous state duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "Gazeous State Duration"))
	float m_fGazeousStateDuration = 8.0f;

	/** Solid state cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "Solid State Cooldown"))
	float m_fSolidStateCooldown = 2.0f;
	/** Gazeous state cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter", meta = (DisplayName = "Gazeous State Cooldown"))
	float m_fGazeousStateCooldown = 2.0f;



#pragma region Break and drill settings
	// ----------------------------------- Break and drill settings -------------------------------------------------------
	
	/** Slide dash Breaker state duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|BreakAndDrill", meta = (DisplayName = "Slide Dash Breaker State Duration"))
	float m_fSlideDashBreakerStateDuration = 1.0f;
	/** Breaker state speed threshold in cm per seconds*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|BreakAndDrill", meta = (DisplayName = "Breaker State Speed Threshold"))
	float m_fBreakerStateSpeedThreshold = 1000.0f;

	/** Driller state duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|BreakAndDrill", meta = (DisplayName = "Driller State Duration"))
	float m_fDrillerStateDuration = 1.0f;
#pragma endregion



	// ----------------------------------- Slope detection related settings -----------------------------------------------

	// Flat surface tolerance (included as flat)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|SlopeDetection", meta = (DisplayName = "Flat Surface Tolerance"))
	float m_fFlatSurfaceTolerance = 0.1f;
	// Velocity moving tolerance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|SlopeDetection", meta = (DisplayName = "Velocity Moving Tolerance"))
	float m_fVelocityMovingTolerance = 0.1f;
	// Length of the line trace vector
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|SlopeDetection", meta = (DisplayName = "Line Trace Vector Length"))
	float m_fLineTraceVLength = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|SlopeDetection", meta = (DisplayName = "Slop Detetction Threshold"))
	float m_fSlopeDetectionThreshold = 1.f;

	// Distance threshold to the ground to be able to splash
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|SplashDetection", meta = (DisplayName = "Splash Detetction Threshold"))
	float m_fSplashDistanceToGroundThreshold = 200.f;



	// ----------------------------------- Art related settings -----------------------------------------------------------

	/** Liquid material instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Materials", meta = (DisplayName = "Liquid Material Instance"))
	class UMaterialInstance* m_pLiquidMaterialInstance = nullptr;
	/** Solid material instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Materials", meta = (DisplayName = "Solid Material Instance"))
	class UMaterialInstance* m_pSolidMaterialInstance = nullptr;
	/** Gazeous material instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Materials", meta = (DisplayName = "Gazeous Material Instance"))
	class UMaterialInstance* m_pGazeousMaterialInstance = nullptr;

	/** Liquid skeletal mesh instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Meshes", meta = (DisplayName = "Liquid Skeletal Mesh Instance"))
	class USkeletalMesh* m_pLiquidSKInstance = nullptr;
	/** Solid skeletal mesh instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Meshes", meta = (DisplayName = "Solid Skeletal Mesh Instance"))
	class USkeletalMesh* m_pSolidSKInstance = nullptr;
	/** Gazeous skeletal mesh instance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Meshes", meta = (DisplayName = "Gazeous Skeletal Mesh Instance"))
	class USkeletalMesh* m_pGazeousSKInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DropletPlayerCharacter|Speed", meta = (DisplayName = "Oil Speed Factor"))
	float m_fOilSpeedFactor = 0.5f;

public:
	ADropletPlayerCharacter();

	/** Called when the game starts or when spawned */
	virtual void BeginPlay() override;

	/** Called every frame */
	virtual void Tick(float fDeltaTime) override;

	/** Called to have visual effects when changing state */
	UFUNCTION(BlueprintImplementableEvent)
	void BPE_OnMaterialStateChanged(EDropletMaterialState eNewMaterialState);

	/** Called when player land */
	UFUNCTION(BlueprintImplementableEvent)
	void BPE_OnLanded(EDropletMaterialState eMaterialState);

	/** Called to determine the angle of the slope we are on */
	virtual float GetSlopeAngle(FHitResult& Hit, FVector& vGlobalSlopeNormal) const;

	/** Called to determine if we are ascending a slope */
	virtual bool IsAscending() const;

	/** Called to determine if we are on a flat surface */
	virtual bool IsOnFlat() const;

	/** Perform special action on landing */
	virtual void Landed(const FHitResult& Hit) override;

	// ----------------------------------- Getters and Setters ------------------------------------------------------------

	float GetMaxSlopeAngle() const { return m_fMaxSlopeAngle; }
	float GetStepSlopeAngle() const { return m_fStepSlopeAngle; }

	TObjectPtr<USphereComponent> GetInteractableRangeSphereComponent() const { return pInteractableRangeSphereComponent; }

#pragma region InteractableMarkers
	TArray<UInteractableMarker*> GetInteractableMarkers() const;

	// Template function to add any type of marker by its type to the array of Interactable markers
	template <typename T>
	void AddInteractableMarker();

	// Template function to remove any type of marker by its type from the array of Interactable markers
	template <typename T>
	void RemoveInteractableMarker();

	// Template function to check if the array of Interactable markers contains any type of marker by its type
	template <typename T>
	bool HasInteractableMarker() const;

	// Checks and returns if the array of Interactable markers contains a Breaker marker
	UFUNCTION(BlueprintCallable)
	bool BPF_HasBreakerMarker() const;

	// Checks and returns if the array of Interactable markers contains a Driller marker
	UFUNCTION(BlueprintCallable)
	bool BPF_HasDrillerMarker() const;

	// Planned to be blueprinted to toggle the markers' display on the UI
	UFUNCTION(BlueprintImplementableEvent)
	void BPE_OnInteractableMarkersToggleDisplay();

	// Planned to be blueprinted to display the markers on the UI
	UFUNCTION(BlueprintImplementableEvent)
	void BPE_OnInteractableMarkersDisplayed();

	// Clear the array of Interactable markers
	void ClearInteractableMarkers();

	// Called to display the interaction button
	void DisplayInteractionButton();

#pragma endregion

protected:
	/** Called to bind functionality to input */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	virtual void Move(const FInputActionValue& Value) override;

	/** Called for jump input */
	virtual void Jump() override;

	/** Called for interaction input */
	virtual void Interact();

	/** Called to handle interaction action display */
	virtual void HandleInteractionButtonDisplay();

	/** Called for changing the material state */
	virtual void ChangeMaterialState(const FInputActionValue& Value);

	/** Called when possession is gained */
	virtual void PossessedBy(AController* pNewController) override;

	/** Called to perform the splash action */
	virtual void Splash(const FHitResult& Hit);

	/** Called to change the input mapping context to the material state's corresponding one */
	void ChangeInputMappingContext(EDropletMaterialState eNewMaterialState);

	/** Called to change the skeletal mesh to the material state's one */
	void ChangeSkeletalMeshInstance(EDropletMaterialState eNewMaterialState);

	/** Called to change the material instance of the mesh */
	void ChangeMeshMaterialInstance(EDropletMaterialState eNewMaterialState);

	/** Called to change the StaminaComponent */
	void ChangeStaminaComponent(EDropletMaterialState eNewMaterialState);

	/** Called to apply the MaterialState corresponding SpeedComponent values */
	void ApplySpeedComponentStateValues(EDropletMaterialState eNewMaterialState);

	/** Called to apply the MaterialState corresponding duration values */
	void ApplyMaterialStateDurationAndCooldown(EDropletMaterialState eNewMaterialState);

	/** Called to get a hit result under the character */
	virtual bool GetHitLineTracedUnder(FHitResult& Hit, FVector vOffset = FVector::ZeroVector, float fOvverideLineTraceVLength = -1.f) const;

	/** Called to get the Query Parameters to ignore Character in line trace */
	virtual FCollisionQueryParams GetIgnoreCharacterLineTraceQueryParams() const;

protected:
	// Cached speed component
	UPROPERTY(BlueprintReadOnly, Category = "DropletPlayerCharacter|Speed", meta = (AllowPrivateAccess = "true"))
	USpeedComponent* m_pSpeedComponent = nullptr;

	// Max slope angle
	UPROPERTY(BlueprintReadOnly, Category = "SlopeDetection", meta = (DisplayName = "Max Slope Angle"))
	float m_fMaxSlopeAngle = 45.0f;

	// Step slope angle
	UPROPERTY(BlueprintReadOnly, Category = "SlopeDetection", meta = (DisplayName = "Step Slope Angle"))
	float m_fStepSlopeAngle = 45.0f;

	// Current falling speed
	UPROPERTY(BlueprintReadOnly, Category = "SpeedComponent", meta = (DisplayName = "Current Falling Speed"))
	float m_fCurrentFallingSpeed = 0.0f;

	// Just Changed State
	UPROPERTY(BlueprintReadOnly, Category = "SpeedComponent", meta = (DisplayName = "Just Changed State"))
	bool m_bJustChangedState = false;

	// InteractableMarkers array
	UPROPERTY(BlueprintReadWrite, Category = "InteractableMarkers", meta = (DisplayName = "Interactable Markers Array"))
	TArray<UInteractableMarker*> m_InteractableMarkers;

	UPROPERTY(BlueprintReadWrite, Category = "SpeedComponent", meta = (DisplayName = "Is Under Oil Effect"))
	bool m_bIsUnderOilEffect;

private:
	ADropletPlayerController* m_pDropletPlayerController = nullptr;

	/** Changes the movement function depending on the DropletPlayerController MaterialState by binding it to a new function */
	FMoveFunction m_MoveFunction;

	float m_fTargetMaxSpeed = -1.f;



	// State transition speed boost values ---------------------------------------

	bool m_bIsSlideDashing = false;
	bool m_bIsGazeousDashing = false;

	// Liquid to other state transition speed boost target (computing at runtime)
	float m_fTransitionSpeedBoostTarget = 0.f;
	// Liquid to other state transition speed boost start velocity
	FVector m_vTransitionSpeedBoostStartVelocity = FVector::ZeroVector;
	// Elapsed time since the start of the speed boost
	float m_fTransitionSpeedBoostElapsedTime = 0.f;


	// Splash speed boost values ---------------------------------------------------

	// Did the splash happen
	bool m_bIsSplashing = false;
	// Splash elapsed time
	float m_fSplashElapsedTime = 0.f;
	// Splash start speed (computing at runtime)
	float m_fSplashStartSpeed = 0.f;
	// Splash target speed (computing at runtime)
	float m_fSplashTargetSpeed = 0.f;
	// Splash direction (computing at runtime)
	FVector m_vSplashDirection = FVector::ZeroVector;

	// Indicates if the character can splash (computing at runtime)
	bool m_bCanSplash = false;

	// Break and Drill values ------------------------------------------------------

	// Indicates if the character is in slide dash breaking
	bool m_bIsSlideDashBreaking = false;
	// Slide dash break elapsed time
	float m_fSlideDashBreakElapsedTime = 0.f;

	// Driller elapsed time
	float m_fDrillerElapsedTime = 0.f;
};
