// 파일이름	:	13_14_SevenSeg_Encoder_Bitfield.c
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x(F28P650DK9 산업용 / F28P659DK8-Q1 차량용) 모듈
// 파일버전	:	1.00
// 갱신이력	:	2026-10-14, 버전 1.00
// 예제설명	:

//************************************************************************************************************************************************************************
//
// 본 예제는 TMS320F28P65x 모듈이 탑재된 TMS320F28X 개발보드(EVM) V2를 대상으로 하고 있으며,
// 개발보드의 '(14) 엔코더 & TACT 스위치' 회로의 로터리 엔코더(24P/R)를 TMS320F28P65x 칩의
// eQEP1 회로로 읽고, 그 값을 '(13) 7-세그먼트 2개' 회로에 00~99로 표시합니다.
//
//		엔코더를 오른쪽으로 돌리면 +1, 왼쪽으로 돌리면 -1 (00과 99에서 멈춤, 넘치지 않음)
//		엔코더 노브를 누르면 00으로 리셋, 누르고 있는 동안 7-세그먼트의 점(DP) 2개가 켜짐
//
// 7-세그먼트는 BCD-to-7세그 디코더(CD4543B)가 자리마다 값을 유지해 주므로, 칩은 자리당 BCD 4핀(A~D)과
// 점(DP) 1핀을 GPIO 출력으로 내보내기만 하면 됩니다(다이내믹 스캔 불필요). 표시 값 계산과 출력 워드 만들기는
// 칩과 무관한 순수 C 모듈 seg_counter.c/.h가 맡고, 세 버전(Bitfield/DriverLib/FreeRTOS)이 이 모듈을 똑같이
// 공유합니다. 세 버전의 차이는 GPIO/eQEP 레지스터를 만지는 방법과 반복 실행 방식뿐입니다.
//
// a.	예제 실행을 위해 아래와 같이 개발보드 핀-헤더와 TMS320F28P65x 칩의 GPIO 포트를 점퍼선으로 연결하세요.
//		모두 개발보드 A Side 핀-헤더의 핀 번호입니다.
//
//		(13) 7-세그먼트 신호 입력 핀-헤더          GPIO      A Side 핀
//			>> A1 (1번 자리 BCD, LSB)            GPIO 0     65번
//			>> B1                                 GPIO 1     67번
//			>> C1                                 GPIO 2     69번
//			>> D1 (1번 자리 BCD, MSB)            GPIO 3     71번
//			>> DP1 (High = 점 켜짐)              GPIO 4     73번
//			>> A2 (2번 자리 BCD, LSB)            GPIO 5     75번
//			>> B2                                 GPIO 6     77번
//			>> C2                                 GPIO 7     79번
//			>> D2 (2번 자리 BCD, MSB)            GPIO 8     81번
//			>> DP2 (High = 점 켜짐)              GPIO 9     83번
//
//		(14) 엔코더 & TACT 스위치 신호 출력 핀-헤더   GPIO      A Side 핀
//			>> A (엔코더 A상)                     GPIO 20    105번   (eQEP1 QEPA)
//			>> B (엔코더 B상)                     GPIO 21    107번   (eQEP1 QEPB)
//			>> S/W (노브 스위치, Active High)     GPIO 22    109번
//
//		1번 자리는 개발보드의 왼쪽(십의 자리), 2번 자리는 오른쪽(일의 자리)입니다.
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> displayValue(표시 중인 값 0~99), encoderPos(eQEP 위치 카운터, 눈금당 4 증가),
//     knobPressed(노브 스위치 상태, 1 = 눌림), segOutput(GPIO0~9로 내보내는 10비트 출력 워드)
//
// 본 예제는 28X 칩 MMR(Memory Mapped Register)을 직접 조작하는 TI의 Bit-Field Approach
// 만으로 제작되었습니다 (Driverlib 미사용, driverlib.lib 자체가 이 프로젝트에는 없습니다).
// 같은 동작을 DriverLib API로 구현한 13_14_SevenSeg_Encoder_Driverlib.c, FreeRTOS 태스크로
// 구현한 13_14_SevenSeg_Encoder_Freertos.c와 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "f28x_project.h"		// TI 제공 칩-지원 헤더 통합 Include 용 헤더파일 (bit-field)
#include "seg_counter.h"		// 엔코더 값 -> 00~99 표시 값, 7-세그먼트 출력 워드 변환 (칩 무관 순수 C)

