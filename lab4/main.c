/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
functionality in an interrupt. */
#include "EventGroupsDemo.h"
#include "TaskNotify.h"
//#include "ParTest.h" /* LEDs - a historic name for "Parallel Port". */

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
 * main_lab4() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1.
 * main_full() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 0.
 */
#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
	static void main_lab4( void );
#else
	extern void main_full( void );
#endif /* #if mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 */

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

/* LAB4: Select which spec task set to run, 1 or 2. */
#define mainLAB_TASK_SET        ( 2 )
#define mainLAB_TICK_LIMIT      ( ( TickType_t ) 20 )
#define mainLAB_TASK_PRIORITY   ( tskIDLE_PRIORITY + 1 )

/* LAB4: Per-task parameters passed through xTaskCreate(). */
typedef struct xLAB_EDF_TASK_PARAMETERS
{
	UBaseType_t uxTaskNumber;
	TickType_t xComputationTime;
	TickType_t xPeriod;
} LabEDFTaskParameters_t;

/* LAB4: Task-set definitions from spec.md. */
// NOTE: task 變數使用 static 讓他從 external linkage -> internal linkage
#if( mainLAB_TASK_SET == 1 )
	static LabEDFTaskParameters_t xLabTasks[] =
	{
		{ 1, 1, 3 },
		{ 2, 3, 5 }
	};
#elif( mainLAB_TASK_SET == 2 )
	static LabEDFTaskParameters_t xLabTasks[] =
	{
		{ 1, 1, 4 },
		{ 2, 2, 5 },
		{ 3, 2, 10 }
	};
#else
	#error mainLAB_TASK_SET must be 1 or 2.
#endif

/* LAB4: Function prototype for the application code. */
static void prvLabEDFTask( void *pvParameters );
static void prvLabRunForTicks( TickType_t xTicksToRun );
static void prvLabPrintTaskName( UBaseType_t uxTaskNumber );
static void prvLabPrintOutputOnce( void );

/* LAB4: Guard so only the first task reaching the limit prints the buffered log. */
// NOTE: pdFALSE 是 FreeRTOS 定義的 marco (其實就是 BaseType_t 0)
static volatile BaseType_t xLabOutputPrinted = pdFALSE;

/* The heap is allocated here so the "persistent" qualifier can be used.  This
requires configAPPLICATION_ALLOCATED_HEAP to be set to 1 in FreeRTOSConfig.h.
See http://www.freertos.org/a00111.html for more information. */
#ifdef __ICC430__
	__persistent 					/* IAR version. */
#else
	#pragma PERSISTENT( ucHeap ) 	/* CCS version. */
#endif
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] = { 0 };

/*-----------------------------------------------------------*/

int main( void )
{
	/* See http://www.FreeRTOS.org/MSP430FR5969_Free_RTOS_Demo.html */

	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	/* The mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is described at the top
	of this file. */
	#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
	{
		/* LAB4: Run the EDF lab instead of the original blinky demo. */
		main_lab4();
	}
	#else
	{
		main_full();
	}
	#endif

	return 0;
}
/*-----------------------------------------------------------*/

