# LibertyCityPlates 호환성 수정 — ThrottleAT 0.3.0

## 이번 로그에서 확인한 원인

0.2.3의 주행 로그에서 GTAIV.exe의 자동차 변속기 갱신 호출이 LibertyCityPlates.asi로 향했습니다. 플레이어는 원본 변속 함수를 거치지 않았으므로 기존 ThrottleAT가 ON이어도 변속을 제어할 수 없었습니다.

| 로그 근거 | 값 |
| --- | --- |
| GTAIV.exe 로드 주소 | 0x730000 |
| LibertyCityPlates.asi 로드 주소 | 0x717E0000 |
| 게임 RVA 0x835672 호출 대상 | 0x717EE6D0 = LibertyCityPlates + 0xE6D0 |
| 원본 경로 플레이어 호출 | updatePlayer=0, shiftPlayer=0, midPlayer=0 |
| 업로드된 LibertyCityPlates.txt | PatchEngine 1 |

공개 [transmission.cpp](https://github.com/ImpossibleEchoes/LibertyCityPlates/blob/main/transmission.cpp)의 CTransmission::patch도 이 설정에서 게임의 process 호출을 교체합니다. 정확한 오프셋과 호출 규약은 공개 소스에서 추측하지 않고 업로드된 ASI의 명령어를 별도로 확인했습니다. 공개 main 소스와 사용자 바이너리 전체가 일치한다고 주장하지 않습니다.

## 수정 내용

- LCP가 사용 중이면 검증한 자체 processGears의 전진 변속 결정 부분에 연결합니다. LCP의 엔진/RPM 처리, 시동/정지 및 클러치 갱신은 계속 실행됩니다.
- 1단 출발 클러치가 미끄러지는 상태를 진행 중인 기어 변경으로 오인하지 않습니다. 이 상태에서도 휠 속도와 기어비에 따른 조기 1→2 변속을 판단합니다.
- 기어 변경 응답과 클러치 재결합 완료를 별개로 처리합니다. 기어가 정상 변경됐는데 클러치가 늦게 붙는 상황에서 잘못된 응답 시간 초과가 발생하지 않습니다.
- 동봉 INI의 승용차 Low/Mid/High는 0.28/0.56/0.94, Down은 0.12입니다. 이는 정규화된 변속 신호입니다. LVS 대시보드에 표시되는 실제 숫자와 직접 대응하는 RPM 설정이 아닙니다.
- ON은 선택 상태이고 ACTIVE는 실제 제어 진입입니다. 로그의 PLAYER CONTROL APPLIED와 SHIFT 행으로 실제 제어 및 기어 변경을 구분합니다.

## 검증한 LCP 연결 위치

아래는 모두 LCP 모듈 기준 RVA이며 해당 업로드 바이너리의 레이아웃에만 적용합니다. 다른 빌드는 명령어 재검증이 필요합니다. 전체 파일 해시는 실행 허용 목록으로 사용하지 않습니다.

| RVA | 확인 내용 |
| --- | --- |
| 0xE6D0 | 게임이 호출하는 LCP process 입구 |
| 0xE849 | processGears(0xEA00) 호출 |
| 0xEA00 | processGears 입구 |
| 0xEBCB | divss xmm2,[edx+4C]; 전진 변속 중간 훅 |
| 0xEC61 | 기어 및 시각 쓰기, 클러치 0.1 쓰기 |
| 0xEC70 | pop edi / pop esi / mov esp,ebp / pop ebp / ret 18 복귀 |

중간 훅에서 EDI는 변속기, EDX는 handling, EBP는 함수 프레임입니다. [EBP+14]/[EBP+18]은 휠/차량 속도입니다. [EBP+08]의 원래 차량 인자는 이미 스로틀 값으로 덮어써져 있으므로 읽지 않습니다. 변속기에서 차량 주소를 도출하고 플레이어 운전 및 풀 소속을 교차 확인합니다. 훅은 GPR/ESP/EFLAGS/x87/XMM/MXCSR를 보존하며, 정상적인 인접 기어 변경에서만 원래 LCP 변속과 같은 네 필드를 씁니다.

분석 입력 SHA-256: 8ec869240f30fdbd1f4ccc3f38e47a5e9f79e0ff05933afa1c43e42b804516fd. PE32, 이미지 크기 0x6E000, 타임스탬프 0x6963B922. 이 값은 분석 기록이며 해시 검사 조건이 아닙니다. 실행 시에는 호출 대상과 관련 명령어 다섯 구간을 확인합니다.

## 설치와 확인

README의 MSVC 빌드 후 게임을 종료하고 생성한 ThrottleAT.asi, 저장소의 ThrottleAT.ini와 ThrottleAT-ThirdParty.txt를 대상 PC의 GTAIV/plugins에 복사합니다. ThrottleAT.asi와 ThrottleAT.ini를 함께 교체하세요. LibertyCityPlates.asi와 LibertyCityPlates.txt는 수정하지 않습니다. 기본값은 관찰 모드이므로 자동차를 앞으로 운전하며 F8을 짧게 누릅니다.

정상적인 시작 로그에는 LCP COMPATIBILITY ACTIVE, 실제 제어 시 PLAYER CONTROL APPLIED, 기어 변경 시 SHIFT가 기록됩니다. ACTIVE는 제어 진입을 뜻하며, 특정 RPM에서 변속했다는 보증은 아닙니다. 계속 문제가 있으면 게임 종료 후 해당 실행의 ThrottleAT.log와 ThrottleAT.csv가 다음 판단 근거입니다.

LCP는 ThrottleAT 초기화 전에 로드되어 패치되어 있어야 합니다. 업로드된 설치에서는 이 순서이며 공개 DllMain도 즉시 패치합니다. 나중에 추가 로드되거나 다른 모드가 호출을 다시 바꾸는 경우를 자동 추적하는 기능은 없습니다. F8은 여전히 전진 변속 콜백에서 감지하므로 도보·정지 등 모든 상황의 입력을 보장하지 않습니다.

검증 결과: MSVC Win32 Release 빌드 및 CTest 6개 통과. 실제 업로드 ASI를 실행하지 않고 읽어 명령어 다섯 구간과 processGears 호출 대상을 대조했습니다. 새 연결의 CPU 보존, 관찰 시 무변경, 플레이어만 제어, 출발 클러치 미끄러짐 중 조기 변속, 잘못된 프레임 시 원래 경로 복귀를 검사했습니다. 수정판의 실제 주행과 LVS 2,000~3,000 RPM 표시는 아직 검증하지 않았습니다.
