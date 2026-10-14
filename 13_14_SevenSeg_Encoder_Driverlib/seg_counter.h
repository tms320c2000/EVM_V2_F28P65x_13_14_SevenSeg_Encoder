// 파일이름	:	seg_counter.h
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x 모듈 (칩/레지스터에 의존하지 않는 순수 C 로직)
// 파일버전	:	1.00
// 갱신이력	:	2026-10-14, 버전 1.00
// 예제설명	:

//************************************************************************************************************************************************************************
//
// 엔코더 위치 카운트(eQEP의 QPOSCNT 값)와 노브 스위치 상태를 받아서
//   - 00~99 범위로 제한된 표시 값
//   - 7-세그먼트 2개(회로블록 13)에 내보낼 10비트 출력 워드
// 를 만들어 주는 작은 모듈입니다. 레지스터를 전혀 건드리지 않으므로 Bitfield / DriverLib /
// FreeRTOS 세 버전이 이 파일(seg_counter.c/.h)을 똑같이 공유하고, PC에서 gcc로 단위 시험도
// 할 수 있습니다(test/host).
//
// 출력 워드의 비트 배치 (개발보드 (13) 7-세그먼트 핀-헤더 순서와 같습니다):
//
//		비트0~3 : 1번 자리(왼쪽, 십의 자리) A1,B1,C1,D1 (BCD, A가 LSB)
//		비트4   : 1번 자리 DP1 (High = 점 켜짐)
//		비트5~8 : 2번 자리(오른쪽, 일의 자리) A2,B2,C2,D2 (BCD, A가 LSB)
//		비트9   : 2번 자리 DP2 (High = 점 켜짐)
//
// 이 10비트를 GPIO0~GPIO9에 그대로 내보내면 됩니다(비트 n = GPIOn).
//
//************************************************************************************************************************************************************************

#ifndef SEG_COUNTER_H
#define SEG_COUNTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEGCNT_MIN_VALUE		0		// 표시 최솟값
#define SEGCNT_MAX_VALUE		99		// 표시 최댓값

// 눈금(detent) 하나를 돌릴 때 eQEP 위치 카운터가 늘어나는 양. eQEP는 쿼드러처 모드에서
// A/B의 네 에지를 모두 세므로(x4), 1펄스 = 4카운트입니다. 이 보드의 엔코더는 24P/R이고
// 노브 눈금이 펄스 하나에 한 칸씩이라면 눈금 하나 = 4카운트입니다. 노브를 한 칸 돌렸는데
// 값이 2씩 바뀌거나 반 칸만 바뀌면 이 값을 2나 8 등으로 바꿔 보세요.
#define SEGCNT_COUNTS_PER_STEP	4

typedef struct
{
	int16_t		value;			// 현재 표시 값 (SEGCNT_MIN_VALUE ~ SEGCNT_MAX_VALUE)
	int32_t		residue;		// 아직 한 눈금이 못 된 카운트 (-3 ~ +3)
	uint32_t	lastPos;		// 지난번에 읽은 위치 카운터 값
	uint16_t	pressed;		// 노브 스위치 현재 상태 (1 = 눌림)
} SegCounter;

//
// SegCounter_init - 값을 0으로 만들고, 현재 위치 카운터 값을 기준점으로 잡습니다.
//
void SegCounter_init(SegCounter *c, uint32_t startPos);

//
// SegCounter_update - 위치 카운터와 스위치 상태를 읽어 값을 갱신합니다. 주기적으로(예: 10ms)
// 호출하세요. 위치는 32비트 카운터가 한 바퀴 돌아도(0xFFFFFFFF <-> 0) 차이를 정확히 계산합니다.
// 스위치가 안 눌림 -> 눌림으로 바뀌는 순간(에지)에 값을 0으로 되돌립니다.
//
void SegCounter_update(SegCounter *c, uint32_t pos, uint16_t pressed);

//
// SegCounter_encode - 0~99 값과 점(DP) 켜짐 여부를 위 비트 배치의 10비트 워드로 바꿉니다.
// 범위를 벗어난 값은 99로 잘라서 처리합니다.
//
uint16_t SegCounter_encode(uint16_t value, uint16_t dotOn);

//
// SegCounter_displayWord - c의 현재 값과 스위치 상태(눌려 있는 동안 DP 켜짐)로 출력 워드를 만듭니다.
//
uint16_t SegCounter_displayWord(const SegCounter *c);

#ifdef __cplusplus
}
#endif

#endif	// SEG_COUNTER_H

// 파일 끝.
