#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"

// One issue found in blueprint
struct FBPIssue
{
	FString BlueprintName;
	FString GraphName;
	FString NodeName;
	FString IssueDescription;
	int32 Severity; // 0: warning, 1: error

	FGuid NodeGuid; // Unique per node instance; zero guid for variable/function issues

	TWeakObjectPtr<UBlueprint> SourceBlueprint;
	TWeakObjectPtr<UEdGraphNode> SourceNode;
};

class FBPAnalyzerLogic
{
public:
	// Main entry point - analyzes all Blueprints in the project
	static TArray<FBPIssue> AnalyzeAllBlueprints();
	
	// Analyze a single Blueprint Asset
	static TArray<FBPIssue> AnalyzeBlueprint(UBlueprint* Blueprint, const TSet<FString>& CrossBPCallSet = TSet<FString>());
	
private:
	// Individual checks
	static void CheckDisconnectedExecPins(UBlueprint* BP, UEdGraph* Graph, TArray<FBPIssue>& OutIssues);
	static void CheckUnusedVariables(UBlueprint* BP, TArray<FBPIssue>& OutIssues);
	static void CheckUnusedFunctions(UBlueprint* BP, const TSet<FString>& CrossBPCallSet, TArray<FBPIssue>& OutIssues);
	static void CheckNodesWithNoOutputConnections(UBlueprint* BP, UEdGraph* Graph, TArray<FBPIssue>& OutIssues);
};
