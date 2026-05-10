#pragma once

#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;
class UToolMenu;
class UEdGraphNode;
class SExecLocalPathWidget;

class FBPExecFlowViewerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Convenience accessor — valid between StartupModule and ShutdownModule. */
	static FBPExecFlowViewerModule& Get()
	{
		return FModuleManager::GetModuleChecked<FBPExecFlowViewerModule>("BPExecFlowViewer");
	}

private:
	void RegisterMenuExtensions();
	void BuildContextMenuSection(UToolMenu* Menu);
	void TriggerLocalFlowView(UEdGraphNode* Node);
	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& Args);

private:
	TWeakPtr<SExecLocalPathWidget> ViewerWidgetPtr;
};
