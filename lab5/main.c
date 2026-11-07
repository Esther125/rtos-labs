/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Standard includes. */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
functionality in an interrupt. */
#include "EventGroupsDemo.h"
#include "TaskNotify.h"

/* TI includes. */
#include "driverlib.h"

/* Set mainCREATE_SIMPLE_BLINKY_DEMO_ONLY to one to run the simple blinky demo,
or 0 to run the more comprehensive test and demo application. */
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY	1

/*-----------------------------------------------------------*/

/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

/*
 * main_blinky() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1.
 * main_full() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 0.
 */
// LAB5: blink-only 模式會改呼叫 main_lab() 進入 checkpoint demo
#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
	extern void main_lab( void );
#else
	extern void main_full( void );
#endif /* #if mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 */

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );
void vApplicationSetupTimerInterrupt( void );

/* ============================== 常數 ===============================*/
// LAB5: ping pong buffer 機制本身用到的常數
#define lab5CHECKPOINT_MAGIC            ( ( uint32_t ) 0xC0DEC10CUL )
#define lab5CHECKPOINT_COPIES           ( 2U )

// LAB5: 要備份的 live SRAM 範圍
#define lab5SRAM_START                  ( ( uint8_t * ) 0x1C00U )
#define lab5SRAM_SIZE                   ( ( size_t ) 0x0C00U )

// LAB5: demo task 本身的參數 
#define lab5TASK_STACK_SIZE             ( configMINIMAL_STACK_SIZE * 2U )
#define lab5TASK_PRIORITY               ( tskIDLE_PRIORITY + 1U )

/* ============================== 記憶體分配 ===============================*/
/* The heap is allocated here so the "persistent" qualifier can be used.  This
requires configAPPLICATION_ALLOCATED_HEAP to be set to 1 in FreeRTOSConfig.h.
See http://www.freertos.org/a00111.html for more information. */
#ifdef __ICC430__
	__persistent 					/* IAR version. */
#else
	#pragma PERSISTENT( ucHeap ) 	/* CCS version. */
#endif
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] = { 0 };

// LAB5: 另外存 random seed（不包進 checkpoint 備份）
// 確保每次 restore 後不會重複同一個 rand() 斷電點
#ifdef __ICC430__
	__persistent
#else
	#pragma PERSISTENT( ulLab5RestoreSeed )
#endif
static uint32_t ulLab5RestoreSeed = 1UL;

// LAB5: 一份 commit 的 metadata
typedef struct Lab5CheckpointHeader
{
	uint32_t ulMagic;
	uint32_t ulSequence;
} Lab5CheckpointHeader_t;

// LAB5: 開 ping pong buffer，並透過 pragma 放到 .lab5_checkpoint data section
// linker command file 會設定把 .lab5_checkpoint 放到 FRAM2 裡面 
#ifdef __ICC430__
	__persistent
#else
	#pragma DATA_SECTION( ucLab5BackupHeap, ".lab5_checkpoint" )
#endif
static uint8_t ucLab5BackupHeap[ lab5CHECKPOINT_COPIES ][ configTOTAL_HEAP_SIZE ];

#ifdef __ICC430__
	__persistent
#else
	#pragma DATA_SECTION( ucLab5BackupSram, ".lab5_checkpoint" )
#endif
static uint8_t ucLab5BackupSram[ lab5CHECKPOINT_COPIES ][ lab5SRAM_SIZE ];

#ifdef __ICC430__
	__persistent
#else
	#pragma DATA_SECTION( xLab5BackupHeader, ".lab5_checkpoint" )
#endif
static Lab5CheckpointHeader_t xLab5BackupHeader[ lab5CHECKPOINT_COPIES ];

/* ==================== 自定義 function 的 function prototype =====================*/
extern void vLab5SaveCurrentTaskContext( void );
extern void vLab5RestoreCurrentTaskContext( void );

static void prvLab5Task( void *pvParameters );
static void prvLab5Commit( void );
static void prvLab5RestoreIfNeeded( void );
static void prvLab5PowerOff( void );
static uint16_t usLab5SlotIsValid( uint16_t usCopyIndex );
static uint16_t usLab5ValidCopyIndex( void );

/*-----------------------------------------------------------*/

int main( void )
{
	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	/* The mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is described at the top
	of this file. */

	// LAB5: blink-only 模式導向 checkpoint lab 的進入點
	#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
	{
		main_lab();
	}
	#else
	{
		main_full();
	}
	#endif

	return 0;
}
/*-----------------------------------------------------------*/

// LAB5: 這個 lab 的 entry point
void main_lab( void )
{
	prvLab5RestoreIfNeeded();

	xTaskCreate( prvLab5Task,
	             "LAB5",
	             lab5TASK_STACK_SIZE,
	             NULL,
	             lab5TASK_PRIORITY,
	             NULL );

	vTaskStartScheduler();

	for( ;; );
}
/*-----------------------------------------------------------*/

