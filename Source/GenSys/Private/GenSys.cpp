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
#include "AssetImportTask.h"
#include "AssetToolsModule.h"

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

	SetupGensysContentFolder();
	MoveContentData();
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

extern GensysParameters UserParams;
TSharedRef<SDockTab> FGenSysModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	[
		SNew(SVerticalBox).RenderTransform(FSlateRenderTransform(0.95f))
		SECTION_TITLE(General)
		ARGUMENT_FIELD_STRING(UserParams, Identifier, Identifier, "landscape identifier")
		SECTION_TITLE(Noise)
		ARGUMENT_FIELD_NUMERIC(UserParams, Noise Octaves, ValueNoiseOctaves, "integer 0-5")
		ARGUMENT_FIELD_NUMERIC(UserParams, Blur Radius , BlurPixelRadius, "integer 0-5")
		ARGUMENT_FIELD_NUMERIC(UserParams, Noise Granularity , Granularity, "float 0-1")
		SECTION_TITLE(Terrain)
		ARGUMENT_FIELD_STRING(UserParams, Outline Texture Path, User_TerrainOutlineMap, "string full path (512x512)")
		ARGUMENT_FIELD_STRING(UserParams, Forced Level Texture Path ,User_TerrainFeatureMap, "string full path (512x512)")
		SECTION_TITLE(River / Erosion)
		ARGUMENT_FIELD_NUMERIC(UserParams, River Iterations, RiverGenerationIterations, "--unused--")
		ARGUMENT_FIELD_NUMERIC(UserParams, River Resolution, RiverResolution, "float 0-1 (technically 0.90 - 1)")
		ARGUMENT_FIELD_NUMERIC(UserParams, River Line Average Thickness, RiverThickness, "integer 0-inf")
		ARGUMENT_FIELD_NUMERIC(UserParams, River Erosion Strength, RiverStrengthFactor, "float 0-1")
		ARGUMENT_CHECKBOX(UserParams, Allow Multiple Node Connections, RiverAllowNodeMismatch)
		ARGUMENT_CHECKBOX(UserParams, Allow Rivers To Erode Forced Level, RiversOnGivenFeatures)
		ARGUMENT_FIELD_STRING(UserParams, River Guide Texture Path, User_RiverOutline, "string full path (512x512)")
		SECTION_TITLE(Layers)
		ARGUMENT_FIELD_NUMERIC(UserParams, Number Of Terrain Layers, NumberOfTerrainLayers, "integer 1-4")
		SECTION_TITLE(Foliage)
		ARGUMENT_FIELD_NUMERIC(UserParams, Number Of Foliage Layers, NumberOfFoliageLayers, "integer 1-4")
		ARGUMENT_FIELD_NUMERIC(UserParams, Foliage Emptyness, FoliageWholeness, "float 0-1")
		ARGUMENT_FIELD_NUMERIC(UserParams, Minimum Height For Foliage (unit), MinUnitFoliageHeight, "float 0-1")
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 30))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.OnClicked_Raw(this, &FGenSysModule::RunGensys)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Generate!"))
				]
			]
			+ SHorizontalBox::Slot()
		]
	];
}

void FGenSysModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(GenSysTabName);
}

FReply FGenSysModule::RunGensys()
{
	ExportParamsIntoJson();
	RunGensysShell();
	ImportGensysOutput();
	return FReply::Handled();
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

void FGenSysModule::RunGensysShell()
{
	static const FString ProjectExePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath + ExecutableName);
	static const bool IsInProjectFolder = FPaths::FileExists(ProjectExePath);

	static const FString GensysPathProjectPlugins = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath);
	static const FString GensysPathEnginePlugins = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(PluginsRelativePath);

	static const FString ExePath = IsInProjectFolder ? GensysPathProjectPlugins : GensysPathEnginePlugins;
	// Append strings into a single command
	FString command = "cd ";
	command.Append(ExePath);
	command.Append(" && ");
	command.Append(ExecutableName);

	// Have Windows system run the Gensys core process
	system(TCHAR_TO_ANSI(*command));
}

#define PARSE_TO_JSON(input, dest, value) \
	dest.emplace(#value, input.value);

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
	if (!FPaths::FileExists(StoragePath + ExecutableName))
		StoragePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(PluginsRelativePath);

	StoragePath.Append("input.json");

	// save json
	std::ofstream File(TCHAR_TO_ANSI(*StoragePath));
	File << FileOut;
}

void FGenSysModule::SetupGensysContentFolder()
{
	const FString ProjectFolderAbs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir());

	// using system command to copy create the desired folder structure if it does not exist
	FString command = "cd " + ProjectFolderAbs + " && " + "mkdir Gensys";
	command.Append(" && cd Gensys && mkdir MaterialFunctions");
	command.Append(" && cd " + ProjectFolderAbs);
	command.Append(" && cd Gensys && mkdir Materials");

	system(TCHAR_TO_ANSI(*command));
}