/* LAB4: Create the EDF task set from spec.md and start the scheduler. */
static void main_lab4( void )
{
	UBaseType_t uxIndex;
	// 計算 task 的數量
	// NOTE: 這裡要轉 UBaseType_t 類型是因為 size_t 在不同平台上長度可能不一樣
	const UBaseType_t uxTaskCount = ( UBaseType_t ) ( sizeof( xLabTasks ) / sizeof( xLabTasks[ 0 ] ) );

	vTaskEDFClearLog( mainLAB_TICK_LIMIT );

	for( uxIndex = ( UBaseType_t ) 0; uxIndex < uxTaskCount; uxIndex++ )
	{
		printf( "T%d c:%d p:%d\n",
				( int ) xLabTasks[ uxIndex ].uxTaskNumber,
				( int ) xLabTasks[ uxIndex ].xComputationTime,
				( int ) xLabTasks[ uxIndex ].xPeriod );
	}

	for( uxIndex = ( UBaseType_t ) 0; uxIndex < uxTaskCount; uxIndex++ )
	{
		TaskHandle_t xTaskHandle = NULL; // 指向 TCB 的 pointer (opaque pointer)
		LabEDFTaskParameters_t * const pxTask = &( xLabTasks[ uxIndex ] ); // NOTE: pxTask 是一個 const 指標，不能更改它指向的位置

		// 組出 task name (debug 方便)
		char pcTaskName[ 3 ];
		pcTaskName[ 0 ] = 'T';
		pcTaskName[ 1 ] = ( char ) ( '0' + pxTask->uxTaskNumber );
		pcTaskName[ 2 ] = '\0';

		// 1. 創建 Task
		xTaskCreate( prvLabEDFTask,
					 pcTaskName,
					 configMINIMAL_STACK_SIZE,
					 pxTask,  // 傳給任務的參數
					 mainLAB_TASK_PRIORITY,
					 &xTaskHandle );
		configASSERT( xTaskHandle != NULL );
		
		// 2. 把 task 初始化
		if( xTaskHandle != NULL )
		{
			vTaskEDFSetTaskInfo( xTaskHandle,
								  pxTask->uxTaskNumber,
								  pxTask->xPeriod,
								  pxTask->xPeriod ); // 第一個 absolute deadline = period
		}
	}

	// 3. 依 deadline 選出第一個該跑的 task，
	vTaskEDFSelectInitialTask();

	// 4. 開始 multitasking
	vTaskStartScheduler(); 

	for( ;; );
}
/*-----------------------------------------------------------*/

/* LAB4: Periodic EDF workload task; each job consumes c ticks then delays to next period. */
static void prvLabEDFTask( void *pvParameters )
{
	LabEDFTaskParameters_t * const pxTask = ( LabEDFTaskParameters_t * ) pvParameters;
	TickType_t xLastWakeTime = ( TickType_t ) 0;
	TickType_t xNextDeadline = pxTask->xPeriod;

	for( ;; )
	{
		if( xTaskGetTickCount() >= mainLAB_TICK_LIMIT )
		{
			prvLabPrintOutputOnce();
			vTaskSuspend( NULL );
		}

		prvLabRunForTicks( pxTask->xComputationTime );
		vTaskEDFMarkJobComplete();

		xNextDeadline += pxTask->xPeriod;
		vTaskEDFSetDeadline( NULL, xNextDeadline );
		vTaskDelayUntil( &xLastWakeTime, pxTask->xPeriod );
	}
}
/*-----------------------------------------------------------*/

/* LAB4: Consume whole scheduler ticks as the lab's computation-time model. */
static void prvLabRunForTicks( TickType_t xTicksToRun )
{
	const TickType_t xStartTick = xTaskGetTickCount();

	while( ( xTaskGetTickCount() - xStartTick ) < xTicksToRun )
	{
		/* busy wait */
	}
}
/*-----------------------------------------------------------*/

/* LAB4: Print a task label in the exact style used by the spec output. */
static void prvLabPrintTaskName( UBaseType_t uxTaskNumber )
{
	if( uxTaskNumber == ( UBaseType_t ) 0 )
	{
		printf( "Idle" );
	}
	else
	{
		printf( "T%d", ( int ) uxTaskNumber );
	}
}
/*-----------------------------------------------------------*/

/* LAB4: Print the buffered EDF schedule once from task context, not from ISR/kernel printf. */
static void prvLabPrintOutputOnce( void )
{
	UBaseType_t uxIndex;
	const UBaseType_t uxLogCount = uxTaskEDFGetLogCount();

	if( xLabOutputPrinted == pdFALSE )
	{
		xLabOutputPrinted = pdTRUE;

		for( uxIndex = ( UBaseType_t ) 0; uxIndex < uxLogCount; uxIndex++ )
		{
			TaskEDFLogEvent_t xEvent;

			if( xTaskEDFGetLogEvent( uxIndex, &xEvent ) != pdFALSE )
			{
				printf( "%d ", ( int ) xEvent.xTime );

				if( xEvent.xIsComplete != pdFALSE )
				{
					printf( "complete " );
				}
				else
				{
					printf( "preempt  " );
				}

				prvLabPrintTaskName( xEvent.uxFromTaskNumber );
				printf( " " );
				prvLabPrintTaskName( xEvent.uxToTaskNumber );
				printf( "\n" );
			}
		}
	}
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





