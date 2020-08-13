// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "EOSCoreUtilitiesTypes.h"
#include "Runtime/Launch/Resources/Version.h"

#pragma once

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

#define STEAMAPPIDFILENAME TEXT("steam_appid.txt")

#include "EOSCoreUtilitiesModule.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(EOSCoreUtilitiesLog, Log, All);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRequestEncryptedAppTicketDelegate, FString, ticket);

UCLASS()
class UTestClass : public UObject
{
	GENERATED_BODY()
};

class FEOSCoreUtilitiesModule : public IModuleInterface, public FTickerObjectBase
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void RequestEncryptedAppTicket(const FOnRequestAppTicketResponse& callback);
	static FEOSCoreUtilitiesModule* Get() { return FModuleManager::GetModulePtr<FEOSCoreUtilitiesModule>(FName("EOSCoreUtilities")); }
private:
	bool Init();
	bool Tick(float deltaTime) override;
private:
	void OnRequestEncryptedAppTicket(EncryptedAppTicketResponse_t* pEncryptedAppTicketResponse, bool bIOFailure);
private:
	TSharedPtr<class FSteamClientInstanceHandler> SteamAPIClientHandle;
	TSharedPtr<class FSteamServerInstanceHandler> SteamAPIServerHandle;
};
