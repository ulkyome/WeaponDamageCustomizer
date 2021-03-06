#include "WeaponDamageCustomizer.h"
#pragma comment(lib, "ArkApi.lib")
DECLARE_HOOK(APrimalCharacter_TakeDamage, float, APrimalCharacter*, float, FDamageEvent *, AController *, AActor *);

const std::string FilterTier(FString WeaponName)
{
	return WeaponName.Replace(L"Primitive", L"").Replace(L"Ramshackle", L"").Replace(L"Apprentice", L"").Replace(L"Journeyman", L"").Replace(L"Mastercraft", L"").Replace(L"Ascendant", L"").ToString();
}

float GetDamageMultiplier(const int Type, const FString& WeaponName)
{
	if (!JsonWepArray.empty())
	{
		const auto& Itr = std::find_if(JsonWepArray.begin(), JsonWepArray.end(), [Type, WeaponName](const nlohmann::json& curjson) -> bool { return (curjson.value("Type", 0) == Type || curjson.value("Type", 0) == 3) && (curjson.value("IgnoreTier", false) ? (curjson.value("ExactMatch", false) ? FilterTier(WeaponName) == curjson.value("WeaponName", "") : FilterTier(WeaponName).find(curjson.value("WeaponName", "")) != std::string::npos) : curjson.value("ExactMatch", false) ? WeaponName.ToString().find(curjson.value("WeaponName", "")) != std::string::npos : WeaponName.ToString() == curjson.value("WeaponName", "")); });
		if (Itr != JsonWepArray.end())
		{
			return Itr->value("Multiplier", 1.0f);
		}
	}
	return 1.0f;
}

float Hook_APrimalCharacter_TakeDamage(APrimalCharacter* _this, float Damage, FDamageEvent* DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	//Log::GetLog()->warn("TakeDamageCalled");
	if (_this && EventInstigator && !EventInstigator->IsLocalController() && EventInstigator->IsA(AShooterPlayerController::GetPrivateStaticClass()))
	{
		//Log::GetLog()->warn("Passed the first IF");
		AShooterPlayerController* AttackerShooterController = static_cast<AShooterPlayerController*>(EventInstigator);
		if (AttackerShooterController && AttackerShooterController->PlayerStateField() && AttackerShooterController->GetPlayerCharacter() && AttackerShooterController->GetPlayerCharacter()->CurrentWeaponField() && AttackerShooterController->GetPlayerCharacter()->CurrentWeaponField()->AssociatedPrimalItemField())
		{
			//Log::GetLog()->warn("Passed the second IF");
			FString WeaponName;
			AttackerShooterController->GetPlayerCharacter()->CurrentWeaponField()->AssociatedPrimalItemField()->GetItemName(&WeaponName, false, true, nullptr);

			const int Type = _this->IsA(APrimalDinoCharacter::GetPrivateStaticClass()) ? (_this->TargetingTeamField() < 10000 ? 1 : (_this->TargetingTeamField() > 10000 ? 2 : (_this->IsA(AShooterCharacter::GetPrivateStaticClass()) ? 0 : 3))) : 3;

			Damage = Damage * GetDamageMultiplier(Type, WeaponName);
			if (Config.value("Debug", false) && !WeaponName.IsEmpty())
			{
				Log::GetLog()->warn("Weapon Name: {}", WeaponName.ToString());
			}
		}
	}
	return APrimalCharacter_TakeDamage_original(_this, Damage, DamageEvent, EventInstigator, DamageCauser);
}

void ReadConfig()
{
	std::ifstream file(ArkApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/WeaponDamageCustomizer/config.json");
	if (!file.is_open())
		throw std::runtime_error("Can't open config.json");
	file >> Config;
	file.close();
	JsonWepArray = Config.value("WeaponDamageCustomizer", nlohmann::json::array());
}

void ReloadConfig(APlayerController* player_controller, FString*, bool)
{
	AShooterPlayerController* shooter_controller = static_cast<AShooterPlayerController*>(player_controller);

	try
	{
		ReadConfig();
	}
	catch (const std::exception& error)
	{
		ArkApi::GetApiUtils().SendServerMessage(shooter_controller, FColorList::Red, "Failed to reload config");

		Log::GetLog()->error(error.what());
		return;
	}

	ArkApi::GetApiUtils().SendServerMessage(shooter_controller, FColorList::Green, "Reloaded config");
}

void ReloadConfigRcon(RCONClientConnection* rcon_connection, RCONPacket* rcon_packet, UWorld*)
{
	FString reply;

	try
	{
		ReadConfig();
	}
	catch (const std::exception& error)
	{
		Log::GetLog()->error(error.what());

		reply = error.what();
		rcon_connection->SendMessageW(rcon_packet->Id, 0, &reply);
		return;
	}

	reply = "Reloaded config";
	rcon_connection->SendMessageW(rcon_packet->Id, 0, &reply);
}

void Load()
{
	Log::Get().Init("WeaponDamageCustomizer");

	try
	{
		ReadConfig();
	}
	catch (const std::exception& error)
	{
		Log::GetLog()->error(error.what());
		throw;
	}
	ArkApi::GetCommands().AddConsoleCommand("WeaponDamageCustomizer.Reload", &ReloadConfig);
	ArkApi::GetCommands().AddRconCommand("WeaponDamageCustomizer.Reload", &ReloadConfigRcon);
	ArkApi::GetHooks().SetHook("APrimalCharacter.TakeDamage", &Hook_APrimalCharacter_TakeDamage, &APrimalCharacter_TakeDamage_original);
}

void Unload()
{
	ArkApi::GetCommands().RemoveConsoleCommand("WeaponDamageCustomizer.Reload");
	ArkApi::GetCommands().RemoveRconCommand("WeaponDamageCustomizer.Reload");
	ArkApi::GetHooks().DisableHook("APrimalCharacter.TakeDamage", &Hook_APrimalCharacter_TakeDamage);
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Unload();
		break;
	}
	return TRUE;
}