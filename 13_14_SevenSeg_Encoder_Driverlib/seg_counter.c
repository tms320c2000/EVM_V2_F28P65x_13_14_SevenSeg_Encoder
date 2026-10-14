// 파일이름	:	seg_counter.c
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x 모듈 (칩/레지스터에 의존하지 않는 순수 C 로직)
// 파일버전	:	1.00
// 갱신이력	:	2026-10-14, 버전 1.00
// 예제설명	:	seg_counter.h 참고

#include "seg_counter.h"

void SegCounter_init(SegCounter *c, uint32_t startPos)
{
	c->value = SEGCNT_MIN_VALUE;
	c->residue = 0;
	c->lastPos = startPos;
	c->pressed = 0U;
}

void SegCounter_update(SegCounter *c, uint32_t pos, uint16_t pressed)
{
	int32_t delta;
	int32_t steps;
	int32_t next;

	//
	// 지난번 위치와의 차이. 부호 없는 32비트 뺄셈 결과를 부호 있는 값으로 해석하면
	// 카운터가 0xFFFFFFFF -> 0 으로 넘어가는 경우에도 올바른 증감량이 나옵니다.
	//
	delta = (int32_t)(pos - c->lastPos);
	c->lastPos = pos;

	//
	// 카운트를 눈금 단위로 환산. 남는 카운트(residue)는 다음 호출 때로 넘겨서,
	// 한 번에 눈금 하나가 안 되게 천천히 돌려도 카운트가 사라지지 않게 합니다.
	//
	c->residue += delta;
	steps = c->residue / SEGCNT_COUNTS_PER_STEP;
	c->residue -= steps * SEGCNT_COUNTS_PER_STEP;

	next = (int32_t)c->value + steps;
	if(next < SEGCNT_MIN_VALUE)
	{
		next = SEGCNT_MIN_VALUE;
	}
	if(next > SEGCNT_MAX_VALUE)
	{
		next = SEGCNT_MAX_VALUE;
	}
	c->value = (int16_t)next;

	//
	// 노브 스위치: 안 눌림 -> 눌림으로 바뀌는 순간 한 번만 0으로 리셋
	//
	if((pressed != 0U) && (c->pressed == 0U))
	{
		c->value = SEGCNT_MIN_VALUE;
		c->residue = 0;
	}
	c->pressed = (pressed != 0U) ? 1U : 0U;
}

uint16_t SegCounter_encode(uint16_t value, uint16_t dotOn)
{
	uint16_t tens;
	uint16_t ones;
	uint16_t word;

	if(value > (uint16_t)SEGCNT_MAX_VALUE)
	{
		value = (uint16_t)SEGCNT_MAX_VALUE;
	}
	tens = (uint16_t)(value / 10U);
	ones = (uint16_t)(value % 10U);

	word = (uint16_t)(tens | (uint16_t)(ones << 5));
	if(dotOn != 0U)
	{
		word |= (uint16_t)((1U << 4) | (1U << 9));
	}
	return word;
}

uint16_t SegCounter_displayWord(const SegCounter *c)
{
	return SegCounter_encode((uint16_t)c->value, c->pressed);
}

// 파일 끝.
