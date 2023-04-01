// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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
	FReply RunGensys();
	
private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;

	// Gensys files constants
	const FString PluginsRelativePath = "GenSys/Resources/GenSysCoreShell/";
	const FString ExecutableName = "CoreTester.exe";

	void CallRunGensysFromProject();
	void CallRunGensysFromEngine();
	void ExportParamsIntoJson(const FString& Path = "");
	void ImportFile(const FString& In, const FString& RelativeDest, const FString& Filename);
	void SetupGensysContentFolder();
	void MoveContentData();
	void ImportGensysOutput();
};
