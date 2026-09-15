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
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "GameplayAbilities", "GameplayTasks", "AIModule", "NavigationSystem", "NetCore", "ModelViewViewModel", "FieldNotification" });

		// Paper2D 는 UPaperFlipbookComponent, PaperZD 는 애니메이션 상태 머신.
		// 헤더에서는 전방 선언만 쓰므로 Private 으로 충분하다.
		PrivateDependencyModuleNames.AddRange(new string[]
		{"UMG",
			"HTTP", "Json",
			"CommonUI",
			"Slate",
			"SlateCore",
			"MoviePlayer",
			"Paper2D",
			"PaperZD",

			// FTDSkillRow.VFX 가 TSoftObjectPtr<UNiagaraSystem> 을 갖는다.
			// 헤더는 전방 선언뿐이지만 리플렉션 생성 코드가 UClass 심볼을 요구한다.
			"Niagara",

			// 옵션의 볼륨 조절. Control Bus 에 값을 밀어넣는 데만 쓴다.
			"AudioModulation",

			// UTDAudioSettings 가 UDeveloperSettings 를 상속한다.
			// 헤더가 보이는 것과 링크되는 것은 별개다(§11-A).
			"DeveloperSettings",

			// 보스방 재입장 기록이 계정 ID(FUniqueNetIdRepl::ToString)로 사람을 알아본다.
			// FUniqueNetIdRepl 헤더는 Engine 을 타고 보이지만 ToString 은 이 모듈에서 export 된다.
			"CoreOnline"
		});

	}
}
