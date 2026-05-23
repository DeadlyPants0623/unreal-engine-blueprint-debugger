// Copyright CJ 2026 All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FBPExecFlowViewerStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
