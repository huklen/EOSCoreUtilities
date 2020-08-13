/**
* Copyright (C) 2017-2020 | Dry Eel Development
*
* Official EOSCore Documentation: https://eeldev.com
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EOSCoreUtilitiesTypes.h"
#include "EOSCoreUtilitiesLibrary.generated.h"

UCLASS()
class EOSCOREUTILITIES_API UEOSCoreUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "EOSCoreUtilities")
		static void RequestEncryptedAppTicket(const FOnRequestAppTicketResponse& callback);
};
