// Copyright CJ 2026 All Rights Reserved.

#include "BPExecFlowViewer.h"

#include "BPExecFlowViewerStyle.h"
#include "SExecLocalPathWidget.h"
#include "SExecFlowGraphNode.h"
#include "EdGraphUtilities.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "GraphEditorModule.h"
#include "BlueprintEditorContext.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"
#include "Widgets/Docking/SDockTab.h"
#include "Editor.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "BPExecFlowViewer"

static const FName ExecFlowTabName("BPExecFlowViewer");
static TSharedPtr<FExecFlowGraphNodeFactory> GExecLocalNodeFactory;

// -----------------------------------------------------------------------
//  Helpers (file-local)
// -----------------------------------------------------------------------
namespace
{

bool IsValidFlowNode(const UEdGraphNode* Node)
{
	if (!Node) return false;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return true;
		}
	}
	return false;
}

FToolMenuSection& FindOrAddNodeActionsSection(UToolMenu* Menu)
{
	static const FName CandidateSections[] =
	{
		TEXT("NodeActions"),
		TEXT("EdGraphSchemaNodeActions"),
		TEXT("GraphNodeActions"),
		TEXT("K2NodeActions"),
		TEXT("BPExecFlowViewerActions")
	};

	for (const FName& SectionName : CandidateSections)
	{
		if (FToolMenuSection* Existing = Menu->FindSection(SectionName))
		{
			return *Existing;
		}
	}

	return Menu->AddSection(TEXT("BPExecFlowViewerActions"));
}

} // namespace

// -----------------------------------------------------------------------
//  Module lifecycle
// -----------------------------------------------------------------------

void FBPExecFlowViewerModule::StartupModule()
{
	FBPExecFlowViewerStyle::Initialize();

	// Register custom graph node factory for flow graph visualization
	GExecLocalNodeFactory = MakeShared<FExecFlowGraphNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(GExecLocalNodeFactory);

	// Register the nomad tab spawner
	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			ExecFlowTabName,
			FOnSpawnTab::CreateRaw(this, &FBPExecFlowViewerModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle",   "Exec Flow"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Local execution flow graph for the selected node"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Register context-menu extension (deferred until ToolMenus is ready)
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FBPExecFlowViewerModule::RegisterMenuExtensions));
}

TSharedRef<SDockTab> FBPExecFlowViewerModule::OnSpawnTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SExecLocalPathWidget> Widget = SNew(SExecLocalPathWidget);
	ViewerWidgetPtr = Widget;

	if (UEdGraphNode* Pending = PendingTargetNode.Get())
	{
		Widget->SetTargetNode(Pending);
		PendingTargetNode.Reset();
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Widget
		];
}

// -----------------------------------------------------------------------
//  Context menu extension
// -----------------------------------------------------------------------

void FBPExecFlowViewerModule::RegisterMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// These are the menu names actually fired for Blueprint nodes in UE 5.x
	static const FName MenuNames[] =
	{
		TEXT("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction"),
		TEXT("GraphEditor.GraphNodeContextMenu.K2Node_Event"),
		TEXT("GraphEditor.GraphNodeContextMenu.K2Node_CustomEvent"),
		TEXT("GraphEditor.GraphNodeContextMenu.K2Node_FunctionEntry"),
		TEXT("GraphEditor.GraphNodeContextMenu.K2Node_MacroInstance"),
		// Fallback for generic common menu
		TEXT("GraphEditor.GraphContextMenu.Common"),
	};

	for (const FName& MenuName : MenuNames)
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
		if (!Menu) continue;

		Menu->AddDynamicSection(
			TEXT("BPExecLocalPathSection"),
			FNewToolMenuDelegate::CreateRaw(
				this, &FBPExecFlowViewerModule::BuildContextMenuSection));
	}
}

void FBPExecFlowViewerModule::BuildContextMenuSection(UToolMenu* Menu)
{
	// Resolve the right-clicked node
	const UEdGraphNode* Node = nullptr;

	if (UGraphNodeContextMenuContext* Ctx = Menu->FindContext<UGraphNodeContextMenuContext>())
	{
		Node = Ctx->Node;
	}

	if (!Node && GEditor)
	{
		if (USelection* Sel = GEditor->GetSelectedObjects())
		{
			Node = Cast<UEdGraphNode>(Sel->GetTop(UEdGraphNode::StaticClass()));
		}
	}

	if (!Node || !IsValidFlowNode(Node)) return;

	TWeakObjectPtr<const UEdGraphNode> WeakNode(Node);

	FToolMenuSection& Section = FindOrAddNodeActionsSection(Menu);
	Section.AddSeparator(NAME_None);
	Section.AddMenuEntry(
		TEXT("ViewLocalExecPath"),
		LOCTEXT("ViewLocalExecPath",        "View Exec Flow"),
		LOCTEXT("ViewLocalExecPathTooltip", "Show local execution flow for this node"),
		FSlateIcon(FBPExecFlowViewerStyle::GetStyleSetName(), "BPExecFlowViewer.Icon16"),
		FUIAction(FExecuteAction::CreateLambda(
			[this, WeakNode]()
			{
				UEdGraphNode* ClickedNode = const_cast<UEdGraphNode*>(WeakNode.Get());

				// Fallback: re-resolve from selection
				if (!ClickedNode && GEditor)
				{
					if (USelection* Sel = GEditor->GetSelectedObjects())
					{
						ClickedNode = Cast<UEdGraphNode>(
							Sel->GetTop(UEdGraphNode::StaticClass()));
					}
				}

				if (ClickedNode && IsValidFlowNode(ClickedNode))
				{
					TriggerLocalFlowView(ClickedNode);
				}
			}))
	);
}

// -----------------------------------------------------------------------
//  Trigger
// -----------------------------------------------------------------------

void FBPExecFlowViewerModule::TriggerLocalFlowView(UEdGraphNode* Node)
{
	if (!Node)
	{
		return;
	}

	PendingTargetNode = Node;
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ExecFlowTabName));

	if (TSharedPtr<SExecLocalPathWidget> Widget = ViewerWidgetPtr.Pin())
	{
		Widget->SetTargetNode(Node);
		PendingTargetNode.Reset();
	}
}

// -----------------------------------------------------------------------
//  Shutdown
// -----------------------------------------------------------------------

void FBPExecFlowViewerModule::ShutdownModule()
{
	PendingTargetNode.Reset();
	ViewerWidgetPtr.Reset();

	FBPExecFlowViewerStyle::Shutdown();

	if (GExecLocalNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GExecLocalNodeFactory);
		GExecLocalNodeFactory.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExecFlowTabName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBPExecFlowViewerModule, BPExecFlowViewer)


