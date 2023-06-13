#include "FGWorldSettings.h"
#include "FactoryGameCustomVersion.h"
#include "FGAudioVolumeSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "FGMinimapCaptureActor.h"
#include "FGSubsystemClasses.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "FGConveyorItemSubSystem.h"
#include "FGEnvironmentSettings.h"
#include "FGFoliageRemovalSubsystem.h"
#include "FGGameUserSettings.h"
#include "FGMapFunctionLibrary.h"
#include "Kismet/KismetMaterialLibrary.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#endif

bool ShouldConsiderActorForSave(AActor* actor, ULevel* settingsLevel) {
	if (actor == NULL) {
		return false;
	}
	if (actor->GetWorld() == NULL || actor->GetWorld()->IsGameWorld()) {
		if (!actor->IsA<AFGWorldSettings>()) {
			if (!actor->Implements<UFGSaveInterface>()) {
				return false;
			}
			return !actor->HasAnyFlags(RF_Transient);
		}
		return actor->IsInPersistentLevel();
	}
	{
		if (actor->IsA<AFGSubsystem>()) {
			return false;
		}
		if (actor->GetLevel() != settingsLevel) {
			return false;
		}
		if (!actor->Implements<UFGSaveInterface>()) {
			return false;
		}
		return !actor->HasAnyFlags(RF_Transient);
	}
}
#if WITH_EDITOR
void AFGWorldSettings::CheckForErrors(){ Super::CheckForErrors(); }
void AFGWorldSettings::PostEditChangeProperty( FPropertyChangedEvent& propertyChangedEvent){ Super::PostEditChangeProperty(propertyChangedEvent); }
#endif 
#if WITH_EDITOR
void AFGWorldSettings::HandleMapChanged( UWorld* newWorld, EMapChangeType mapChangeType){ }
void AFGWorldSettings::OnActorSpawned(AActor* actor) {
	if (AExponentialHeightFog* HeightFog = Cast<AExponentialHeightFog>(actor)) {
		mExponentialHeightFog = HeightFog;
	}
	if (ASkyAtmosphere* SkyAtmosphere = Cast<ASkyAtmosphere>(actor)) {
		mSkyAtmosphere = SkyAtmosphere;
	}
	if (AFGSkySphere* SkySphere = Cast<AFGSkySphere>(actor)) {
		mSkySphere = SkySphere;
	}
	if (AFGMinimapCaptureActor* MinimapCaptureActor = Cast<AFGMinimapCaptureActor>(actor)) {
		mMinimapCaptureActor = MinimapCaptureActor;
	}
	mSaveActorsDirty = true;

}
#endif 
#if WITH_EDITORONLY_DATA
#endif 
AFGWorldSettings::AFGWorldSettings() : Super() {
	this->mDefaultHeightFogSettings.FogHeight = 5000.0;
	this->mDefaultHeightFogSettings.FogDensity = 0.02;
	this->mDefaultHeightFogSettings.FogInscatteringColor = FLinearColor(0.0, 0.0, 0.0, 1.0);
	this->mDefaultHeightFogSettings.FullyDirectionalInscatteringColorDistance = 100000.0;
	this->mDefaultHeightFogSettings.NonDirectionalInscatteringColorDistance = 1000.0;
	this->mDefaultHeightFogSettings.DirectionalInscatteringExponent = 4.0;
	this->mDefaultHeightFogSettings.DirectionalInscatteringStartDistance = 10000.0;
	this->mDefaultHeightFogSettings.DirectionalInscatteringColor = FLinearColor(0.0, 0.0, 0.0, 1.0);
	this->mDefaultHeightFogSettings.FogHeightFalloff = 0.2;
	this->mDefaultHeightFogSettings.FogMaxOpacity = 1.0;
	this->mDefaultHeightFogSettings.StartDistance = 0.0;
	this->mDefaultHeightFogSettings.FogCutoffDistance = 0.0;
	this->mDefaultHeightFogSettings.SecondFogDensity = 0.0;
	this->mDefaultHeightFogSettings.SecondFogHeightFalloff = 0.2;
	this->mDefaultHeightFogSettings.SecondFogHeightOffset = 0.0;
	this->mSaveActorsDirty = false;
	this->mExponentialHeightFog = nullptr;
	this->mSkyAtmosphere = nullptr;
	this->mSkySphere = nullptr;
	this->mMinimapCaptureActor = nullptr;
	this->mLevelStartedEvent = nullptr;
	this->mDefaultLoadSave = TEXT("");
	this->mStartTimeOfDay = 12.0;
	this->mBuildableSubsystem = nullptr;
	this->mAudioVolumeSubsystem = nullptr;
	this->mFoliageRemovalSubsystem = nullptr;
	this->mConveyorItemSubsystem = nullptr;
	// this->mPhotoModeManager = nullptr;
}
void AFGWorldSettings::BeginDestroy() {
	Super::BeginDestroy();

#if WITH_EDITOR
	//Unregister Map Change Events
	if (mOnMapChangedDelegateHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnMapChanged().Remove(mOnMapChangedDelegateHandle);
		mOnMapChangedDelegateHandle.Reset();
	}

	//Unregister Actor Spawned Event
	if (GetWorld() != NULL) {
		GetWorld()->RemoveOnActorSpawnedHandler(mActorSpawnedDelegateHandle);
		mActorSpawnedDelegateHandle.Reset();
	}
#endif

}
void AFGWorldSettings::AddReferencedObjects(UObject* inThis, FReferenceCollector& collector) {
	AFGWorldSettings* Settings = CastChecked<AFGWorldSettings>(inThis);
	collector.AddReferencedObjects(Settings->mSaveActors, inThis);

}
void AFGWorldSettings::Serialize(FArchive& ar) {
	Super::Serialize(ar);
	ar.UsingCustomVersion(FFactoryGameCustomVersion::GUID);

	if (ar.IsSaveGame()) {
		return;	
	}
	if (!ar.IsSaving() && !ar.IsLoading() || ar.CustomVer(FFactoryGameCustomVersion::GUID) < FFactoryGameCustomVersion::CachedSaveActors) {
		return;
	}
	
	const FString LevelName = GetLevel()->GetFullName();
	if (LevelName.Find(TEXT("_LOD"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) != INDEX_NONE) {
		return;
	}

	if (ar.IsCooking()) {
		if (mSaveActors.Contains((AActor*) NULL)) {
			UE_LOG(LogGame, Warning,
				TEXT("AFGWorldSettings::mSaveActors contains nullpointers during cook, that should never happen. Please resave the level %s"),
				*LevelName);
		}
	}
	if (mSaveActorsDirty) {
		PrepareSaveActors();
		mSaveActorsDirty = false;
	}
	ar << mSaveActors;

}
void AFGWorldSettings::PostActorCreated(){ Super::PostActorCreated(); }
void AFGWorldSettings::PostLoad() {
	Super::PostLoad();
	PrepareSaveActors();

}
void AFGWorldSettings::PreInitializeComponents() {
	Super::PreInitializeComponents();

#if WITH_EDITOR
	//Register Map Change Events
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	mOnMapChangedDelegateHandle = LevelEditor.OnMapChanged().AddUObject(this, &AFGWorldSettings::HandleMapChanged);

	//Register Actor Spawned Event
	mActorSpawnedDelegateHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AFGWorldSettings::OnActorSpawned));
