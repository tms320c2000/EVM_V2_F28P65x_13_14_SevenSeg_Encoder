// seg_counter.c PC 단위 시험 (gcc). 빌드/실행은 같은 폴더의 README 참고:
//   gcc -Wall -Wextra -I../.. test_seg_counter.c ../../seg_counter.c -o test_seg_counter && ./test_seg_counter

#include <stdio.h>
#include <stdint.h>
#include "seg_counter.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; \
	printf("FAIL line %d: %s\n", __LINE__, #cond); } } while(0)

static void test_encode(void)
{
	// 표 20-1: A가 LSB, 2번 자리는 5비트 위
	CHECK(SegCounter_encode(0, 0) == 0x000);
	CHECK(SegCounter_encode(7, 0) == (7 << 5));
	CHECK(SegCounter_encode(10, 0) == 0x001);				// 십 자리 1
	CHECK(SegCounter_encode(42, 0) == (4 | (2 << 5)));
	CHECK(SegCounter_encode(99, 0) == (9 | (9 << 5)));
	CHECK(SegCounter_encode(99, 1) == (9 | (9 << 5) | 0x010 | 0x200));
	CHECK(SegCounter_encode(0, 1) == 0x210);				// DP1=비트4, DP2=비트9
	CHECK(SegCounter_encode(150, 0) == SegCounter_encode(99, 0));	// 범위 초과는 99로
	CHECK((SegCounter_encode(99, 1) & ~0x3FFu) == 0);		// 10비트 밖은 항상 0
}

static void test_basic_count(void)
{
	SegCounter c;
	SegCounter_init(&c, 0);
	CHECK(c.value == 0);

	SegCounter_update(&c, 4, 0);		// +4카운트 = 1눈금
	CHECK(c.value == 1);
	SegCounter_update(&c, 12, 0);		// +8카운트 = 2눈금
	CHECK(c.value == 3);
	SegCounter_update(&c, 8, 0);		// -4카운트 = -1눈금
	CHECK(c.value == 2);
}

static void test_residue_kept(void)
{
	SegCounter c;
	SegCounter_init(&c, 0);
	// 1카운트씩 천천히 돌려도 4번째에 한 눈금
	SegCounter_update(&c, 1, 0); CHECK(c.value == 0);
	SegCounter_update(&c, 2, 0); CHECK(c.value == 0);
	SegCounter_update(&c, 3, 0); CHECK(c.value == 0);
	SegCounter_update(&c, 4, 0); CHECK(c.value == 1);
	// 반대로 돌릴 때 남은 카운트가 부호 따라 정확히 상쇄되는지
	SegCounter_update(&c, 3, 0); CHECK(c.value == 1);
	SegCounter_update(&c, 0, 0); CHECK(c.value == 0);
}

static void test_clamp(void)
{
	SegCounter c;
	SegCounter_init(&c, 0);
	SegCounter_update(&c, (uint32_t)(-400), 0);		// 0에서 아래로 100눈금
	CHECK(c.value == 0);

	SegCounter_init(&c, 0);
	SegCounter_update(&c, 4000, 0);					// 위로 1000눈금
	CHECK(c.value == 99);
	SegCounter_update(&c, 4004, 0);					// 99에서 더 올려도 99
	CHECK(c.value == 99);
	SegCounter_update(&c, 4000, 0);					// 한 눈금 내리면 98
	CHECK(c.value == 98);
}

static void test_wraparound(void)
{
	SegCounter c;
	// 위치 카운터가 0 아래로 내려가 0xFFFFFFFC 로 넘어가는 경우
	SegCounter_init(&c, 8);
	SegCounter_update(&c, 8, 0);
	SegCounter_update(&c, 12, 0);
	CHECK(c.value == 1);
	SegCounter_init(&c, 0);
	SegCounter_update(&c, 4, 0);
	SegCounter_update(&c, 0, 0);
	SegCounter_update(&c, 0xFFFFFFFCu, 0);			// 0 -> 0xFFFFFFFC = -4카운트
	CHECK(c.value == 0);								// 0 이하로는 안 내려감

	SegCounter_init(&c, 0xFFFFFFFCu);
	SegCounter_update(&c, 0, 0);						// 0xFFFFFFFC -> 0 = +4카운트 (넘어감)
	CHECK(c.value == 1);
	SegCounter_update(&c, 4, 0);
	CHECK(c.value == 2);
}

static void test_switch(void)
{
	SegCounter c;
	SegCounter_init(&c, 0);
	SegCounter_update(&c, 40, 0);
	CHECK(c.value == 10);
	CHECK(SegCounter_displayWord(&c) == SegCounter_encode(10, 0));

	SegCounter_update(&c, 40, 1);					// 누른 순간 리셋
	CHECK(c.value == 0);
	CHECK(SegCounter_displayWord(&c) == SegCounter_encode(0, 1));	// 눌려 있는 동안 DP 켜짐

	SegCounter_update(&c, 44, 1);					// 계속 누른 채 돌려도 에지가 아니므로 리셋 안 됨
	CHECK(c.value == 1);
	SegCounter_update(&c, 44, 0);					// 뗌: DP 꺼짐
	CHECK(SegCounter_displayWord(&c) == SegCounter_encode(1, 0));
	SegCounter_update(&c, 44, 1);					// 다시 누르면 다시 리셋
	CHECK(c.value == 0);
}

int main(void)
{
	test_encode();
	test_basic_count();
	test_residue_kept();
	test_clamp();
	test_wraparound();
	test_switch();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
