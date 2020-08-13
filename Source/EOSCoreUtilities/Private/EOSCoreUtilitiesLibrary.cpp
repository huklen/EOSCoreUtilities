/**
* Copyright (C) 2017-2020 | Dry Eel Development
*
* Official EOSCore Documentation: https://eeldev.com
*/

#include "EOSCoreUtilitiesLibrary.h"
#include "EOSCoreUtilitiesModule.h"

void UEOSCoreUtilitiesLibrary::RequestEncryptedAppTicket(const FOnRequestAppTicketResponse& callback)
{
	FEOSCoreUtilitiesModule::Get()->RequestEncryptedAppTicket(callback);
}
