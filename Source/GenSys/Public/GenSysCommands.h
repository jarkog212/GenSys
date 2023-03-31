// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "GenSysStyle.h"

class FGenSysCommands : public TCommands<FGenSysCommands>
{
public:

	FGenSysCommands()
		: TCommands<FGenSysCommands>(TEXT("GenSys"), NSLOCTEXT("Contexts", "GenSys", "GenSys Plugin"), NAME_None, FGenSysStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};