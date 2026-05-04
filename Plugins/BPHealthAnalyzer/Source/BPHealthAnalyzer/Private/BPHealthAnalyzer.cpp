// Copyright Epic Games, Inc. All Rights Reserved.

#include "BPHealthAnalyzer.h"
#include "BPHealthAnalyzerStyle.h"
#include "BPHealthAnalyzerCommands.h"
#include "SBPHealthAnalyzerWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

static const FName BPHealthAnalyzerTabName("BPHealthAnalyzer");

#define LOCTEXT_NAMESPACE "FBPHealthAnalyzerModule"

void FBPHealthAnalyzerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBPHealthAnalyzerStyle::Initialize();
	FBPHealthAnalyzerStyle::ReloadTextures();

	FBPHealthAnalyzerCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBPHealthAnalyzerCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FBPHealthAnalyzerModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBPHealthAnalyzerModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BPHealthAnalyzerTabName, FOnSpawnTab::CreateRaw(this, &FBPHealthAnalyzerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBPHealthAnalyzerTabTitle", "BPHealthAnalyzer"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBPHealthAnalyzerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBPHealthAnalyzerStyle::Shutdown();

	FBPHealthAnalyzerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BPHealthAnalyzerTabName);
}

TSharedRef<SDockTab> FBPHealthAnalyzerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBPHealthAnalyzerWidget)
		];
}

void FBPHealthAnalyzerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BPHealthAnalyzerTabName);
}

void FBPHealthAnalyzerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBPHealthAnalyzerCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBPHealthAnalyzerCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBPHealthAnalyzerModule, BPHealthAnalyzer)