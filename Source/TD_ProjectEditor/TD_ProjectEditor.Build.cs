using UnrealBuildTool;

/**
 * 에디터에서만 도는 코드가 들어가는 모듈.
 *
 * 구글 시트 파서가 여기 있어야 하는 이유는 두 가지다.
 * 하나는 UnrealEd 와 DataTable 편집 API 를 쓰기 때문이고,
 * 다른 하나는 이 코드가 패키징된 게임에 들어가면 안 되기 때문이다 —
 * 배포된 게임은 시트를 부르지 않는다.
 */
public class TD_ProjectEditor : ModuleRules
{
	public TD_ProjectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory + "/Public");

		// TD_Project 는 행 구조체(FTDItemStatRow 등)를 쓰기 위해,
		// GoogleSheetLoader 는 UGoogleSheetParserBase 를 상속하기 위해 필요하다.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"TD_Project",
			"GoogleSheetLoader"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"UMG", "UMGEditor", "CommonUI", "Slate", "SlateCore", "Kismet", "BlueprintGraph", "RenderCore", "AssetRegistry", "GameplayAbilities"
		});
	}
}
