// Copyright Epic Games, Inc. All Rights Reserved.

#include "BPHealthAnalyzerCommands.h"

#define LOCTEXT_NAMESPACE "FBPHealthAnalyzerModule"

void FBPHealthAnalyzerCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "BPHealthAnalyzer", "Bring up BPHealthAnalyzer window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