void FGenSysModule::MoveContentData()
{
	const FString ProjectExePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath + ExecutableName);
	const bool IsInProjectFolder = FPaths::FileExists(ProjectExePath);

	const FString RelativeMaterialFunctionsFolder = "GenSys/Content/GensysMaterialFunctions";
	const FString ProjectMaterialFunctions = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(RelativeMaterialFunctionsFolder);
	const FString EngineMaterialFunctions = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(RelativeMaterialFunctionsFolder);;
	const FString Destination = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir()).Append("Gensys/MaterialFunctions");

	// using system command to copy .uasset files from Engine/Project material function folder (if available)
	FString command = "xcopy /S /Y \"";
	if (IsInProjectFolder)
		command.Append(ProjectMaterialFunctions);
	else
		command.Append(EngineMaterialFunctions);

	command.Append("\" \"");
	command.Append(Destination);
	command.Append("\"");

	system(TCHAR_TO_ANSI(*command));

	const FString RelativeMaterialsFolder = "GenSys/Content/GensysMaterials";
	const FString ProjectMaterial = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(RelativeMaterialsFolder);
	const FString EngineMaterial = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(RelativeMaterialsFolder);
	const FString MaterialDestination = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir()).Append("Gensys/Materials");

	// using system command to copy .uasset files from Engine material folder (if available)
	command = "xcopy /S /Y \"";
	if (IsInProjectFolder)
		command.Append(ProjectMaterial);
	else
		command.Append(EngineMaterial);

	command.Append("\" \"");
	command.Append(MaterialDestination);
	command.Append("\"");

	system(TCHAR_TO_ANSI(*command));
}

void FGenSysModule::ImportGensysOutput()
{
	static const FString ProjectContentPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir());
	static const FString ProjectExePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath + ExecutableName);
	static const bool IsInProjectFolder = FPaths::FileExists(ProjectExePath);

	static const FString ProjectOutputs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath);
	static const FString EngineOutputs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(PluginsRelativePath);
	static const FString InputFolder = IsInProjectFolder ? ProjectOutputs : EngineOutputs;

	// The out put files to consider for removal 
	static const TArray<FString> OutputFiles = {
		"FoliageMap",
		"RiverErosionMap",
		"TerrainLayersMap",
		"TerrainMap"
	};

	const FString Destination = ProjectContentPath + "Gensys/" + UserParams.Identifier.data();

	// make the relevant landscape folder in content
	FString command = "";
	command.Append("cd " + ProjectContentPath + "Gensys && mkdir " + UserParams.Identifier.data());

	// Fix paths to use backslashes
	for(auto &character : command)
	{
		if (character == '/')
			character = '\\';
	}

	system(TCHAR_TO_ANSI(*command));

	// copy the output files into the relevant content folder
	command = "";
	for(auto& fileName : OutputFiles)
		command.Append("xcopy ,Y \"" + InputFolder + fileName + ".png*" + "\" \"" + Destination + "\\" + fileName + ".png*\" && ");

	command.RemoveFromEnd("&& ");

	// Fix paths to use backslashes and replace , with forward slashes
	// Needed by Windows XCOPY command
	for (auto& character : command)
	{
		if (character == '/')
			character = '\\';

		if (character == ',')
			character = '/';
	}

	system(TCHAR_TO_ANSI(*command));

	// list of textures to import from the engine output folder (if available)
	for (auto& fileName : OutputFiles)
		ImportFile(Destination + "/" + fileName + ".png", FString(UserParams.Identifier.data()) + "/", fileName);
}

void FGenSysModule::ImportFile(const FString& In, const FString& RelativeDest, const FString& Filename)
{
	const FString GensysImportDest = "/Game/Gensys/" + RelativeDest + Filename;
	UAssetImportTask* importRequest = NewObject<UAssetImportTask>();

	if (importRequest == nullptr)
		return;

	// specify the import rules
	importRequest->Filename = In;
	importRequest->DestinationPath = FPaths::GetPath(GensysImportDest);
	importRequest->DestinationName = FPaths::GetCleanFilename(GensysImportDest);
	importRequest->bSave = true;
	importRequest->bAutomated = true;
	importRequest->bReplaceExisting = true;
	importRequest->bReplaceExistingSettings = false;

	// load the engine module responsible for assets handling
	FAssetToolsModule* AssetTools = FModuleManager::LoadModulePtr<FAssetToolsModule>("AssetTools");

	if (AssetTools == nullptr)
		return;

	// add the task for engine to execute
	AssetTools->Get().ImportAssetTasks({ importRequest });
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGenSysModule, GenSys)