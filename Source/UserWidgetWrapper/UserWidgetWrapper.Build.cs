// Copyright Gianluca Bortolanza. All Rights Reserved.

using UnrealBuildTool;

public class UserWidgetWrapper : ModuleRules
{
	public UserWidgetWrapper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
			}
			);
		
		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
			);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ContentBrowserAssetDataSource",
				"ContentBrowserData",
				"CoreUObject",
				"Engine",
				"Kismet",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UMG",
				"UMGEditor", 
				"UnrealEd"
			}
			);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
