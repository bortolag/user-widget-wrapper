// Copyright Gianluca Bortolanza. All Rights Reserved.

#include "UserWidgetWrapper.h"

#include <AssetPropertyTagCache.h>
#include <AssetToolsModule.h>
#include <Blueprint/WidgetTree.h>
#include <BlueprintEditorContext.h>
#include <ContentBrowserItemPath.h>
#include <ContentBrowserModule.h>
#include <IContentBrowserSingleton.h>
#include <Templates/WidgetTemplateBlueprintClass.h>
#include <UObject/GCObjectScopeGuard.h>
#include <WidgetBlueprintEditor.h>
#include <WidgetBlueprintEditorUtils.h>
#include <WidgetBlueprintFactory.h>

DEFINE_LOG_CATEGORY_STATIC(LogUserWidgetWrapper, Log, All);

#define LOCTEXT_NAMESPACE "FUserWidgetWrapperModule"

namespace UserWidgetWrapper
{
	constexpr const TCHAR* GamePath = TEXT("/Game");
	const FName AssetsToolModuleName = FName(TEXT("AssetTools"));
	const FName ContentBrowserModuleName = FName(TEXT("ContentBrowser"));
	const FName SlateIconName = FName(TEXT("FontEditor.ExportAllPages"));
	const FName UserWidgetWrapperName = FName(TEXT("UserWidgetWrapper"));
	const FName WidgetBlueprintEditorToolBarName = FName(TEXT("AssetEditor.WidgetBlueprintEditor.ToolBar.DesignerName"));
}

void FUserWidgetWrapperModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUserWidgetWrapperModule::RegisterMenus));
}

void FUserWidgetWrapperModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FUserWidgetWrapperModule::RegisterMenus()
{
	// Use the current object as the owner of the menus
	FToolMenuOwnerScoped OwnerScoped(this);
	
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(UserWidgetWrapper::WidgetBlueprintEditorToolBarName);
	if (!IsValid(ToolbarMenu))
	{
		UE_LOG(LogUserWidgetWrapper, Error, TEXT("Failed to register WidgetBlueprintEditor's toolbar: %s"), *UserWidgetWrapper::WidgetBlueprintEditorToolBarName.ToString());
		return;
	}
	
	FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection(UserWidgetWrapper::UserWidgetWrapperName);
	
	// Adds a dynamic entry that will be setup on the Widget Blueprint Editor opening
	ToolbarSection.AddDynamicEntry(UserWidgetWrapper::UserWidgetWrapperName, FNewToolMenuSectionDelegate::CreateLambda([] (FToolMenuSection& InSection)
	{
		const UBlueprintEditorToolMenuContext* Context = InSection.FindContext<UBlueprintEditorToolMenuContext>();

		InSection.AddEntry
		(
			FToolMenuEntry::InitToolBarButton
			(
				UserWidgetWrapper::UserWidgetWrapperName,
				FUIAction
				(
					FExecuteAction::CreateLambda([WeakContext = MakeWeakObjectPtr(Context)]
					{
						if (!WeakContext.IsValid())
						{
							UE_LOG(LogUserWidgetWrapper, Error, TEXT("Invalid Blueprint Editor Tool Menu's Context on User Widget Wrapper's action activation"));
							return;
						}
						
						if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = StaticCastSharedPtr<FWidgetBlueprintEditor>(WeakContext.Get()->BlueprintEditor.Pin()))
						{
							HandleWidgetsWrapping(WidgetBlueprintEditor->GetSelectedWidgets());
						}
					}),
					FCanExecuteAction::CreateLambda([WeakContext = MakeWeakObjectPtr(Context)]
					{
						if (!WeakContext.IsValid())
						{
							UE_LOG(LogUserWidgetWrapper, Error, TEXT("Invalid Blueprint Editor Tool Menu's Context when checking if the User Widget Wrapper's action can be executed"));
							return false;
						}
						
						if (TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor = StaticCastSharedPtr<FWidgetBlueprintEditor>(WeakContext.Get()->BlueprintEditor.Pin()))
						{
							TSet<FWidgetReference> Widgets = WidgetBlueprintEditor->GetSelectedWidgets();
							if (!Widgets.IsEmpty())
							{
								UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj();
								UWidgetTree* WidgetTree = IsValid(WidgetBlueprint) ? WidgetBlueprint->WidgetTree.Get() : nullptr;
								return AreWidgetsOnSameRoot(Widgets, WidgetTree);
							}
						}
						
						return false;
					}),
					FGetActionCheckState(),
					FIsActionButtonVisible()
				),
				LOCTEXT("UserWidgetWrapper", "User Widget Wrapper"),
				LOCTEXT("UserWidgetWrapperToolTip", "Run the User Widget Wrapper on the selected widget components."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), UserWidgetWrapper::SlateIconName)
			));
	}));
}

