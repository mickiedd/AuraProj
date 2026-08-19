// Copyright Druid Mechanics

#include "Game/GameServerClient.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Json.h"
#include "Misc/NetworkVersion.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameServerClient, Log, All);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace GameServerClientInternal
{
	static FString SocketErrorToString(ISocketSubsystem* SS)
	{
		if (!SS)
		{
			return TEXT("NoSocketSubsystem");
		}

		const ESocketErrors LastError = SS->GetLastErrorCode();
		return FString::Printf(TEXT("%s (%d)"), SS->GetSocketError(LastError), static_cast<int32>(LastError));
	}

	/** Convert FString to UTF-8 byte array. */
	static TArray<uint8> StringToUtf8(const FString& Str)
	{
		FTCHARToUTF8 Convert(*Str, Str.Len());
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Convert.Get()), Convert.Length());
		return Bytes;
	}

	/**
	 * Read bytes from Socket until a newline ('\n') is found, a limit is reached,
	 * or the per-read Wait times out.
	 * Returns the line without the trailing newline.
	 */
	static bool ReadLine(FSocket* Socket, FTimespan WaitTime, FString& OutLine, const FString& RequestId, int32 MaxBytes = 8192)
	{
		OutLine.Empty();

		uint8 Byte = 0;
		int32 BytesRead = 0;

		while (OutLine.Len() < MaxBytes)
		{
			// Wait until data is available or timeout.
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
			{
				UE_LOG(LogGameServerClient, Warning, TEXT("[Req=%s] ReadLine timed out waiting for data"), *RequestId);
				return false;
			}

			if (!Socket->Recv(&Byte, 1, BytesRead, ESocketReceiveFlags::None) || BytesRead == 0)
			{
				UE_LOG(LogGameServerClient, Warning, TEXT("[Req=%s] ReadLine recv failed or remote closed (BytesRead=%d)"), *RequestId, BytesRead);
				// Connection closed by remote.
				break;
			}

			if (Byte == '\n')
			{
				return true;
			}

			OutLine.AppendChar(static_cast<TCHAR>(Byte));
		}

		// We reach here if the connection closed without a newline or the buffer was full.
		if (OutLine.Len() >= MaxBytes)
		{
			UE_LOG(LogGameServerClient, Warning, TEXT("[Req=%s] ReadLine reached max bytes (%d) without newline"), *RequestId, MaxBytes);
		}
		return !OutLine.IsEmpty();
	}

	/** Fire the delegate on the game thread; captures result by value. */
	static void DispatchResult(FOnGameServerResponse Delegate, FGameServerResponse Result)
	{
		AsyncTask(ENamedThreads::GameThread, [Delegate = MoveTemp(Delegate), Result = MoveTemp(Result)]()
		{
			Delegate.ExecuteIfBound(Result);
		});
	}
}

// ---------------------------------------------------------------------------
// UGameServerClient::RequestServer
// ---------------------------------------------------------------------------

