// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EOSCoreUtilitiesModule.h"
#include "SteamSharedModule.h"
#include "Serialization/BufferArchive.h"
#include "ThirdParty/EOSLibrary/Include/eos_common.h"

#define LOCTEXT_NAMESPACE "FEOSCoreUtilitiesModule"

DEFINE_LOG_CATEGORY(EOSCoreUtilitiesLog);

inline FString GetSteamAppIdFilename()
{
	return FString::Printf(TEXT("%s%s"), FPlatformProcess::BaseDir(), STEAMAPPIDFILENAME);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
/**
 * Write out the steam app id to the steam_appid.txt file before initializing the API
 * @param SteamAppId id assigned to the application by Steam
 */
static bool WriteSteamAppIdToDisk(int32 SteamAppId)
{
	if (SteamAppId > 0)
	{
		// Access the physical file writer directly so that we still write next to the executable in CotF builds.
		FString SteamAppIdFilename = GetSteamAppIdFilename();
		IFileHandle* Handle = IPlatformFile::GetPlatformPhysical().OpenWrite(*SteamAppIdFilename, false, false);
		if (!Handle)
		{
			UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Failed to create file: %s"), *SteamAppIdFilename);
			return false;
		}
		else
		{
			FString AppId = FString::Printf(TEXT("%d"), SteamAppId);

			FBufferArchive Archive;
			Archive.Serialize((void*)TCHAR_TO_ANSI(*AppId), AppId.Len());

			Handle->Write(Archive.GetData(), Archive.Num());
			delete Handle;
			Handle = nullptr;

			return true;
		}
	}

	UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Steam App Id provided (%d) is invalid, must be greater than 0!"), SteamAppId);
	return false;
}

/**
 * Deletes the app id file from disk
 */
static void DeleteSteamAppIdFromDisk()
{
	const FString SteamAppIdFilename = GetSteamAppIdFilename();
	// Turn off sandbox temporarily to make sure file is where it's always expected

	if (FPaths::FileExists(*SteamAppIdFilename))
	{
		bool bSuccessfullyDeleted = IFileManager::Get().Delete(*SteamAppIdFilename);
	}
}

#endif // !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR

void FEOSCoreUtilitiesModule::StartupModule()
{
}

void FEOSCoreUtilitiesModule::ShutdownModule()
{
	SteamAPIClientHandle.Reset();
	SteamAPIServerHandle.Reset();

#if !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
	DeleteSteamAppIdFromDisk();
#endif // !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
	SteamAPI_Shutdown();
}

bool ConfigureSteamInitDevOptions(bool& RequireRelaunch, int32& RelaunchAppId)
{
	// Write out the steam_appid.txt file before launching
	if (!GConfig->GetInt(TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), RelaunchAppId, GEngineIni))
	{
		UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Missing SteamDevAppId key in OnlineSubsystemSteam of DefaultEngine.ini"));
		return false;
	}
	else
	{
		if (!WriteSteamAppIdToDisk(RelaunchAppId))
		{
			UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Could not create/update the steam_appid.txt file! Make sure the directory is writable and there isn't another instance using this file"));
			return false;
		}
	}

	return true;
}

bool FEOSCoreUtilitiesModule::Init()
{
	if (SteamAPIServerHandle.IsValid() || SteamAPIClientHandle.IsValid())
	{
		return true;
	}

	bool bRelaunchInSteam = false;
	int RelaunchAppId = 0;

	if (!ConfigureSteamInitDevOptions(bRelaunchInSteam, RelaunchAppId))
	{
		UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Could not set up the steam environment!"));
		return false;
	}

	const bool bIsDedicated = IsRunningDedicatedServer();

	if (bIsDedicated)
	{
		SteamAPIServerHandle = FSteamSharedModule::Get().ObtainSteamServerInstanceHandle();

		if (!SteamAPIServerHandle.IsValid())
		{
			UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Could not initialize the game server SteamAPI!"));
			SteamAPIServerHandle.Reset();
		}
	}
	else
	{
		SteamAPIClientHandle = FSteamSharedModule::Get().ObtainSteamClientInstanceHandle();

		if (!SteamAPIClientHandle.IsValid())
		{
			UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("Could not obtain a handle to SteamAPI!"));
			SteamAPIClientHandle.Reset();
		}
	}

	if (SteamAPIServerHandle.IsValid() || SteamAPIClientHandle.IsValid())
	{
		UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("SteamAPI Initialized!"));

		return true;
	}

	return false;
}

