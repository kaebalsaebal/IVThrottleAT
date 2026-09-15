# ThrottleAT 0.3.0 — GTA IV 1.2.0.59 Windows 실험 버전

0.3.0은 업로드된 **LibertyCityPlates의 `PatchEngine 1` 변속 경로**를 지원합니다. 0.2.3 주행 로그에서 원본 게임 함수가 우회되는 것을 확인했고, 해당 LCP 바이너리의 전진 변속 결정 부분에도 연결했습니다. 저회전 출발 클러치를 변속 중으로 오인하는 판정을 수정하고 승용차 저스로틀 변속 기준을 낮췄습니다. 빌드와 자동 검사는 통과했지만, 새 버전의 실제 주행과 대시보드 2,000~3,000 RPM 변속은 아직 확인하지 않았습니다. [원인과 수정 근거](docs/LCP-COMPATIBILITY.md)를 참고하세요.

대상: Windows / GTA IV 1.2.0.59 / FusionFix 5.0.1 / ASI Loader 9.7.1. ScriptHook, ScriptHookDotNet, .NET 런타임이 필요하지 않습니다. 자동차만 제어하며 오토바이·항공기·보트는 기존 게임 동작을 유지합니다.

## 사용

1. 아래 MSVC 빌드 절차로 ASI를 만듭니다. 게임을 종료한 상태에서 `build-win32/Release/ThrottleAT.asi`, 저장소의 `ThrottleAT.ini`, `ThrottleAT-ThirdParty.txt`를 `GTAIV/plugins`에 넣습니다. **이번에는 ASI와 INI를 모두 교체**합니다. 필요하면 기존 INI를 먼저 백업하세요. 기존 FusionFix, 로더, LibertyCityPlates와 그 설정은 유지합니다.
2. 기본 설정 `Enabled=0`은 **관찰 모드**입니다. 로딩 후 자동차를 운전하면 정보만 기록하고 기본 변속을 사용합니다.
3. 자동차를 앞으로 운전하면서 키보드 **F8**을 한 번 누르면 실험용 자동변속이 켜집니다. 다시 누르면 기본 변속으로 돌아갑니다. 가속은 게임패드 아날로그 트리거를 그대로 사용합니다.
4. F8로 켜면 `ThrottleAT: ON (waiting for vehicle)`가 표시되고, 유효한 플레이어 차량의 변속 제어에 실제 진입하면 `ThrottleAT: ACTIVE`가 표시됩니다. 다시 누르면 `ThrottleAT: OFF (stock shifts)`가 표시됩니다. 메시지는 약 3초 유지됩니다. 오류로 제어가 중단되면 `ThrottleAT: ERROR (stock shifts)`가 표시됩니다. 알림 함수 검증에 실패하면 로그로 확인합니다. 짧게 눌렀다 놓으세요. 이 키는 전진 변속 함수가 호출될 때만 감지됩니다.
5. 다음 실행부터 자동으로 켜려면 게임을 닫고 INI의 `Enabled=1`로 바꿉니다. F8 토글은 저장되지 않습니다.

처음에는 평지에서 트리거를 약 20%, 50%, 100%로 나누어 밟고, 중속에서 깊게 눌러 킥다운을 확인하세요. 관찰 모드와 제어 모드를 비교할 수 있습니다. 시작 버전/훅 결과는 `ThrottleAT.log`, 주행 정보는 `ThrottleAT.csv`에 저장됩니다. 두 파일은 실행마다 덮어씁니다. CSV는 최대 60,000행으로 제한합니다.

테스트 기록에는 실제 기어, 요청 기어, 스로틀, 브레이크, 속도, 변속용 정규화 RPM, 적용 여부가 들어갑니다. `mode=1`은 관찰, `2`는 제어, `3`은 오류로 기본 변속에 복귀한 상태입니다. CSV `applied=1`은 해당 호출의 제어를 맡았다는 뜻이며 항상 기어를 바꿨다는 뜻은 아닙니다. 요청과 실제 기어 차이 및 reason을 함께 보세요.

실제 동작을 판단하려면 게임을 실행하고 주행한 로그가 필요합니다. 현재 검사 통과는 게임 호환성·주행 안정성을 보증하지 않습니다. 원상복구는 게임 종료 후 이 프로젝트의 ASI/INI/로그 파일을 제거하면 됩니다. EXE와 handling.dat는 수정하지 않습니다.

## 구현된 변속

공통 컨트롤러가 낮은 스로틀에서는 일찍, 중간 스로틀에서는 중간 회전 영역에서, 높은 스로틀에서는 높은 회전 영역에서 변속합니다. 저장소 INI의 승용차 값은 스로틀 0%/50%/100%에 대해 0.28/0.56/0.94이며 중간 입력은 선형 보간합니다. 킥다운, 예상 다운시프트 회전수 제한, 변속 조건 유지 시간, 쿨다운, 기어 변경 응답 대기를 포함합니다.

