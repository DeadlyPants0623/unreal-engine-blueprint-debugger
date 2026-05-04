// Copyright Epic Games, Inc. All Rights Reserved.

#include "BPHealthAnalyzerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBPHealthAnalyzerStyle::StyleInstance = nullptr;

void FBPHealthAnalyzerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBPHealthAnalyzerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBPHealthAnalyzerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BPHealthAnalyzerStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FBPHealthAnalyzerStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BPHealthAnalyzerStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BPHealthAnalyzer")->GetBaseDir() / TEXT("Resources"));

	Style->Set("BPHealthAnalyzer.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	return Style;
}

void FBPHealthAnalyzerStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBPHealthAnalyzerStyle::Get()
{
	return *StyleInstance;
}