bool FEOSCoreUtilitiesModule::Tick(float deltaTime)
{
	if (SteamAPIServerHandle.IsValid() || SteamAPIClientHandle.IsValid())
	{
		SteamAPI_RunCallbacks();
	}

	return true;
}

static CCallResult<FEOSCoreUtilitiesModule, EncryptedAppTicketResponse_t> SteamCallResultEncryptedAppTicket;

static TQueue<FOnRequestAppTicketResponse> s_OnRequestAppTicketResponse;
void FEOSCoreUtilitiesModule::RequestEncryptedAppTicket(const FOnRequestAppTicketResponse& callback)
{
	if (Init())
	{
		s_OnRequestAppTicketResponse.Enqueue(callback);

		UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("Requested encrypted app ticket"));

		SteamAPICall_t m_Handle = SteamUser()->RequestEncryptedAppTicket(nullptr, 0);
		SteamCallResultEncryptedAppTicket.Set(m_Handle, Get(), &FEOSCoreUtilitiesModule::OnRequestEncryptedAppTicket);
	}
}

void FEOSCoreUtilitiesModule::OnRequestEncryptedAppTicket(EncryptedAppTicketResponse_t* pEncryptedAppTicketResponse, bool bIOFailure)
{
	UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("OnRequestEncryptedAppTicket Callback"));

	if (bIOFailure)
	{
		UE_LOG(EOSCoreUtilitiesLog, Error, TEXT("OnRequestEncryptedAppTicket callback IOFailure!"));
		return;
	}

	if (pEncryptedAppTicketResponse->m_eResult == k_EResultOK)
	{
		uint32 m_TicketSize = 0;
		SteamUser()->GetEncryptedAppTicket(nullptr, 0, &m_TicketSize);

		uint32 m_BuffSize = m_TicketSize;
		uint8* m_AppTicket = new uint8[m_BuffSize];

		if (!SteamUser()->GetEncryptedAppTicket(m_AppTicket, m_BuffSize, &m_TicketSize))
		{
			UE_LOG(EOSCoreUtilitiesLog, Warning, TEXT("Steam App Ticket not available"));
			delete[] m_AppTicket;
			return;
		}

		TArray<uint8> m_StringBuff;
		m_StringBuff.Append(m_AppTicket, m_TicketSize);
		FString m_TicketString;

		char* m_Buffer = new char[2048];
		uint32_t outBuffer = 2048;

		EOS_EResult m_Result = EOS_ByteArray_ToString(m_StringBuff.GetData(), m_StringBuff.Num(), m_Buffer, &outBuffer);
		if (m_Result == EOS_EResult::EOS_Success)
		{
			m_TicketString = m_Buffer;
		}

		delete[] m_Buffer;
		delete[] m_AppTicket;

		FOnRequestAppTicketResponse m_Delegate;
		while (s_OnRequestAppTicketResponse.Dequeue(m_Delegate))
		{
			m_Delegate.ExecuteIfBound(m_TicketString);
		}
	}
	else if (pEncryptedAppTicketResponse->m_eResult == k_EResultLimitExceeded)
	{
		UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("OnRequestEncryptedAppTicket Callback - Calling RequestEncryptedAppTicket more than once per minute returns this error"));
	}
	else if (pEncryptedAppTicketResponse->m_eResult == k_EResultDuplicateRequest)
	{
		UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("OnRequestEncryptedAppTicket Callback - Calling RequestEncryptedAppTicket while there is already a pending request results in this error"));
	}
	else if (pEncryptedAppTicketResponse->m_eResult == k_EResultNoConnection)
	{
		UE_LOG(EOSCoreUtilitiesLog, Verbose, TEXT("OnRequestEncryptedAppTicket Callback - Calling RequestEncryptedAppTicket while not connected to steam results in this error"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEOSCoreUtilitiesModule, EOSCoreUtilities)