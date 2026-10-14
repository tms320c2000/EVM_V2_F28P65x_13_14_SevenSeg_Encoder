// 파일이름	:	13_14_SevenSeg_Encoder_Freertos.c
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
//     knobPressed(노브 스위치 상태, 1 = 눌림), segOutput(GPIO0~9로 내보내는 10비트 출력 워드),
//     displayUpdateCount(표시 태스크가 7-세그먼트를 갱신한 횟수)
//
// 본 예제는 13_14_SevenSeg_Encoder_Driverlib.c와 완전히 동일한 DriverLib GPIO/eQEP 제어 코드를
// 쓰되, 반복 실행을 FreeRTOS 태스크 2개로 나누어 구현했습니다.
//
//		엔코더 입력 태스크 : 10ms마다 eQEP 위치와 노브 스위치를 읽어 표시 값을 갱신하고,
//		                      출력 워드가 바뀐 경우에만 표시 태스크에 알림(task notification)을 보냄
//		표시 태스크        : 알림이 올 때까지 잠자고 있다가, 깨어나면 7-세그먼트 GPIO를 갱신
//
// 힙(heap)을 전혀 쓰지 않는 정적 할당 구성입니다(configSUPPORT_DYNAMIC_ALLOCATION=0,
// xTaskCreateStatic 사용). 레지스터를 직접 조작하는 Bit-Field 방식의
// 13_14_SevenSeg_Encoder_Bitfield.c와 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "driverlib.h"		// TI 제공 Driver API Library 헤더파일 (driverlib)
#include "device.h"
#include "FreeRTOS.h"
#include "task.h"
#include "seg_counter.h"	// 엔코더 값 -> 00~99 표시 값, 7-세그먼트 출력 워드 변환 (칩 무관 순수 C)

// 전처리 구문 정의
#define	NUM_SEG_GPIO	10U				// 7-세그먼트 출력용 GPIO 개수 (GPIO0~GPIO9, 비트 n = GPIOn)
#define	SEG_MASK		0x000003FFUL	// GPIO0~GPIO9에 해당하는 포트 A 비트 마스크
#define	ENC_SW_GPIO		22U				// 엔코더 노브 스위치 (Active High)
#define	STACK_SIZE		256U			// 태스크 스택 크기, 워드 단위 (C28x는 1워드=16비트이므로 512바이트)

// 함수 원형 선언
static void initSegmentGpio(void);
static void initEncoderGpio(void);
static void initEqep1(void);
static void writeSegments(uint16_t word);
void Encoder_Task(void *pvParameters);
void Display_Task(void *pvParameters);


// 전역 변수 선언
volatile uint16_t	displayValue;		// 7-세그먼트에 표시 중인 값 (0~99)
volatile uint32_t	encoderPos;			// eQEP1 위치 카운터(QPOSCNT) 값, 눈금 하나에 4씩 증감
volatile uint16_t	knobPressed;		// 엔코더 노브 스위치 상태 (1 = 눌림)
volatile uint16_t	segOutput;			// GPIO0~GPIO9로 내보내는 10비트 출력 워드
volatile uint32_t	displayUpdateCount;	// 표시 태스크가 7-세그먼트를 갱신한 횟수

// GPIO0~GPIO9를 일반 GPIO로 지정하는 핀 설정 값 (pin_map.h). 인덱스 = GPIO 번호
static const uint32_t segPinConfig[NUM_SEG_GPIO] =
{
    GPIO_0_GPIO0, GPIO_1_GPIO1, GPIO_2_GPIO2, GPIO_3_GPIO3, GPIO_4_GPIO4,
    GPIO_5_GPIO5, GPIO_6_GPIO6, GPIO_7_GPIO7, GPIO_8_GPIO8, GPIO_9_GPIO9
};

// FreeRTOS 태스크용 정적 메모리 선언 - 엔코더 태스크 1개 + 표시 태스크 1개 + Idle 태스크 1개,
// 전부 정적 할당(힙 미사용)입니다.
static StaticTask_t encoderTaskBuffer;
static StackType_t  encoderTaskStack[STACK_SIZE];
#pragma DATA_SECTION(encoderTaskStack, ".freertosStaticStack")
#pragma DATA_ALIGN(encoderTaskStack, portBYTE_ALIGNMENT)

static StaticTask_t displayTaskBuffer;
static StackType_t  displayTaskStack[STACK_SIZE];
#pragma DATA_SECTION(displayTaskStack, ".freertosStaticStack")
#pragma DATA_ALIGN(displayTaskStack, portBYTE_ALIGNMENT)

static StaticTask_t idleTaskBuffer;
static StackType_t  idleTaskStack[STACK_SIZE];
#pragma DATA_SECTION(idleTaskStack, ".freertosStaticStack")
#pragma DATA_ALIGN(idleTaskStack, portBYTE_ALIGNMENT)