// 전처리 구문 정의
#define	SEG_FIRST_GPIO	0U				// 7-세그먼트 출력용 GPIO 시작 번호 (GPIO0~GPIO9, 비트 n = GPIOn)
#define	SEG_NUM_GPIO	10U				// 7-세그먼트 출력용 GPIO 개수 (자리당 A,B,C,D,DP 5개 x 2자리)
#define	SEG_MASK		0x000003FFUL	// GPIO0~GPIO9에 해당하는 GPADAT 비트 마스크
#define	ENC_A_GPIO		20U				// 엔코더 A상  (eQEP1 QEPA)
#define	ENC_B_GPIO		21U				// 엔코더 B상  (eQEP1 QEPB)
#define	ENC_SW_GPIO		22U				// 엔코더 노브 스위치 (Active High)
#define	ENC_GPIO_MUX	1U				// GPIO20/21을 eQEP1로 쓰는 멀티플렉서 값 (pin_map.h의 GPIO_20_EQEP1_A)

// 함수 원형 선언
static void initSegmentGpio(void);
static void initEncoderGpio(void);
static void initEqep1(void);
static void writeSegments(Uint16 word);


// 전역 변수 선언
volatile Uint16	displayValue;		// 7-세그먼트에 표시 중인 값 (0~99)
volatile Uint32	encoderPos;			// eQEP1 위치 카운터(QPOSCNT) 값, 눈금 하나에 4씩 증감
volatile Uint16	knobPressed;		// 엔코더 노브 스위치 상태 (1 = 눌림)
volatile Uint16	segOutput;			// GPIO0~GPIO9로 내보내는 10비트 출력 워드


// 메인 함수
void main(void)
{
	SegCounter counter;

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
	DINT;			// 전역 인터럽트 스위치 OFF (/INTM OFF)
	IER = 0x0000;	// CPU 인터럽트 벡터 비-활성화
	IFR = 0x0000;	// CPU 인터럽트 플래그 클리어


//	2. 시스템 초기화 - InitSysCtrl( ) 함수 호출 (f28p65x_sysctrl.c)
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL)
//	* 주변회로 클럭 공급 설정 (eQEP1 클럭 포함)
	InitSysCtrl();

//	* 범용 입출력 포트(GPIO) 설정 - InitGpio( ) 함수 호출
	InitGpio();


//	3. 주변회로 인터럽트 확장회로 초기화 - InitPieCtrl( ) 함수 호출 (f28p65x_piectrl.c)
	InitPieCtrl();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - InitPieVectTable( ) 함수 호출 (f28p65x_pievect.c)
	InitPieVectTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결, 인터럽트 벡터 활성화
//	   (본 예제는 인터럽트를 사용하지 않습니다)


//	6. 주변회로 초기화
	initSegmentGpio();		// GPIO0~GPIO9를 7-세그먼트 출력으로 설정 ("00"을 먼저 내보냄)
	initEncoderGpio();		// GPIO20, 21을 eQEP1 입력으로, GPIO22를 스위치 입력으로 설정
	initEqep1();			// eQEP1을 쿼드러처 모드로 설정하고 위치 카운터 시작


//	7. 전역 변수 및 S/W 모듈 초기화
	SegCounter_init(&counter, EQep1Regs.QPOSCNT);
	displayValue = 0U;
	encoderPos = EQep1Regs.QPOSCNT;
	knobPressed = 0U;
	segOutput = 0U;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정 (실시간 디버깅이 가능하도록 ST1 레지스터의 /DBGM 비트를 0으로 클리어)
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. Idle(Background) Loop
	for(;;)
	{
		Uint32 pos = EQep1Regs.QPOSCNT;												// 엔코더 위치 카운터 읽기
		Uint16 pressed = ((GpioDataRegs.GPADAT.all & (1UL << ENC_SW_GPIO)) != 0UL) ? 1U : 0U;	// 노브 스위치 읽기

		SegCounter_update(&counter, pos, pressed);

		encoderPos = pos;
		knobPressed = pressed;
		displayValue = (Uint16)counter.value;
		segOutput = SegCounter_displayWord(&counter);
		writeSegments(segOutput);

		DELAY_US(10000);	// 10msec 지연 (폴링 주기)
	}
}

