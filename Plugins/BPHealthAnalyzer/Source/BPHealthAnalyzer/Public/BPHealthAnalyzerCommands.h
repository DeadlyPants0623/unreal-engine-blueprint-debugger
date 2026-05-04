// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "BPHealthAnalyzerStyle.h"

class FBPHealthAnalyzerCommands : public TCommands<FBPHealthAnalyzerCommands>
{
public:

	FBPHealthAnalyzerCommands()
		: TCommands<FBPHealthAnalyzerCommands>(TEXT("BPHealthAnalyzer"), NSLOCTEXT("Contexts", "BPHealthAnalyzer", "BPHealthAnalyzer Plugin"), NAME_None, FBPHealthAnalyzerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};