// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "DataTypes.h"


class FToolBarBuilder;
class FMenuBuilder;

class FGenSysModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:

	void RegisterMenus();

	void CallRunGensysFromProject();
	void CallRunGensysFromEngine();
	void ExportParamsIntoJson(const FString& path = "");


	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	GensysParameters Params;

	// Gensys files constants
	const FString PluginsRelativePath = "GenSys/Binaries/GenSysCoreShell";
	const FString ExecutableName = "CoreTester.exe";
};