//	10. 인터럽트 서비스 루틴 및 기타 함수들
//	    (본 예제는 인터럽트 서비스 루틴이 없습니다)

//
// writeSegments - 10비트 출력 워드를 GPIO0~GPIO9에 내보냅니다. 1인 비트는 GPASET으로,
// 0인 비트는 GPACLEAR로 처리해서 다른 GPIO(eQEP 입력 등)의 출력 래치는 건드리지 않습니다.
//
static void writeSegments(Uint16 word)
{
	GpioDataRegs.GPASET.all = (Uint32)word & SEG_MASK;
	GpioDataRegs.GPACLEAR.all = (~(Uint32)word) & SEG_MASK;
}

//
// initSegmentGpio - GPIO0~GPIO9를 일반 GPIO 출력(푸시풀)으로 설정. 출력 래치를 먼저 0으로
// 만든 뒤 출력 방향을 켜서, 켜지는 순간 임의의 숫자가 깜빡이지 않게 합니다.
//
static void initSegmentGpio(void)
{
	Uint16 i;

	writeSegments(0U);
	for(i = 0U; i < SEG_NUM_GPIO; i++)
	{
		GPIO_SetupPinMux(SEG_FIRST_GPIO + i, GPIO_MUX_CPU1, 0);
		GPIO_SetupPinOptions(SEG_FIRST_GPIO + i, GPIO_OUTPUT, GPIO_PUSHPULL);
	}
}

//
// initEncoderGpio - GPIO20/21은 eQEP1의 QEPA/QEPB 입력으로, GPIO22는 노브 스위치용 일반
// 입력으로 설정합니다. 엔코더 신호는 개발보드에서 슈미트 트리거 버퍼를 거쳐 나오고 노브
// 스위치 쪽에는 10k 풀다운이 있으므로, 칩 내부 풀업은 켜지 않습니다(풀업이 스위치 출력의
// Low 레벨을 끌어올릴 수 있음).
//
static void initEncoderGpio(void)
{
	GPIO_SetupPinMux(ENC_A_GPIO, GPIO_MUX_CPU1, ENC_GPIO_MUX);
	GPIO_SetupPinOptions(ENC_A_GPIO, GPIO_INPUT, GPIO_SYNC);

	GPIO_SetupPinMux(ENC_B_GPIO, GPIO_MUX_CPU1, ENC_GPIO_MUX);
	GPIO_SetupPinOptions(ENC_B_GPIO, GPIO_INPUT, GPIO_SYNC);

	GPIO_SetupPinMux(ENC_SW_GPIO, GPIO_MUX_CPU1, 0);
	GPIO_SetupPinOptions(ENC_SW_GPIO, GPIO_INPUT, GPIO_SYNC);
}

//
// initEqep1 - eQEP1을 쿼드러처 카운트 모드로 설정. A/B의 네 에지를 모두 세므로(x4)
// 1펄스 = 4카운트이고, 위치 카운터는 32비트 전체 범위(0 <-> 0xFFFFFFFF)에서 돕니다.
// 00~99로 제한하는 일은 seg_counter 모듈이 소프트웨어로 처리합니다.
//
static void initEqep1(void)
{
	EQep1Regs.QEPCTL.bit.QPEN = 0;			// 설정하는 동안 위치 카운터 정지 (리셋)
	EQep1Regs.QDECCTL.all = 0x0000;			// QSRC=0 쿼드러처 카운트, SWAP=0, 입력 극성 그대로
	EQep1Regs.QEPCTL.bit.FREE_SOFT = 2;		// 에뮬레이션(브레이크포인트) 중에도 계속 카운트
	EQep1Regs.QEPCTL.bit.PCRM = 1;			// 최대 위치(QPOSMAX)에서 위치 카운터 리셋
	EQep1Regs.QPOSMAX = 0xFFFFFFFFUL;
	EQep1Regs.QPOSINIT = 0UL;
	EQep1Regs.QPOSCNT = 0UL;
	EQep1Regs.QEPCTL.bit.QPEN = 1;			// 위치 카운터 동작 시작
}

// 파일 끝.