정규화 RPM은 실측 엔진 rpm 단위가 아닙니다. 1단 출발 중이거나 클러치가 재결합하면 게임 기본 변속이 사용하는 `휠 속도 × 현재 기어비 / 구동 속도 기준값`을 사용하고, 2단 이상에서 재결합 중에는 게임의 평활화된 회전값을 관찰하며 기어를 유지합니다. 컴파일된 코어 기본값과 다른 이전 INI를 쓰면 저장소의 조기 변속 설정이 적용되지 않습니다. 엔진 토크나 토크컨버터 물리를 새로 구현한 모드는 아닙니다.

클래스 프리셋 -> `[Class:...]` 설정 -> `[Model:...]` 설정 순으로 적용합니다. 차량명은 게임의 vehicles.ide에서 읽고 모델 해시와 연결합니다. 트럭/버스와 스포츠카의 대표 모델 목록을 분류하며, 나머지는 승용차 프리셋입니다. 이는 실제 차량 제원을 추정하는 분류기가 아닙니다. 코어에 있는 Motorcycle 프리셋은 현 자동차 전용 어댑터에서는 사용하지 않습니다. 모델 이름은 대소문자를 구분하지 않고 나머지 키는 정확히 입력해야 합니다.

플레이어 운전 여부, 차량 세대/슬롯, 바퀴 접지, 과도한 휠 슬립, 입력값, 기어비를 검사합니다. 맞지 않으면 그 호출에서 기본 변속을 실행합니다. 후진/정차/승하차/일시정지로 호출이 끊기면 한 번 기본 변속에 맡기고 다시 동기화합니다. 변속 응답 지연이나 메모리 접근 오류는 기본 변속으로 복귀하며 F8로 재시도할 수 있습니다. 실행 스레드가 바뀌는 오류는 게임을 재시작해야 합니다.

## Windows MSVC 빌드

Visual Studio 2022 Build Tools의 C++ 데스크톱 개발 도구, Windows SDK와 CMake가 필요합니다. 이 프로젝트 폴더에서 실행합니다.

```powershell
cmake -S . -B build-win32 -G "Visual Studio 17 2022" -A Win32
cmake --build build-win32 --config Release
ctest --test-dir build-win32 -C Release --output-on-failure
```

결과는 `build-win32/Release/ThrottleAT.asi`입니다. **Win32가 필수**입니다. 64비트 OS에서도 x86 게임에 맞춰야 합니다. MSVC 런타임과 MinHook는 정적으로 연결하며, MinHook 소스와 라이선스가 포함되어 있습니다. 최종 직접 DLL 의존성은 Windows의 KERNEL32/USER32/VERSION입니다.

## 다른 빌드 경로

이번 실제 CE059 연결은 MSVC x86 인라인 어셈블리/SEH를 사용합니다. MinGW/Proton 연결은 더 이상 작업 대상이 아닙니다. 기존 교차 빌드 파일은 남겨 두었지만 MinGW로 생성한 ASI에는 이 제어 연결이 없고 빌드 때 경고합니다.

```sh
# Linux에서 게임과 무관한 코어 검사
cmake -S . -B build-host -DAT_BUILD_ASI=OFF
cmake --build build-host
ctest --test-dir build-host --output-on-failure
# Windows용 비활성 셸만 교차 빌드: cmake, ninja-build, g++-mingw-w64-i686 필요
cmake -S . -B build-mingw -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw32.cmake -DBUILD_TESTING=OFF
cmake --build build-mingw
```

## 파일과 검증 근거

- `src/controller.cpp`, `config.cpp`, `runtime.cpp`: 게임과 독립적인 변속 코어와 추상 백엔드 구조.
- `src/ce059.cpp`: 실행 파일 버전·명령어 검사, 실제 Windows 어댑터, 관찰/제어, 로그. 이 어댑터는 동기식 훅에서 직접 컨트롤러를 호출하므로 추상 Runtime의 프레임별 임대 방식은 사용하지 않습니다.
- `src/x86_gate.cpp`, `src/x86_lcp.cpp`: 원본 게임/LCP 중간 훅의 CPU 상태 보존과 기본/커스텀 분기.
- `src/x86_trace.cpp`: 이전 진단용 입구 게이트. 0.3.0에서는 설치하지 않습니다.
- `src/plugin.cpp`: ASI Loader가 호출하는 InitializeASI 및 상태/종료 API.
- [정확한 주소·구조 검증 근거](docs/CE059-EVIDENCE.md), [검사 결과](docs/VALIDATION.md), [외부 라이브러리](docs/THIRD-PARTY.md).

다른 게임 버전에서 오프셋을 그대로 사용하면 안 됩니다. 지원 범위 확장에는 실행 파일별 재검증과 실주행 테스트가 필요합니다.