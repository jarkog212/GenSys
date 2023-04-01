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
		SECTION_TITLE(Noise)
		ARGUMENT_FIELD_NUMERIC(UserParams, Noise Octaves, ValueNoiseOctaves, "integer 0-5")
		ARGUMENT_FIELD_NUMERIC(UserParams, Blur Radius , BlurPixelRadius, "integer 0-5")
		ARGUMENT_FIELD_NUMERIC(UserParams, Noise Granularity , Granularity, "float 0-1")
		SECTION_TITLE(Terrain)
		ARGUMENT_FIELD_STRING(UserParams, Outline Texture Path, User_TerrainOutlineMap, "string full path (512x512)")
		ARGUMENT_FIELD_STRING(UserParams, Forced Level Texture Path ,User_TerrainFeatureMap, "string full path (512x512)")
		SECTION_TITLE(River / Erosion)
		ARGUMENT_FIELD_NUMERIC(UserParams,  River Iterations, RiverGenerationIterations, "--unused--")
		ARGUMENT_FIELD_NUMERIC(UserParams,  River Resolution, RiverResolution, "float 0-1 (technically 0.90 - 1)")
		ARGUMENT_FIELD_NUMERIC(UserParams, River Line Average Thickness, RiverThickness, "integer 0-inf")
		ARGUMENT_FIELD_NUMERIC(UserParams, River Erosion Strength, RiverStrengthFactor, "float 0-1")
		ARGUMENT_CHECKBOX(UserParams,  Allow Multiple Node Connections, RiverAllowNodeMismatch)
		ARGUMENT_CHECKBOX(UserParams, Allow Rivers To Erode Forced Level, RiversOnGivenFeatures)
		ARGUMENT_FIELD_STRING(UserParams, River Guide Texture Path, User_RiverOutline, "string full path (512x512)")
		SECTION_TITLE(Layers)
		ARGUMENT_FIELD_NUMERIC(UserParams, Number Of Terrain Layers, NumberOfTerrainLayers, "integer 1-4")
		SECTION_TITLE(Foliage)
		ARGUMENT_FIELD_NUMERIC(UserParams, Number Of Foliage Layers, NumberOfFoliageLayers, "integer 1-4")
		ARGUMENT_FIELD_NUMERIC(UserParams,  Foliage Emptyness, FoliageWholeness, "float 0-1")
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
	CallRunGensysFromEngine();
	CallRunGensysFromProject();
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
	StoragePath.Append("input.json");

	// save json
	std::ofstream File(TCHAR_TO_ANSI(*StoragePath));
	File << FileOut;
}

void FGenSysModule::SetupGensysContentFolder()
{
	const FString ProjectFolderAbs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir());

	// using system command to copy create the desired folder structure if it does not exist
	FString command = "cd " + ProjectFolderAbs + " && ";
	command.Append("mkdir Gensys && cd Gensys && mkdir MaterialFunctions");

	system(TCHAR_TO_ANSI(*command));
}

void FGenSysModule::MoveContentData()
{
	static const FString RelativeMaterialFunctionsFolder = "GenSys/Content/GensysMaterialFunctions";
	static const FString ProjectMaterialFunctions = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(RelativeMaterialFunctionsFolder);
	static const FString EngineMaterialFunctions = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(RelativeMaterialFunctionsFolder);;
	static const FString Destination = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectContentDir()).Append("Gensys/MaterialFunctions");

	// using system command to copy .uasset files from material function folder
	FString command = "xcopy /s \"";
	command.Append(ProjectMaterialFunctions);
	command.Append("\" \"");
	command.Append(Destination);
	command.Append("\"");

	system(TCHAR_TO_ANSI(*command));
}

void FGenSysModule::ImportGensysOutput()
{
	static const FString ProjectOutputs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectPluginsDir()).Append(PluginsRelativePath);
	static const FString EngineOutputs = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::EnginePluginsDir()).Append(PluginsRelativePath);

	// list of textures to import from the output folder
	ImportFile(ProjectOutputs + "FoliageMap.png", "", "FoliageMap");
	ImportFile(ProjectOutputs + "RiverErosionMap.png", "", "RiverErosionMap");
	ImportFile(ProjectOutputs + "TerrainLayersMap.png", "", "TerrainLayersMap");
	ImportFile(ProjectOutputs + "TerrainMap.png", "", "TerrainMap");
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