void UGameServerClient::RequestServer(
	const FString& GameServerAddress,
	int32 GameServerPort,
	const FString& LevelId,
	float TimeoutSeconds,
	FOnGameServerResponse OnResponse)
{
	// Capture everything by value; no UObject pointers cross the thread boundary.
	Async(EAsyncExecution::ThreadPool,
		[GameServerAddress, GameServerPort, LevelId, TimeoutSeconds, OnResponse]()
	{
		using namespace GameServerClientInternal;

		const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const double RequestStartSeconds = FPlatformTime::Seconds();

		FGameServerResponse Result;
		const FTimespan Timeout = FTimespan::FromSeconds(FMath::Max(1.0f, TimeoutSeconds));

		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] RequestServer begin host=%s port=%d levelId='%s' timeout=%.2fs"),
			*RequestId, *GameServerAddress, GameServerPort, *LevelId, TimeoutSeconds);

		// -----------------------------------------------------------------
		// 1. Get socket subsystem
		// -----------------------------------------------------------------
		ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!SS)
		{
			Result.ErrorMessage = TEXT("Socket subsystem is unavailable");
			UE_LOG(LogGameServerClient, Error, TEXT("[Req=%s] %s"), *RequestId, *Result.ErrorMessage);
			DispatchResult(OnResponse, MoveTemp(Result));
			return;
		}

		// -----------------------------------------------------------------
		// 2. Resolve the game server address
		// -----------------------------------------------------------------
		bool bIpValid = false;
		TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
		Addr->SetIp(*GameServerAddress, bIpValid);

		if (!bIpValid)
		{
			UE_LOG(LogGameServerClient, Display,
				TEXT("[Req=%s] Host '%s' is not a direct IP, attempting DNS resolve"),
				*RequestId, *GameServerAddress);

			// Attempt DNS resolve.
			FAddressInfoResult GAI = SS->GetAddressInfo(
				*GameServerAddress, nullptr,
				EAddressInfoFlags::Default, NAME_None);

			if (GAI.ReturnCode != SE_NO_ERROR || GAI.Results.IsEmpty())
			{
				Result.ErrorMessage = FString::Printf(
					TEXT("Failed to resolve game server host: %s"), *GameServerAddress);
				UE_LOG(LogGameServerClient, Error,
					TEXT("[Req=%s] DNS resolve failed for host '%s' (returnCode=%d): %s"),
					*RequestId, *GameServerAddress, GAI.ReturnCode, *Result.ErrorMessage);
				DispatchResult(OnResponse, MoveTemp(Result));
				return;
			}
			Addr = GAI.Results[0].Address->Clone();
			UE_LOG(LogGameServerClient, Display,
				TEXT("[Req=%s] DNS resolve succeeded host='%s'"),
				*RequestId, *GameServerAddress);
		}

		Addr->SetPort(GameServerPort);

		// -----------------------------------------------------------------
		// 3. Create TCP socket
		// -----------------------------------------------------------------
		FSocket* Socket = SS->CreateSocket(
			NAME_Stream,
			TEXT("AuraGameServerClient"),
			Addr->GetProtocolType());

		if (!Socket)
		{
			Result.ErrorMessage = TEXT("Failed to create TCP socket");
			UE_LOG(LogGameServerClient, Error,
				TEXT("[Req=%s] %s | LastSocketError=%s"),
				*RequestId, *Result.ErrorMessage, *SocketErrorToString(SS));
			DispatchResult(OnResponse, MoveTemp(Result));
			return;
		}

		// Use a lambda to guarantee socket cleanup in every code path.
		auto CleanupAndDispatch = [&](FGameServerResponse&& Res)
		{
			UE_LOG(LogGameServerClient, Warning,
				TEXT("[Req=%s] Failing request after %.3fs | Reason=%s | LastSocketError=%s"),
				*RequestId,
				FPlatformTime::Seconds() - RequestStartSeconds,
				*Res.ErrorMessage,
				*SocketErrorToString(SS));
			SS->DestroySocket(Socket);
			Socket = nullptr;
			DispatchResult(OnResponse, MoveTemp(Res));
		};

		// -----------------------------------------------------------------
		// 4. Non-blocking connect with timeout
		// -----------------------------------------------------------------
		Socket->SetNonBlocking(true);
		const bool bImmediateConnect = Socket->Connect(*Addr);
		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] Connect initiated to %s:%d (immediateResult=%s)"),
			*RequestId,
			*GameServerAddress,
			GameServerPort,
			bImmediateConnect ? TEXT("true") : TEXT("false"));

		bool bConnected = bImmediateConnect;
		if (!bConnected)
		{
			// For non-blocking sockets, WaitForWrite indicates connect completion.
			const bool bWriteReady = Socket->Wait(ESocketWaitConditions::WaitForWrite, Timeout);
			const ESocketConnectionState ConnState = Socket->GetConnectionState();

			UE_LOG(LogGameServerClient, Display,
				TEXT("[Req=%s] Connect wait result writeReady=%s connState=%d"),
				*RequestId,
				bWriteReady ? TEXT("true") : TEXT("false"),
				static_cast<int32>(ConnState));

			bConnected = bWriteReady && ConnState == SCS_Connected;
		}

		if (!bConnected)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Could not connect to Game Server Manager at %s:%d (timeout=%.1fs)"),
				*GameServerAddress, GameServerPort, TimeoutSeconds);
			CleanupAndDispatch(MoveTemp(Result));
			return;
		}

		Socket->SetNonBlocking(false);

		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] Connected to Game Server Manager %s:%d (elapsed=%.3fs)"),
			*RequestId,
			*GameServerAddress,
			GameServerPort,
			FPlatformTime::Seconds() - RequestStartSeconds);

		// -----------------------------------------------------------------
		// 5. Send JSON request
		// -----------------------------------------------------------------
		// Sanitise LevelId: strip characters that would break the JSON string.
		FString SafeLevelId = LevelId;
		SafeLevelId.ReplaceInline(TEXT("\""), TEXT("_"));
		SafeLevelId.ReplaceInline(TEXT("\\"), TEXT("_"));
		FString ClientExecutable = FPlatformProcess::ExecutableName(false);
		ClientExecutable.ReplaceInline(TEXT("\""), TEXT("_"));
		ClientExecutable.ReplaceInline(TEXT("\\"), TEXT("/"));
		FString ClientEngineRoot = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
		ClientEngineRoot.ReplaceInline(TEXT("\""), TEXT("_"));
		ClientEngineRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
		const uint32 ClientNetworkChangelist = FNetworkVersion::GetNetworkCompatibleChangelist();
		const uint32 ClientNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();

		const FString RequestJson = FString::Printf(
			TEXT("{\"action\":\"request_server\",\"levelId\":\"%s\",\"clientExecutable\":\"%s\",\"clientEngineRoot\":\"%s\",\"clientNetworkChangelist\":%u,\"clientNetworkVersion\":%u}\n"),
			*SafeLevelId,
			*ClientExecutable,
			*ClientEngineRoot,
			ClientNetworkChangelist,
			ClientNetworkVersion);

		const TArray<uint8> RequestBytes = StringToUtf8(RequestJson);

		// Wait until the socket is writable before sending.
		if (!Socket->Wait(ESocketWaitConditions::WaitForWrite, Timeout))
		{
			Result.ErrorMessage = TEXT("Timed out waiting to send request to game server");
			CleanupAndDispatch(MoveTemp(Result));
			return;
		}

		int32 BytesSent = 0;
		if (!Socket->Send(RequestBytes.GetData(), RequestBytes.Num(), BytesSent)
			|| BytesSent != RequestBytes.Num())
		{
			Result.ErrorMessage = TEXT("Failed to send request to Game Server Manager");
			CleanupAndDispatch(MoveTemp(Result));
			return;
		}

		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] Sent request bytes=%d levelId='%s' executable='%s' engine='%s' netCL=%u checksum=%u"),
			*RequestId,
			RequestBytes.Num(),
			*LevelId,
			*ClientExecutable,
			*ClientEngineRoot,
			ClientNetworkChangelist,
			ClientNetworkVersion);

		// -----------------------------------------------------------------
		// 6. Read newline-delimited JSON response
		//    The game server may need time to start the DS, so we use the
		//    full TimeoutSeconds budget for the read as well.
		// -----------------------------------------------------------------
		FString ResponseLine;
		if (!ReadLine(Socket, Timeout, ResponseLine, RequestId))
		{
			Result.ErrorMessage = TEXT("No response received from Game Server Manager (timeout or connection closed)");
			CleanupAndDispatch(MoveTemp(Result));
			return;
		}

		SS->DestroySocket(Socket);
		Socket = nullptr;

		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] Received response bytes=%d payload=%s"),
			*RequestId,
			ResponseLine.Len(),
			*ResponseLine);

		// -----------------------------------------------------------------
		// 7. Parse response JSON
		// -----------------------------------------------------------------
		TSharedPtr<FJsonObject> ResponseObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseLine);
		if (!FJsonSerializer::Deserialize(Reader, ResponseObj) || !ResponseObj.IsValid())
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Invalid JSON in game server response: %s"), *ResponseLine);
			UE_LOG(LogGameServerClient, Error,
				TEXT("[Req=%s] %s"), *RequestId, *Result.ErrorMessage);
			DispatchResult(OnResponse, MoveTemp(Result));
			return;
		}

		FString Status;
		ResponseObj->TryGetStringField(TEXT("status"), Status);

		if (Status == TEXT("ready"))
		{
			ResponseObj->TryGetStringField(TEXT("host"), Result.Host);

			double PortValue = 0.0;
			ResponseObj->TryGetNumberField(TEXT("port"), PortValue);
			Result.Port = static_cast<int32>(PortValue);

			if (Result.Host.IsEmpty() || Result.Port <= 0 || Result.Port > 65535)
			{
				Result.bSuccess = false;
				Result.ErrorMessage = FString::Printf(
					TEXT("Game server returned invalid host/port: %s:%d"), *Result.Host, Result.Port);
				UE_LOG(LogGameServerClient, Error,
					TEXT("[Req=%s] %s"), *RequestId, *Result.ErrorMessage);
			}
			else
			{
				Result.bSuccess = true;
				UE_LOG(LogGameServerClient, Display,
					TEXT("[Req=%s] Game server ready: %s:%d (total_elapsed=%.3fs)"),
					*RequestId,
					*Result.Host,
					Result.Port,
					FPlatformTime::Seconds() - RequestStartSeconds);
			}
		}
		else
		{
			Result.bSuccess = false;
			ResponseObj->TryGetStringField(TEXT("message"), Result.ErrorMessage);
			if (Result.ErrorMessage.IsEmpty())
			{
				Result.ErrorMessage = FString::Printf(
					TEXT("Game server returned status: %s"), *Status);
			}
			UE_LOG(LogGameServerClient, Warning,
				TEXT("[Req=%s] Game server error status='%s': %s"),
				*RequestId,
				*Status,
				*Result.ErrorMessage);
		}

		UE_LOG(LogGameServerClient, Display,
			TEXT("[Req=%s] RequestServer finished success=%s total_elapsed=%.3fs"),
			*RequestId,
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			FPlatformTime::Seconds() - RequestStartSeconds);

		DispatchResult(OnResponse, MoveTemp(Result));
	});
}
