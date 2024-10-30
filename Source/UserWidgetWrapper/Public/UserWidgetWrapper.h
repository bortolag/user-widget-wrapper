// Copyright Gianluca Bortolanza. All Rights Reserved.

#pragma once

#include <Modules/ModuleManager.h>

struct FWidgetReference;
class UWidgetTree;
class UWidget;

class FUserWidgetWrapperModule : public IModuleInterface
{

public:

	// Begin IModuleInterface override
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface override

protected:

	// Adds the plugin tool-bar button to the blueprint widget editor and register its related functions
	void RegisterMenus();
	
	// Creates a new User Widget's asset and moves the given widgets to that
	static void HandleWidgetsWrapping(const TSet<FWidgetReference>& InWidgets);

	// Creates an asset through a dialog and returns it
	static UObject* CreateAsset(const FName& InDefaultAssetName, const FName& InPackagePath, UClass* InAssetClass, UFactory* InFactory);

	// Moves the given widgets from a widgets tree to the other
	static void MoveWidgetsTo(const TSet<FWidgetReference>& InWidgets, UWidgetTree* InOldWidgetTree, UWidgetTree* InNewWidgetTree);

private:

	// Returns the first found widget tree that contains the given widgets
	static UWidgetTree* GetParentWidgetTree(const TSet<FWidgetReference>& InWidgets);

	// Returns true if a common root has been found for the given widgets
	static bool AreWidgetsOnSameRoot(const TSet<FWidgetReference>& InWidgets, const UWidgetTree* InWidgetTree);

	// Returns the common widget root for the given widgets
	static UWidget* GetRootWidget(const TSet<FWidgetReference>& InWidgets, const UWidgetTree* InWidgetTree);
	
};
