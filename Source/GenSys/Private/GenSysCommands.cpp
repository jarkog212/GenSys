// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenSysCommands.h"

#define LOCTEXT_NAMESPACE "FGenSysModule"

void FGenSysCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "GenSys", "Bring up GenSys window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
