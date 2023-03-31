// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenSys.h"
#include "GenSysStyle.h"
#include "GenSysCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Interfaces/IPluginManager.h"
#include "Windows/WindowsSystemIncludes.h"
#include "DataTypes.h"
#include "SlateMacroLibrary.h"

#include <fstream>

//external JSON library by nlohmann
#include "json.hpp"

using json = nlohmann::json;

static const FName GenSysTabName("GenSys");

#define LOCTEXT_NAMESPACE "FGenSysModule"

void FGenSysModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FGenSysStyle::Initialize();
	FGenSysStyle::ReloadTextures();

	FGenSysCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FGenSysCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FGenSysModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGenSysModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GenSysTabName, FOnSpawnTab::CreateRaw(this, &FGenSysModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FGenSysTabTitle", "GenSys"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FGenSysModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FGenSysStyle::Shutdown();

	FGenSysCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GenSysTabName);
}

TSharedRef<SDockTab> FGenSysModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	[
		SNew(SVerticalBox).RenderTransform(FSlateRenderTransform(7.0f))
		SECTION_TITLE(Noise)
		ARGUMENT_FIELD(ValueNoiseOctaves, "integer 0-5")
		ARGUMENT_FIELD(BlurPixelRadius, "integer 0-5")
		ARGUMENT_FIELD(Granularity, "float 0-1")
		SECTION_TITLE(Terrain)
		ARGUMENT_FIELD(User_TerrainOutlineMap, "string full path (512x512)")
		ARGUMENT_FIELD(User_TerrainFeatureMap, "string full path (512x512)")
		SECTION_TITLE(River / Erosion)
		ARGUMENT_FIELD(RiverGenerationIterations, "--unused--")
		ARGUMENT_FIELD(RiverResolution, "float 0-1 (technically 0.90 - 1)")
		ARGUMENT_FIELD(RiverThickness, "integer 0-inf")
		ARGUMENT_FIELD(RiverStrengthFactor, "float 0-1")
		ARGUMENT_CHECKBOX(RiverAllowNodeMismatch)
		ARGUMENT_CHECKBOX(RiversOnGivenFeatures)
		ARGUMENT_FIELD(User_RiverOutline, "string full path (512x512)")
		SECTION_TITLE(Layers)
		ARGUMENT_FIELD(NumberOfTerrainLayers, "integer 1-4")
		SECTION_TITLE(Foliage)
		ARGUMENT_FIELD(NumberOfFoliageLayers, "integer 1-4")
		ARGUMENT_FIELD(FoliageWholeness, "float 0-1")
		ARGUMENT_FIELD(MinUnitFoliageHeight, "float 0-1")
	];
}

void FGenSysModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(GenSysTabName);

	ExportParamsIntoJson();

	CallRunGensysFromEngine();
	CallRunGensysFromProject();
}

void FGenSysModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FGenSysCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FGenSysCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FGenSysModule::CallRunGensysFromProject()
{
	// Determine paths to the core exe
	const FString GensysPathProjectPlugins = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath);

	// Append strings into a single command
	FString command = "cd ";
	command.Append(GensysPathProjectPlugins);
	command.Append(" && ");
	command.Append(ExecutableName);

	// Have Windows system run the Gensys core process
	system(TCHAR_TO_ANSI(*command));
}

void FGenSysModule::CallRunGensysFromEngine()
{
	// Determine paths to the core exe
	const FString GensysPathEnginePlugins = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(PluginsRelativePath);

	// Append strings into a single command
	FString command = "cd ";
	command.Append(GensysPathEnginePlugins);
	command.Append(" && ");
	command.Append(ExecutableName);

	// Have Windows system run the Gensys core process
	system(TCHAR_TO_ANSI(*command));
}

#define PARSE_TO_JSON(input, dest, value) \
	dest.emplace(#value, input.value);

extern GensysParameters UserParams;
void FGenSysModule::ExportParamsIntoJson(const FString& path)
{
	json FileOut;

	PARSE_TO_JSON(UserParams, FileOut, ValueNoiseOctaves)
	PARSE_TO_JSON(UserParams, FileOut, BlurPixelRadius)
	PARSE_TO_JSON(UserParams, FileOut, Granularity)
	PARSE_TO_JSON(UserParams, FileOut, RiverGenerationIterations)
	PARSE_TO_JSON(UserParams, FileOut, RiverResolution)
	PARSE_TO_JSON(UserParams, FileOut, RiverThickness)
	PARSE_TO_JSON(UserParams, FileOut, RiverAllowNodeMismatch)
	PARSE_TO_JSON(UserParams, FileOut, RiversOnGivenFeatures)
	PARSE_TO_JSON(UserParams, FileOut, RiverStrengthFactor)
	PARSE_TO_JSON(UserParams, FileOut, NumberOfTerrainLayers)
	PARSE_TO_JSON(UserParams, FileOut, NumberOfFoliageLayers)
	PARSE_TO_JSON(UserParams, FileOut, FoliageWholeness)
	PARSE_TO_JSON(UserParams, FileOut, MinUnitFoliageHeight)
	PARSE_TO_JSON(UserParams, FileOut, User_TerrainOutlineMap)
	PARSE_TO_JSON(UserParams, FileOut, User_TerrainFeatureMap)
	PARSE_TO_JSON(UserParams, FileOut, User_RiverOutline)

	FString StoragePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath);
	StoragePath.Append("/input.json");

	// save json
	std::ofstream File(TCHAR_TO_ANSI(*StoragePath));
	File << FileOut;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGenSysModule, GenSys)