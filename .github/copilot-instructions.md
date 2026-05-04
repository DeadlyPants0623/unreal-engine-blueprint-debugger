# GitHub Copilot Instructions — MyCity (UE5 Blueprint Debugger Plugin)

## Project Overview
This is an **Unreal Engine 5.7 C++ Editor Plugin** focused on:
- Blueprint execution flow debugging
- Graphing Blueprint execution paths (single and cross-Blueprint)
- Visualizing runtime Blueprint call stacks and execution traces
- Plugin systems — this is a standalone `.uplugin`, not a game project

---

## C++ Conventions — Always Follow These

### Memory Management
- Use UE smart pointers: `TSharedPtr`, `TSharedRef`, `TWeakPtr`, `TWeakObjectPtr`
- For UObjects: rely on GC — never manually `delete` a UObject
- Use `MakeShared<T>()` and `MakeUnique<T>()`, not `std::make_shared`
- Avoid raw owning pointers; raw non-owning pointers (`T*`) are fine for UObjects observed by GC

### UE Reflection System
- Always add `UCLASS()`, `USTRUCT()`, `UENUM()` macros to reflected types
- Include `GENERATED_BODY()` in every reflected class/struct
- Use `UPROPERTY()` for any member that needs GC tracking, serialization, or Blueprint exposure
- Use `UFUNCTION()` for any method exposed to Blueprint or bound to delegates
- Never skip `#include "FileName.generated.h"` as the last include

### Plugin-Specific Patterns
- Module startup/shutdown goes in `IModuleInterface::StartupModule()` / `ShutdownModule()`
- Register editor extensions (menus, toolbars, slate widgets) only inside `#if WITH_EDITOR` guards
- Use `FSlateApplication` and Slate widgets (`SWidget`, `SCompoundWidget`) for any editor UI
- Prefer `FBlueprintEditorUtils`, `FKismetDebugUtilities`, and `UBlueprintGeneratedClass` for Blueprint introspection
- For execution tracing, look at `FBlueprintCoreDelegates` and `FKismetDebugUtilities::NotifyDebugHandlerWithBreakpoint`
- Graph visualization should use `SGraphPanel` or a custom `SNodePanel` derivative

### Code Style
- Class names: `F` prefix for structs/non-UObject, `U` for UObject subclasses, `A` for Actors, `I` for interfaces, `S` for Slate widgets
- Follow UE naming — `bIsEnabled` for bools, `OnEventName` for delegates
- Keep `.h` files minimal — forward declare where possible to reduce compile times
- Split large classes into smaller focused ones; avoid god classes
- Use `UE_LOG(LogYourModule, ...)` with a custom log category, not `printf` or `std::cout`

---

## What Copilot Should Do

### Always
- Suggest UE5.7-compatible APIs — flag if an API was deprecated after 5.4
- Prefer engine delegate hooks over polling/ticking where possible (better for a debugger plugin)
- When editing Blueprint-facing code, remind to recompile Blueprints if the C++ interface changed
- Suggest const-correctness — `const TArray<T>&` params, `const` member functions
- Flag thread safety issues — Blueprint VM runs on game thread; any async work needs care

### When Suggesting Improvements
- Point out if a pattern will cause editor crashes (common with invalid UObject pointers in editor plugins)
- Suggest `TOptional<T>` over nullable pointers for optional values
- Recommend `FMessageLog` over `UE_LOG` for errors the user should see in the editor UI
- If touching graph node code, check whether `UEdGraphNode` or `UK2Node` is the right base

### Never
- Never suggest `std::` containers — use `TArray`, `TMap`, `TSet`, `TQueue`
- Never suggest `std::shared_ptr` / `std::unique_ptr` — use UE equivalents
- Never add `#include <iostream>` or any STL stream headers
- Never suggest `new` / `delete` for UObjects — use `NewObject<T>()` and let GC handle it
- Never suggest polling in `Tick()` when a delegate or callback exists
- Never use `Cast<T>` without checking the result before dereferencing

---

## Build & Compile Notes
- Module type is `Editor` — it will not ship with the game
- Use `LiveCoding` for hot reload when only `.cpp` changed; full recompile if `.h` changed
- If UBT errors mention "missing module", check `.Build.cs` `PublicDependencyModuleNames`
- Key dependencies likely needed: `"Kismet"`, `"KismetCompiler"`, `"BlueprintGraph"`, `"GraphEditor"`, `"Slate"`, `"SlateCore"`, `"EditorStyle"`, `"UnrealEd"`

---

## Debugging the Debugger
- Use `ensureMsgf()` over `check()` in plugin code — crashes the editor less aggressively
- Wrap editor-only logic in `GIsEditor` checks if there's any risk of it running outside the editor
- For graph rendering issues, check `FSlateApplication::IsInitialized()` before accessing Slate
- Blueprint VM execution hooks are fragile — always guard with `IsValid()` on UObject pointers before use
