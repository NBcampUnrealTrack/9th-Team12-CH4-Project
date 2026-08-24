// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TD_Project : ModuleRules
{
	public TD_Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		// AIModule 은 IGenericTeamAgentInterface, NetCore 는 FFastArraySerializer,
		// GameplayTasks 는 AbilitySystemComponent 가 요구한다.
		// 전부 헤더에서 상속하거나 멤버로 쓰므로 Private 이 아니라 Public 이어야 한다.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "GameplayAbilities", "GameplayTasks", "AIModule", "NetCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

	}
}