void FUserWidgetWrapperModule::HandleWidgetsWrapping(const TSet<FWidgetReference>& InWidgets)
{
	if (InWidgets.IsEmpty())
	{
		return;
	}
	
	UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), UWidgetBlueprintFactory::StaticClass());
	if (!ensureMsgf(NewFactory, TEXT("%hs: could not create factory for %s"), __FUNCTION__, *UWidgetBlueprintFactory::StaticClass()->GetName()))
	{
		return;
	}
	
	FGCObjectScopeGuard FactoryGCGuard(NewFactory);
	
	// Opens a dialog that give the user the chance to select a child class of the User Widget object
	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
	if (NewFactory->ConfigureProperties())
	{
		FEditorDelegates::OnNewAssetCreated.Broadcast(NewFactory);
		
		int32 OutChildIndex;
		UWidgetTree* OldWidgetTree = GetParentWidgetTree(InWidgets);
		ensure(OldWidgetTree);
		
		UPanelWidget* OldParentWidgetTemplate = UWidgetTree::FindWidgetParent(GetRootWidget(InWidgets, OldWidgetTree), OutChildIndex);
		
		FString DefaultAssetName;
		FString PackageNameToUse;
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(UserWidgetWrapper::AssetsToolModuleName);
		AssetToolsModule.Get().CreateUniqueAssetName(UserWidgetWrapper::GamePath / NewFactory->GetDefaultNewAssetName(), FString(), PackageNameToUse, DefaultAssetName);
		
		// Opens a dialogue that enable the user to choose the path where they want to create the new asset
		UUserWidgetBlueprint* CreatedAsset = Cast<UUserWidgetBlueprint>(CreateAsset(*DefaultAssetName, UserWidgetWrapper::GamePath, NewFactory->GetSupportedClass(), NewFactory));
		if (IsValid(CreatedAsset))
		{
			if (UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				// Tries to open the editor the newly created asset
				if (EditorSubsystem->OpenEditorForAsset(CreatedAsset))
				{
					if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(CreatedAsset))
					{
						// Removes the widgets from the current parent, adds them to the new one and marks the widgets as modified
						MoveWidgetsTo(InWidgets, OldWidgetTree, WidgetBlueprint->WidgetTree.Get());
					}

					// Searches for the editor we just opened and set the focus, after that triggers a re-compile of the blueprint
					if (FWidgetBlueprintEditor* WidgetBlueprintEditor = static_cast<FWidgetBlueprintEditor*>(EditorSubsystem->FindEditorForAsset(CreatedAsset, true)))
					{
						WidgetBlueprintEditor->Compile();
					}
				}
			}

			FAssetData AssetData(CreatedAsset, true);
			if (AssetData.IsValid())
			{
				// Creates a new template object that will be added to the widget tree that got its widgets removed
				TSharedPtr<FWidgetTemplateBlueprintClass> Template = MakeShareable(new FWidgetTemplateBlueprintClass(AssetData));
				if (UUserWidget* NewUserWidgetTemplate = Cast<UUserWidget>(Template->Create(OldWidgetTree)))
				{
					if (IsValid(OldParentWidgetTemplate))
					{
						OldParentWidgetTemplate->InsertChildAt(OutChildIndex, NewUserWidgetTemplate);
					}
					else
					{
						OldWidgetTree->RootWidget = NewUserWidgetTemplate;
					}
				}
				
			}
		}
	}
}

UObject* FUserWidgetWrapperModule::CreateAsset(const FName& InDefaultAssetName, const FName& InPackagePath, UClass* InAssetClass, UFactory* InFactory)
{
	if (!ensureMsgf(InAssetClass || InFactory, TEXT("%hs: Invalid Asset Class or Factory passed in"), __FUNCTION__))
	{
		return nullptr;
	}
	
	if (InAssetClass && InFactory && !ensure(InAssetClass->IsChildOf(InFactory->GetSupportedClass())))
	{
		return nullptr;
	}
	
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(UserWidgetWrapper::ContentBrowserModuleName);
	FContentBrowserItemPath AssetPathToUse = ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(InPackagePath, EContentBrowserPathType::Internal));
	const FString& InitialInternalPath = AssetPathToUse.HasInternalPath() ? AssetPathToUse.GetInternalPathString() : UserWidgetWrapper::GamePath;
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>(UserWidgetWrapper::AssetsToolModuleName);
	return AssetToolsModule.Get().CreateAssetWithDialog(InDefaultAssetName.ToString(), InitialInternalPath, InAssetClass, InFactory, NAME_None, false);
}

