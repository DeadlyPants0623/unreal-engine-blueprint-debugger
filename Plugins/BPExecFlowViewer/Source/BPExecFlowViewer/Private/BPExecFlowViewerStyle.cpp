// Copyright CJ 2026 All Rights Reserved.

#include "BPExecFlowViewerStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FBPExecFlowViewerStyle::StyleSet;

void FBPExecFlowViewerStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	const FString PluginContentDir = IPluginManager::Get().FindPlugin(TEXT("BPExecFlowViewer"))->GetBaseDir() / TEXT("Resources");

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	StyleSet->SetContentRoot(PluginContentDir);

	const FVector2D Icon16Size(16.f, 16.f);
	const FVector2D Icon128Size(128.f, 128.f);
	StyleSet->Set(
		"BPExecFlowViewer.Icon16",
		new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icon16.png")), Icon16Size));
	StyleSet->Set(
		"BPExecFlowViewer.Icon128",
		new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icon128.png")), Icon128Size));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FBPExecFlowViewerStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

FName FBPExecFlowViewerStyle::GetStyleSetName()
{
	static FName StyleName(TEXT("BPExecFlowViewerStyle"));
	return StyleName;
}

const ISlateStyle& FBPExecFlowViewerStyle::Get()
{
	return *StyleSet;
}