#endif
	
	UFGSubsystemClasses* SubsystemClasses = UFGSubsystemClasses::Get();
	SpawnSubsystem<AFGFoliageRemovalSubsystem>(mFoliageRemovalSubsystem, SubsystemClasses->mFoliageRemovalSubsystemClass, TEXT("FoliageRemovalSubsystem"));
	SpawnSubsystem<AFGAudioVolumeSubsystem>(mAudioVolumeSubsystem, AFGAudioVolumeSubsystem::StaticClass(), TEXT("AudioVolumeSubsystem"));
	SpawnSubsystem<AFGBuildableSubsystem>(mBuildableSubsystem, SubsystemClasses->mBuildableSubsystemClass.Get(), TEXT("BuildableSubsystem"));
	// SpawnSubsystem<AFGPhotoModeManager>(mPhotoModeManager, SubsystemClasses->mPhotoModeManagerClass, TEXT("PhotoModeManager"));

	if (GetWorld()->WorldType != EWorldType::Editor && GetWorld()->WorldType != EWorldType::EditorPreview) {
		SpawnSubsystem<AFGConveyorItemSubsystem>(mConveyorItemSubsystem, SubsystemClasses->mConveyorItemSubsystemClass, TEXT("ConveyorItemSubsystem"));
	}
}
void AFGWorldSettings::NotifyBeginPlay() {
	Super::NotifyBeginPlay();

	if (!GetWorld()->HasBegunPlay()) {
		AFGFoliageRemovalSubsystem* FoliageRemovalSubsystem = AFGFoliageRemovalSubsystem::Get(GetWorld());
		if (FoliageRemovalSubsystem) {
			FoliageRemovalSubsystem->Init();
		}

		UFGGameUserSettings* UserSettings = UFGGameUserSettings::GetFGGameUserSettings();
		if (UserSettings) {
			UserSettings->ApplyHologramColoursToCollectionParameterInstance(GetWorld());
		}
	}
}
void AFGWorldSettings::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion){ }
void AFGWorldSettings::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion){ }
void AFGWorldSettings::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion){ }
void AFGWorldSettings::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion){ }
void AFGWorldSettings::GatherDependencies_Implementation(TArray< UObject* >& out_dependentObjects){ }
bool AFGWorldSettings::NeedTransform_Implementation(){ return bool(); }
bool AFGWorldSettings::ShouldSave_Implementation() const{ return bool(); }
AExponentialHeightFog* AFGWorldSettings::GetExponentialHeightFog() const {
	return mExponentialHeightFog.LoadSynchronous();

}
ASkyAtmosphere* AFGWorldSettings::GetSkyAtmosphere() const {
	return mSkyAtmosphere.LoadSynchronous();

}
AFGSkySphere* AFGWorldSettings::GetSkySphere() const {
	return mSkySphere.LoadSynchronous();

}
void AFGWorldSettings::UpdateWorldBounds() {
	UMaterialParameterCollection* ParameterCollection = UFGEnvironmentSettings::GetWorldBoundsParameters();

	if (ParameterCollection) {
		FVector2D Min, Max;
		UFGMapFunctionLibrary::GetWorldBounds(this, Min, Max);

		UKismetMaterialLibrary::SetVectorParameterValue(this, ParameterCollection,
			UFGEnvironmentSettings::WorldBoundsMinName,
			FLinearColor(FVector(Min.X, Min.Y, 0.0f)));
		
		UKismetMaterialLibrary::SetVectorParameterValue(this, ParameterCollection,
			UFGEnvironmentSettings::WorldBoundsExtentName,
			FLinearColor(FVector(Max.X - Min.X, Max.Y - Min.Y, 0.0f)));
	}
}
void AFGWorldSettings::OnSaveActorDestroyed(AActor* actor) {
	actor->OnEndPlay.RemoveAll(this);
	actor->OnDestroyed.RemoveAll(this);
	mSaveActors.Remove(actor);
	
	if (GetWorld()) {
		if (!GetWorld()->IsGameWorld()) {
			mSaveActorsDirty = true;
		}
	}

}
void AFGWorldSettings::PrepareSaveActors() {
	ULevel* Level = GetLevel();
	check(Level);

	if (mSaveActorsDirty || GetLinkerCustomVersion(FFactoryGameCustomVersion::GUID) < FFactoryGameCustomVersion::CachedSaveActors) {
		mSaveActors.Empty();

		for (AActor* LevelActor : Level->Actors) {
			if (!ShouldConsiderActorForSave(LevelActor, Level)) {
				continue;
			}
			if (LevelActor->IsPendingKill()) {
				continue;
			}
			if (!LevelActor->OnDestroyed.IsAlreadyBound(this, &AFGWorldSettings::OnSaveActorDestroyed)) {
				LevelActor->OnDestroyed.AddDynamic(this, &AFGWorldSettings::OnSaveActorDestroyed);
			}
			mSaveActors.Add(LevelActor);
		}
	}
	mSaveActors.Remove(NULL);

	for (AActor* SaveActor : mSaveActors) {
		if (!SaveActor->OnDestroyed.IsAlreadyBound(this, &AFGWorldSettings::OnSaveActorDestroyed)) {
			SaveActor->OnDestroyed.AddDynamic(this, &AFGWorldSettings::OnSaveActorDestroyed);
		}
	}

}