void FUserWidgetWrapperModule::MoveWidgetsTo(const TSet<FWidgetReference>& InWidgets, UWidgetTree* InOldWidgetTree, UWidgetTree* InNewWidgetTree)
{
	if (!ensureMsgf(IsValid(InOldWidgetTree) && IsValid(InNewWidgetTree), TEXT("%hs: Attempting to move widgets with invalid Widget Trees"), __FUNCTION__))
	{
		return;
	}
	
	auto ModifyObject = [] (UObject* InObject)
	{
		if (!IsValid(InObject))
		{
			return;
		}
					
		InObject->SetFlags(RF_Transactional);
		InObject->Modify();
	};
	
	UWidget* RootWidgetTemplate = GetRootWidget(InWidgets, InOldWidgetTree);
	if (!IsValid(RootWidgetTemplate))
	{
		return;
	}
	
	ModifyObject(RootWidgetTemplate);
	ModifyObject(InNewWidgetTree);
	ModifyObject(InOldWidgetTree);
	
	int32 OutIndex;
	if (UPanelWidget* ParentWidgetTemplate = InOldWidgetTree->FindWidgetParent(RootWidgetTemplate, OutIndex))
	{
		ModifyObject(ParentWidgetTemplate);
	}
	
	// Rename the root parent widget template and set the new widget tree as its outer
	RootWidgetTemplate->Rename(nullptr, InNewWidgetTree);
	
	// Rename also every descendant widget
	TArray<UWidget*> ChildWidgets;
	UWidgetTree::GetChildWidgets(RootWidgetTemplate, ChildWidgets);
	for (UWidget* WidgetTemplate : ChildWidgets)
	{
		if (!IsValid(WidgetTemplate))
		{
			continue;
		}
		
		ModifyObject(WidgetTemplate);
		WidgetTemplate->Rename(nullptr, InNewWidgetTree);
	}
	
	if (ensure(InNewWidgetTree->RootWidget == nullptr))
	{
		// Set it as the root widget since the new object should be empty
		InNewWidgetTree->RootWidget = RootWidgetTemplate;

		// Removing the root will remove all descendants too
		InOldWidgetTree->RemoveWidget(RootWidgetTemplate);
	}
	
}

UWidgetTree* FUserWidgetWrapperModule::GetParentWidgetTree(const TSet<FWidgetReference>& InWidgets)
{
	for (const FWidgetReference& InWidget : InWidgets)
	{
		if (UWidget* Template = InWidget.GetTemplate())
		{
			return Cast<UWidgetTree>(Template->GetOuter());
		}
	}
	
	return nullptr;
}

bool FUserWidgetWrapperModule::AreWidgetsOnSameRoot(const TSet<FWidgetReference>& InWidgets, const UWidgetTree* InWidgetTree)
{
	return GetRootWidget(InWidgets, InWidgetTree) != nullptr;
}

UWidget* FUserWidgetWrapperModule::GetRootWidget(const TSet<FWidgetReference>& InWidgets, const UWidgetTree* InWidgetTree)
{
	if (InWidgets.Num() <= 1)
	{
		return InWidgets.Array().IsValidIndex(0) ? InWidgets.Array()[0].GetTemplate() : nullptr;
	}
	
	for (const FWidgetReference& InWidget : InWidgets)
	{
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(InWidget.GetTemplate()))
		{
			bool bContainsAllWidgets = true;
			
			for (const FWidgetReference& OtherWidget : InWidgets)
			{
				if (InWidget.GetTemplate() == OtherWidget.GetTemplate())
				{
					continue;
				}
				
				const int32 ChildIndex = InWidgetTree->FindChildIndex(PanelWidget, OtherWidget.GetTemplate());
				if (ChildIndex == INDEX_NONE)
				{
					bContainsAllWidgets = false;
				}
			}
			
			if (bContainsAllWidgets)
			{
				return PanelWidget;
			}
		}
	}
	
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUserWidgetWrapperModule, UserWidgetWrapper)