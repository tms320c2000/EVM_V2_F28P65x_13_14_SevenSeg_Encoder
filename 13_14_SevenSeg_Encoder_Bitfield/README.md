# 엔코더(eQEP)로 7-세그먼트 00~99 카운터 — TMS320F28P65x 비트필드 버전

**요약**
- 개발보드 V2의 **(14) 로터리 엔코더(24P/R)** 를 칩의 **eQEP1** 로 읽어서 **(13) 7-세그먼트 2개** 에 **00~99** 로 표시합니다. 오른쪽으로 돌리면 +1, 왼쪽으로 돌리면 -1, 노브를 누르면 00으로 리셋되고 누르는 동안 점(DP)이 켜집니다.
- **DriverLib을 전혀 쓰지 않고** 레지스터 구조체(`EQep1Regs`, `GpioDataRegs`)를 직접 조작합니다. 값 계산(`seg_counter.c/.h`)은 [DriverLib 버전](../13_14_SevenSeg_Encoder_Driverlib/)과 같은 파일입니다.
- 배선, 동작 설명, PC 단위 시험, 문제 해결은 DriverLib 버전 README에 자세히 있습니다. **보드에서는 아직 확인하지 않았습니다**(빌드만 확인).

## 1. 하드웨어 배선 (세 버전 공통)

개발보드 **A Side 핀-헤더** 에 점퍼선 13개로 연결합니다.

| 개발보드 핀-헤더 | 신호 | GPIO | A-Side 핀 |
|---|---|---|---|
| (13) A1, B1, C1, D1 | 1번 자리(왼쪽) BCD, A = LSB | GPIO0, 1, 2, 3 | 65, 67, 69, 71번 |
| (13) DP1 | 1번 자리 점 | GPIO4 | 73번 |
| (13) A2, B2, C2, D2 | 2번 자리(오른쪽) BCD | GPIO5, 6, 7, 8 | 75, 77, 79, 81번 |
| (13) DP2 | 2번 자리 점 | GPIO9 | 83번 |
| (14) A | 엔코더 A상 (eQEP1 QEPA) | GPIO20 | 105번 |
| (14) B | 엔코더 B상 (eQEP1 QEPB) | GPIO21 | 107번 |
| (14) S/W | 노브 스위치 (누르면 High) | GPIO22 | 109번 |

![TMS320F28X EVM V2 — (13) 7-세그먼트와 (14) 엔코더를 A Side 핀-헤더에 연결한 결선](f28xevm_v2_encoder.png)

실선 10가닥이 (13) 7-세그먼트, 점선 3가닥이 (14) 엔코더입니다.

## 2. 비트필드 방식으로 한 일

- **GPIO:** `GPIO_SetupPinMux` / `GPIO_SetupPinOptions`로 GPIO0~9는 출력, GPIO20/21은 eQEP1(MUX 1) 입력, GPIO22는 일반 입력(풀업 없음)으로 설정합니다. 출력은 `GPASET` / `GPACLEAR`로 GPIO0~9만 바꿉니다.
- **eQEP1:** `QDECCTL`은 0(쿼드러처 카운트), `QEPCTL.FREE_SOFT = 2`(브레이크포인트 중에도 카운트), `QEPCTL.PCRM = 1`(최대 위치에서 리셋), `QPOSMAX = 0xFFFFFFFF`로 설정한 뒤 `QEPCTL.QPEN = 1`로 시작합니다. 클럭은 `InitSysCtrl()`이 이미 켭니다.
- 메인 루프가 10 ms마다 `EQep1Regs.QPOSCNT`와 `GPADAT` 22번 비트를 읽어 `seg_counter`에 넘기고, 결과 워드를 GPIO0~9에 내보냅니다.

## 3. 빌드

CCS에서 `CCS/13_14_SevenSeg_Encoder_Bitfield.projectspec`을 Import하세요. `driverlib.lib`이 없는 자기완결형 프로젝트입니다(`device/` 폴더에 TI 헤더와 초기화 코드 포함).

## 4. 검증 상태

- **빌드로 확인함(2026-10-14)**: CPU1_RAM, CPU1_FLASH 구성이 모두 오류 없이 컴파일·링크됩니다. 공통 파일 `seg_counter.c`는 DriverLib 버전의 PC 테스트를 통과한 것과 같은 파일입니다.
- **보드에서 확인 전**: 엔코더 회전 방향, 눈금당 카운트, 노브 스위치 리셋, DP 점등, 7-세그먼트 자리 대응.

## 5. 메모리 사용량

| | CPU1_RAM 구성 합계 | CPU1_FLASH 구성 — Flash | CPU1_FLASH 구성 — RAM |
|---|---|---|---|
| 비트필드 | 4,739 | 4,929 | 307 |

세 버전 비교는 [FreeRTOS 버전 README](../13_14_SevenSeg_Encoder_Freertos/README.md)에 있습니다.