// LAB5: demo task：計數器遞增、視情況斷電、每十次 commit 一次、印出目前計數
static void prvLab5Task( void *pvParameters )
{
	static uint16_t usCounter = 0U;
	uint16_t usLastCommit = 0U;

	( void ) pvParameters;
	srand( 1U );

	for( ;; )
	{
		usCounter++;
		// 1. 用 rand() 隨機觸發斷電，並避免 stagnation
		if( ( usCounter > 10U ) &&
		    ( ( usCounter - usLastCommit ) >= 4U ) &&
		    ( ( rand() % 23 ) == 0 ) )
		{
			printf( "%u <- Power off\r\n", usCounter );
			prvLab5PowerOff();
		}
		// 2. Checkpoint commit
		if( ( usCounter % 10U ) == 0U )
		{
			prvLab5Commit();
			usLastCommit = usCounter;
			printf( "%u <- commit\r\n", usCounter );
		}
		// 3. 印出目前計數
		else
		{
			printf( "%u\r\n", usCounter );
		}

		vTaskDelay( pdMS_TO_TICKS( 100 ) );
	}
}
/*-----------------------------------------------------------*/

// LAB5: commit 前先把目前 task 的 CPU context 定格
// 再依序備份 ucHeap、SRAM 到目前沒在用的那個 FRAM checkpoint slot
static void prvLab5Commit( void )
{
	const uint16_t usValidIndex = usLab5ValidCopyIndex();
	const uint16_t usTargetIndex = ( usValidIndex < lab5CHECKPOINT_COPIES ) ? ( uint16_t ) ( usValidIndex ^ 1U ) : 0U;
	const uint32_t ulPrevSequence = ( usValidIndex < lab5CHECKPOINT_COPIES ) ? xLab5BackupHeader[ usValidIndex ].ulSequence : 0UL;
	const uint32_t ulNextSequence = ulPrevSequence + 1UL;
	Lab5CheckpointHeader_t * const pxHeader = &xLab5BackupHeader[ usTargetIndex ];

	taskENTER_CRITICAL();
	{
		// 1. 備份 CPU registers
		vLab5SaveCurrentTaskContext();

		pxHeader->ulMagic = 0UL;
		// 2. 備份 ucHeap
		memcpy( ucLab5BackupHeap[ usTargetIndex ], ucHeap, configTOTAL_HEAP_SIZE );
		// 3. 備份 SRAM
		memcpy( ucLab5BackupSram[ usTargetIndex ], lab5SRAM_START, lab5SRAM_SIZE );

		pxHeader->ulSequence = ulNextSequence;
		pxHeader->ulMagic = lab5CHECKPOINT_MAGIC;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

// LAB5: 如果有有效的 FRAM checkpoint 就 restore
//       順序：1. ucHeap 2. SRAM 3. CPU register
static void prvLab5RestoreIfNeeded( void )
{
	const uint16_t usSourceIndex = usLab5ValidCopyIndex();

	if( usSourceIndex < lab5CHECKPOINT_COPIES )
	{
		// 1. 還原 ucHeap
		memcpy( ucHeap, ucLab5BackupHeap[ usSourceIndex ], configTOTAL_HEAP_SIZE );
		// 2. 還原 SRAM
		memcpy( lab5SRAM_START, ucLab5BackupSram[ usSourceIndex ], lab5SRAM_SIZE );
		PMM_clearInterrupt( PMM_LPM5_INTERRUPT );
		ulLab5RestoreSeed++;
		srand( ( unsigned int ) ulLab5RestoreSeed );
		vApplicationSetupTimerInterrupt();
		// 3. 還原 CPU registers
		vLab5RestoreCurrentTaskContext();
	}
}
/*-----------------------------------------------------------*/

// LAB5: 進入 LPM4.5 模擬斷電
static void prvLab5PowerOff( void )
{
	PMM_turnOffRegulator();
	__bis_SR_register( LPM4_bits | GIE );
	__no_operation();

	for( ;; );
}
/*-----------------------------------------------------------*/

// LAB5: 檢查單一個 slot 的 magic number，確認他是合法的 slot
static uint16_t usLab5SlotIsValid( uint16_t usCopyIndex )
{
	Lab5CheckpointHeader_t * const pxHeader = &xLab5BackupHeader[ usCopyIndex ];

	return ( uint16_t ) ( pxHeader->ulMagic == lab5CHECKPOINT_MAGIC );
}
/*-----------------------------------------------------------*/

// LAB5: 從兩個 FRAM2 backup slot 裡挑出最新且合法的 checkpoint slot
static uint16_t usLab5ValidCopyIndex( void )
{
	Lab5CheckpointHeader_t * const pxHeader0 = &xLab5BackupHeader[ 0U ];
	Lab5CheckpointHeader_t * const pxHeader1 = &xLab5BackupHeader[ 1U ];

	// 驗證 slot 是不是合法的
	const uint16_t usValid0 = usLab5SlotIsValid( 0U );
	const uint16_t usValid1 = usLab5SlotIsValid( 1U );

	// 兩個 slot 都有效 -> 選 sequence 大的（新的）
	if( ( usValid0 != 0U ) && ( usValid1 != 0U ) )
	{
		return ( pxHeader1->ulSequence > pxHeader0->ulSequence ) ? 1U : 0U;
	}

	if( usValid0 != 0U )
	{
		return 0U;
	}

	if( usValid1 != 0U )
	{
		return 1U;
	}

	return lab5CHECKPOINT_COPIES; // 兩個 slot 都無效，回傳哨兵值
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.
	See http://www.freertos.org/Stacks-and-stack-overflow-checking.html */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    __bis_SR_register( LPM4_bits + GIE );
    __no_operation();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0 )
	{
		/* Call the periodic event group from ISR demo. */
		vPeriodicEventGroupsProcessing();

		/* Call the code that 'gives' a task notification from an ISR. */
		xNotifyTaskFromISR();
	}
	#endif
}
/*-----------------------------------------------------------*/

/* The MSP430X port uses this callback function to configure its tick interrupt.
This allows the application to choose the tick interrupt source.
configTICK_VECTOR must also be set in FreeRTOSConfig.h to the correct
interrupt vector for the chosen tick interrupt source.  This implementation of
vApplicationSetupTimerInterrupt() generates the tick from timer A0, so in this
case configTICK_VECTOR is set to TIMER0_A0_VECTOR. */
void vApplicationSetupTimerInterrupt( void )
{
const unsigned short usACLK_Frequency_Hz = 32768;

	/* Ensure the timer is stopped. */
	TA0CTL = 0;

	/* Run the timer from the ACLK. */
	TA0CTL = TASSEL_1;

	/* Clear everything to start with. */
	TA0CTL |= TACLR;

	/* Set the compare match value according to the tick rate we want. */
	TA0CCR0 = usACLK_Frequency_Hz / configTICK_RATE_HZ;

	/* Enable the interrupts. */
	TA0CCTL0 = CCIE;

	/* Start up clean. */
	TA0CTL |= TACLR;

	/* Up mode. */
	TA0CTL |= MC_1;
}
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__ );

	/* Set all GPIO pins to output and low. */
	GPIO_setOutputLowOnPin( GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setOutputLowOnPin( GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setOutputLowOnPin( GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setOutputLowOnPin( GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setOutputLowOnPin( GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15 );
	GPIO_setAsOutputPin( GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setAsOutputPin( GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setAsOutputPin( GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setAsOutputPin( GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 );
	GPIO_setAsOutputPin( GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15 );

	/* Configure P2.0 - UCA0TXD and P2.1 - UCA0RXD. */
	GPIO_setOutputLowOnPin( GPIO_PORT_P2, GPIO_PIN0 );
	GPIO_setAsOutputPin( GPIO_PORT_P2, GPIO_PIN0 );
	GPIO_setAsPeripheralModuleFunctionInputPin( GPIO_PORT_P2, GPIO_PIN1, GPIO_SECONDARY_MODULE_FUNCTION );
	GPIO_setAsPeripheralModuleFunctionOutputPin( GPIO_PORT_P2, GPIO_PIN0, GPIO_SECONDARY_MODULE_FUNCTION );

	/* Set PJ.4 and PJ.5 for LFXT. */
	GPIO_setAsPeripheralModuleFunctionInputPin(  GPIO_PORT_PJ, GPIO_PIN4 + GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION  );

	/* Set DCO frequency to 8 MHz. */
	CS_setDCOFreq( CS_DCORSEL_0, CS_DCOFSEL_6 );

	/* Set external clock frequency to 32.768 KHz. */
	CS_setExternalClockSource( 32768, 0 );

	/* Set ACLK = LFXT. */
	CS_initClockSignal( CS_ACLK, CS_LFXTCLK_SELECT, CS_CLOCK_DIVIDER_1 );

	/* Set SMCLK = DCO with frequency divider of 1. */
	CS_initClockSignal( CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1 );

	/* Set MCLK = DCO with frequency divider of 1. */
	CS_initClockSignal( CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1 );

	/* Start XT1 with no time out. */
	CS_turnOnLFXT( CS_LFXT_DRIVE_0 );

	/* Disable the GPIO power-on default high-impedance mode. */
	PMM_unlockLPM5();
}
/*-----------------------------------------------------------*/

int _system_pre_init( void )
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__ );

    /* Return 1 for segments to be initialised. */
    return 1;
}