static TaskHandle_t displayTaskHandle;	// 엔코더 태스크가 알림을 보낼 대상


// 메인 함수
void main(void)
{

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
//	   (DriverLib에서는 별도 처리 없이, 3번의 Interrupt_initModule( ) 함수가 한 번에
//	   담당합니다 — 아래 3번 참고)


//	2. 시스템 초기화 - Device_init( ) 함수 호출
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL)
//	* 주변회로 클럭 공급 설정
	Device_init();

//	* 범용 입출력 포트(GPIO) 핀 락 해제, 내부 풀업 활성화 - Device_initGPIO( ) 함수 호출
	Device_initGPIO();


//	3. 주변회로 인터럽트 확장회로 초기화 - Interrupt_initModule( ) 함수 호출
//	   (전역 인터럽트 비활성화 + PIE 인터럽트 인에이블/플래그 클리어까지 이 함수 하나로
//	   처리합니다 — 클래식 Bit-Field 버전의 1번+3번을 합친 것과 같습니다)
	Interrupt_initModule();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - Interrupt_initVectorTable( ) 함수 호출
	Interrupt_initVectorTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결, 인터럽트 벡터 활성화
//	   (본 예제는 인터럽트를 사용하지 않습니다)


//	6. 주변회로 초기화
	initSegmentGpio();		// GPIO0~GPIO9를 7-세그먼트 출력으로 설정 ("00"을 먼저 내보냄)
	initEncoderGpio();		// GPIO20, 21을 eQEP1 입력으로, GPIO22를 스위치 입력으로 설정
	initEqep1();			// eQEP1을 쿼드러처 모드로 설정하고 위치 카운터 시작


//	7. 전역 변수 및 S/W 모듈 초기화
	displayValue = 0U;
	encoderPos = EQEP_getPosition(EQEP1_BASE);
	knobPressed = 0U;
	segOutput = 0U;
	displayUpdateCount = 0UL;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정 (실시간 디버깅이 가능하도록 ST1 레지스터의 /DBGM 비트를 0으로 클리어)
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. FreeRTOS 태스크 생성 및 스케줄러 시작
//	   (Idle Loop를 직접 돌리지 않고, 두 태스크를 스케줄러에 맡깁니다 — 아래 10번 참고)
	displayTaskHandle = xTaskCreateStatic(Display_Task,			// 태스크 함수
	                                      "Display Task",		// 이름(디버깅용)
	                                      STACK_SIZE,			// 스택 크기(워드)
	                                      NULL,					// 파라미터 없음
	                                      tskIDLE_PRIORITY + 1,
	                                      displayTaskStack,
	                                      &displayTaskBuffer);

	xTaskCreateStatic(Encoder_Task,
	                  "Encoder Task",
	                  STACK_SIZE,
	                  NULL,
	                  tskIDLE_PRIORITY + 2,		// 입력 태스크의 우선순위를 표시 태스크보다 한 단계 높게
	                  encoderTaskStack,
	                  &encoderTaskBuffer);

	vTaskStartScheduler();		// 이 아래로는 절대 돌아오지 않음

	for(;;)
	{
	    // 여기 도달하면 스케줄러 시작 실패 (메모리 부족 등)
	}
}

//	10. 인터럽트 서비스 루틴 및 기타 함수들

//
// Encoder_Task - DriverLib 버전의 메인 루프 중 "읽고 계산하는" 부분에 해당합니다. 출력 워드가
// 바뀐 경우에만 표시 태스크를 깨웁니다(알림). DEVICE_DELAY_US busy-wait 대신 vTaskDelay로
// 다른 태스크(표시, Idle)에게 CPU를 양보합니다.
//
void Encoder_Task(void *pvParameters)
{
    SegCounter counter;
    uint16_t   lastWord = 0U;

    (void)pvParameters;

    SegCounter_init(&counter, EQEP_getPosition(EQEP1_BASE));

    for(;;)
    {
        uint32_t pos = EQEP_getPosition(EQEP1_BASE);
        uint16_t pressed = (GPIO_readPin(ENC_SW_GPIO) != 0U) ? 1U : 0U;
        uint16_t word;

        SegCounter_update(&counter, pos, pressed);
        word = SegCounter_displayWord(&counter);

        encoderPos = pos;
        knobPressed = pressed;
        displayValue = (uint16_t)counter.value;
        segOutput = word;

        if(word != lastWord)
        {
            lastWord = word;
            xTaskNotifyGive(displayTaskHandle);		// 표시 태스크 깨우기
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);	// 10msec 지연 (폴링 주기)
    }
}

//
// Display_Task - DriverLib 버전의 메인 루프 중 "7-세그먼트에 내보내는" 부분에 해당합니다.
// 알림이 올 때까지 CPU를 쓰지 않고 잠들어 있다가, 깨어나면 최신 출력 워드를 내보냅니다.
// 알림이 짧은 시간에 여러 번 와도 한 번에 모아서(ulTaskNotifyTake의 pdTRUE) 처리하므로
// 항상 가장 최근 값이 표시됩니다.
//
void Display_Task(void *pvParameters)
{
    (void)pvParameters;

    for(;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        writeSegments(segOutput);
        displayUpdateCount++;
    }
}

