#include "Modules/ModuleManager.h"

/**
 * 에디터 전용 모듈의 진입점.
 *
 * 지금은 초기화할 것이 없어 기본 구현을 그대로 쓴다.
 * 나중에 커스텀 에셋 에디터나 디테일 패널 커스터마이징이 생기면
 * FDefaultModuleImpl 대신 직접 만든 모듈 클래스로 바꾸면 된다.
 */
IMPLEMENT_GAME_MODULE(FDefaultModuleImpl, TD_ProjectEditor);
