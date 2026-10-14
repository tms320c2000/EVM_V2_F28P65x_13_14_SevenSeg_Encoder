# TMS320F28X 개발보드 V2 — F28P65x 엔코더(eQEP)로 7-세그먼트 00~99 카운터 예제

[TMS320F28X 범용 개발보드 V2](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127)의
회로블록 **(14) 로터리 엔코더(24P/R)** 를 F28P65x 모듈의 **eQEP1** 로 읽어서 **(13) 7-세그먼트 2개** 에 **00~99** 로
표시하는 예제입니다. 엔코더를 오른쪽으로 돌리면 +1, 왼쪽으로 돌리면 -1이고, 00과 99에서 멈춥니다. 노브를 누르면 00으로
리셋되고 누르는 동안 점(DP)이 켜집니다. 같은 동작을 세 가지 방식으로 각각 구현해서 코드량·메모리 사용량을 비교할 수 있게
만들었습니다.

## 포함된 프로젝트

| 프로젝트 | 설명 |
|---|---|
| [13_14_SevenSeg_Encoder_Bitfield](13_14_SevenSeg_Encoder_Bitfield/) | 레지스터 비트필드 직접 제어(DriverLib 미사용) |
| [13_14_SevenSeg_Encoder_Driverlib](13_14_SevenSeg_Encoder_Driverlib/) | TI DriverLib 사용. 배선·동작 설명이 가장 자세하고, PC에서 돌리는 호스트 테스트(`test/host`) 포함 |
| [13_14_SevenSeg_Encoder_Freertos](13_14_SevenSeg_Encoder_Freertos/) | FreeRTOS 태스크 2개(엔코더 입력, 표시), 정적 할당(힙 미사용), 세 버전 메모리 비교표 |

각 폴더는 자기완결형(self-contained) CCS 프로젝트입니다 — 폴더 하나만 받아도 필요한 파일이 전부
로컬에 포함되어 있어 Import → Build가 됩니다. 값 계산(`seg_counter.c/.h`)은 레지스터를 건드리지 않는 순수 C이고
세 버전에서 같은 파일입니다. GPIO와 eQEP을 다루는 부분만 버전별로 다릅니다.

## 배선

7-세그먼트(13)와 엔코더(14) 신호를 모두 **A-Side** 핀-헤더에 점퍼선 13개로 연결합니다. (13)의 CD4543B 디코더가 자리마다
값을 유지하므로 다이내믹 스캔은 필요 없습니다. 헤더는 `A1 B1 C1 D1 DP1 A2 B2 C2 D2 DP2` 순서이고 GPIO0~9도 같은 순서라
점퍼선이 나란히 들어갑니다.

| 개발보드 핀-헤더 | 신호 | GPIO | A-Side 핀 |
|---|---|---|---|
| (13) A1, B1, C1, D1 | 1번 자리(왼쪽) BCD, A = LSB | GPIO0, 1, 2, 3 | 65, 67, 69, 71번 |
| (13) DP1 | 1번 자리 점 | GPIO4 | 73번 |
| (13) A2, B2, C2, D2 | 2번 자리(오른쪽) BCD | GPIO5, 6, 7, 8 | 75, 77, 79, 81번 |
| (13) DP2 | 2번 자리 점 | GPIO9 | 83번 |
| (14) A | 엔코더 A상 (eQEP1 QEPA) | GPIO20 | 105번 |
| (14) B | 엔코더 B상 (eQEP1 QEPB) | GPIO21 | 107번 |
| (14) S/W | 노브 스위치 (누르면 High) | GPIO22 | 109번 |

![TMS320F28X EVM V2 — (13) 7-세그먼트와 (14) 엔코더를 A Side 핀-헤더에 연결한 결선](13_14_SevenSeg_Encoder_Bitfield/f28xevm_v2_encoder.png)

실선 10가닥이 (13) 7-세그먼트, 점선 3가닥이 (14) 엔코더입니다. 엔코더 A/B는 보드에서 슈미트 트리거 버퍼를 거쳐 나오고
노브 스위치는 10k 풀다운 회로라서, 세 입력 모두 칩 내부 풀업을 켜지 않습니다. GPIO20/21은 TI의 eQEP 예제와 같은 핀입니다.

## 동작과 조정

- 엔코더 눈금 하나(펄스 1개)를 돌릴 때마다 표시 값이 1씩 바뀝니다. eQEP는 쿼드러처 모드에서 네 에지를 모두 세므로 눈금
  하나 = 위치 카운터 4이고, `seg_counter.h`의 `SEGCNT_COUNTS_PER_STEP`(기본 4)이 이 값입니다.
- 오른쪽으로 돌렸는데 값이 줄면 A/B를 바꿔 연결하거나 eQEP 설정의 `EQEP_CONFIG_NO_SWAP`을 `EQEP_CONFIG_SWAP`으로 바꾸세요.
- 10 ms 주기로 위치와 스위치를 읽습니다. 주기 안에 눈금 여러 개를 돌려도 놓치지 않습니다.
- CCS Watch 창에 `displayValue`, `encoderPos`, `knobPressed`, `segOutput`을 등록하면 실시간으로 볼 수 있습니다.

## 메모리 사용량 비교

링커 맵의 영역별 사용량 합계(워드, 16비트)입니다. `--opt_level=off`, CGT 25.11.1.LTS로 측정했습니다.

| | Bitfield | DriverLib | FreeRTOS |
|---|---|---|---|
| CPU1_RAM 구성 합계 | 4,739 | 5,721 | 9,585 |
| CPU1_FLASH 구성 — Flash | 4,929 | 5,166 | 8,001 |
| CPU1_FLASH 구성 — RAM | 307 | 1,267 | 2,296 |
| `FLASH_BANK0`(131,070워드) 대비 Flash | 약 3.8% | 약 3.9% | 약 6.1% |

RAM 구성은 코드까지 RAM에 올라가므로 크고, 스택과 힙 설정이 포함됩니다.

## 검증 상태

- **빌드·PC 테스트로 확인함(2026-10-14)**: 세 버전 모두 CPU1_RAM, CPU1_FLASH 구성이 오류 없이 컴파일·링크됩니다. 공통 파일 `seg_counter.c`는 PC 호스트 테스트(34개 항목)를 통과합니다.
- **보드에서 확인 전**: 엔코더 회전 방향(+/-)과 눈금당 카운트(4), 노브 스위치 리셋, DP 점등, 7-세그먼트 자리 대응(1번 자리 = 왼쪽), FreeRTOS 두 태스크의 알림 동작(`displayUpdateCount`).

## 관련 링크
- 상품 페이지: https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127

## 프로세서 모듈

- [TMS320F28P650DK9 모듈(산업용)](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903200)
- [TMS320F28P659DK8-Q1 모듈(차량 전장용)](https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903201)

## 개발 환경

CCS 21.x(Theia 기반) / C2000Ware 26.00.00.00

## Import 방법 (zip 직접 import도 지원)

압축을 미리 풀어서 "Select search-directory"로 지정하거나, GitHub에서 받은 zip 파일을
그대로 CCS의 "Select archive file"로 지정해도 됩니다.