//
// writeSegments - 10비트 출력 워드를 GPIO0~GPIO9에 내보냅니다. 1인 비트는 setPortPins로,
// 0인 비트는 clearPortPins로 처리해서 다른 GPIO(eQEP 입력 등)의 출력 래치는 건드리지 않습니다.
//
static void writeSegments(uint16_t word)
{
	GPIO_setPortPins(GPIO_PORT_A, (uint32_t)word & SEG_MASK);
	GPIO_clearPortPins(GPIO_PORT_A, (~(uint32_t)word) & SEG_MASK);
}

//
// initSegmentGpio - GPIO0~GPIO9를 일반 GPIO 출력(푸시풀)으로 설정. 출력 래치를 먼저 0으로
// 만든 뒤 출력 방향을 켜서, 켜지는 순간 임의의 숫자가 깜빡이지 않게 합니다.
//
static void initSegmentGpio(void)
{
	uint16_t i;

	writeSegments(0U);
	for(i = 0U; i < NUM_SEG_GPIO; i++)
	{
		GPIO_setPinConfig(segPinConfig[i]);
		GPIO_setPadConfig(i, GPIO_PIN_TYPE_STD);
		GPIO_setDirectionMode(i, GPIO_DIR_MODE_OUT);
	}
}

//
// initEncoderGpio - GPIO20/21은 eQEP1의 QEPA/QEPB 입력으로, GPIO22는 노브 스위치용 일반
// 입력으로 설정합니다. 엔코더 신호는 개발보드에서 슈미트 트리거 버퍼를 거쳐 나오고 노브
// 스위치 쪽에는 10k 풀다운이 있으므로, 칩 내부 풀업은 끕니다(GPIO_PIN_TYPE_STD).
// (Device_initGPIO( )가 켜 둔 풀업을 여기서 해제하는 것입니다.)
//
static void initEncoderGpio(void)
{
	GPIO_setPinConfig(GPIO_20_EQEP1_A);
	GPIO_setPadConfig(20U, GPIO_PIN_TYPE_STD);
	GPIO_setQualificationMode(20U, GPIO_QUAL_SYNC);

	GPIO_setPinConfig(GPIO_21_EQEP1_B);
	GPIO_setPadConfig(21U, GPIO_PIN_TYPE_STD);
	GPIO_setQualificationMode(21U, GPIO_QUAL_SYNC);

	GPIO_setPinConfig(GPIO_22_GPIO22);
	GPIO_setPadConfig(ENC_SW_GPIO, GPIO_PIN_TYPE_STD);
	GPIO_setDirectionMode(ENC_SW_GPIO, GPIO_DIR_MODE_IN);
	GPIO_setQualificationMode(ENC_SW_GPIO, GPIO_QUAL_SYNC);
}

//
// initEqep1 - eQEP1을 쿼드러처 카운트 모드로 설정. A/B의 네 에지를 모두 세므로(x4)
// 1펄스 = 4카운트이고, 위치 카운터는 32비트 전체 범위(0 <-> 0xFFFFFFFF)에서 돕니다.
// 00~99로 제한하는 일은 seg_counter 모듈이 소프트웨어로 처리합니다.
//
static void initEqep1(void)
{
	SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EQEP1);

	EQEP_disableModule(EQEP1_BASE);			// 설정하는 동안 위치 카운터 정지 (리셋)
	EQEP_setDecoderConfig(EQEP1_BASE, (EQEP_CONFIG_QUADRATURE | EQEP_CONFIG_NO_SWAP));
	EQEP_setEmulationMode(EQEP1_BASE, EQEP_EMULATIONMODE_RUNFREE);	// 브레이크포인트 중에도 계속 카운트
	EQEP_setPositionCounterConfig(EQEP1_BASE, EQEP_POSITION_RESET_MAX_POS, 0xFFFFFFFFUL);
	EQEP_setInitialPosition(EQEP1_BASE, 0UL);
	EQEP_setPosition(EQEP1_BASE, 0UL);
	EQEP_enableModule(EQEP1_BASE);			// 위치 카운터 동작 시작
}

//
// vApplicationStackOverflowHook - FreeRTOS가 스택오버플로우를 감지하면 호출
//
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for(;;) { }
}

//
// vApplicationGetIdleTaskMemory - configSUPPORT_STATIC_ALLOCATION=1 이면 Idle 태스크
// 메모리도 애플리케이션이 직접 제공해야 함(힙을 안 쓰므로)
//
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &idleTaskBuffer;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pulIdleTaskStackSize = STACK_SIZE;
}

// 파일 끝.
