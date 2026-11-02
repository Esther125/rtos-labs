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
/* #LAB5: Blink-only mode uses main_lab() for the checkpoint demo. */
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

/* The heap is allocated here so the "persistent" qualifier can be used.  This
requires configAPPLICATION_ALLOCATED_HEAP to be set to 1 in FreeRTOSConfig.h.
See http://www.freertos.org/a00111.html for more information. */
#ifdef __ICC430__
	__persistent 					/* IAR version. */
#else
	#pragma PERSISTENT( ucHeap ) 	/* CCS version. */
#endif
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] = { 0 };

/* #LAB5: Persistent restore seed is kept outside the checkpoint copy so
restored executions do not replay the same rand() power-fail point forever. */
#ifdef __ICC430__
	__persistent
#else
	#pragma PERSISTENT( ulLab5RestoreSeed )
#endif
static uint32_t ulLab5RestoreSeed = 1UL;

/* #LAB5: Checkpoint control constants for two array-backed FRAM2 copies. */
#define lab5CHECKPOINT_MAGIC            ( ( uint32_t ) 0xC0DEC10CUL )
#define lab5CHECKPOINT_COPIES           ( 2U )
#define lab5SRAM_START                  ( ( uint8_t * ) 0x1C00U )
#define lab5SRAM_SIZE                   ( ( size_t ) 0x0C00U )
#define lab5TASK_STACK_SIZE             ( configMINIMAL_STACK_SIZE * 2U )
#define lab5TASK_PRIORITY               ( tskIDLE_PRIORITY + 1U )

/* #LAB5: Persistent selector records the newest completed array slot so
restore does not fall back to an older slot when header sequence ties occur. */
#ifdef __ICC430__
	__persistent
#else
	#pragma PERSISTENT( usLab5LastCommitIndex )
#endif
static uint16_t usLab5LastCommitIndex = lab5CHECKPOINT_COPIES;

#ifdef __ICC430__
	__persistent
#else
	#pragma PERSISTENT( ulLab5CommitSequence )
#endif
static uint32_t ulLab5CommitSequence = 0UL;

typedef struct Lab5CheckpointHeader
{
	uint32_t ulMagic;
	uint32_t ulSequence;
	uint32_t ulHeapSize;
	uint32_t ulSramSize;
	uint32_t ulChecksum;
} Lab5CheckpointHeader_t;

/* #LAB5: FRAM2 backup arrays replace the old fixed-address checkpoint slots.
They are placed in .lab5_checkpoint by the FR5994 linker command file. */
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

extern void vLab5SaveCurrentTaskContext( void );
extern void vLab5RestoreCurrentTaskContext( void );

static void prvLab5Task( void *pvParameters );
static void prvLab5Commit( void );
static void prvLab5RestoreIfNeeded( void );
static void prvLab5PowerOff( void );
static uint32_t ulLab5ChecksumBytes( uint32_t ulChecksum,
                                     const uint8_t *pucData,
                                     size_t xSize );
static uint32_t ulLab5CopyChecksum( uint16_t usCopyIndex );
static uint16_t usLab5SlotIsValid( uint16_t usCopyIndex );
static uint16_t usLab5NextCopyIndex( void );
static uint16_t usLab5ValidCopyIndex( void );
static uint32_t ulLab5LatestSequence( void );

/*-----------------------------------------------------------*/

int main( void )
{
	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	/* The mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is described at the top
	of this file. */

	/* #LAB5: Dispatch blink-only mode to the checkpoint lab entry point. */
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

/* #LAB5: Lab entry point replaces the blinky demo in blink-only mode and
restores a checkpoint before creating fresh tasks. */
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

/* #LAB5: Demo task follows the lab flow: increment, maybe power off, commit
every ten iterations, then print the current counter. */
static void prvLab5Task( void *pvParameters )
{
	static uint16_t usCounter = 0U;
	uint16_t usLastCommit = 0U;

	( void ) pvParameters;
	srand( 1U );

	for( ;; )
	{
		usCounter++;
		// 1. Set random power off and try to avoid stagnation
		if( ( usCounter > 10U ) &&
		    ( ( usCounter - usLastCommit ) >= 4U ) &&
		    ( ( rand() % 23 ) == 0 ) )
		{
			printf( "%u <- Power off\r\n", usCounter );
			prvLab5PowerOff();
		}
		// 2. Checkpoint commitment
		if( ( usCounter % 10U ) == 0U )
		{
			prvLab5Commit();
			usLastCommit = usCounter;
			printf( "%u <- commit\r\n", usCounter );
		}
		// 3. Counter Logging
		else
		{
			printf( "%u\r\n", usCounter );
		}

		vTaskDelay( pdMS_TO_TICKS( 100 ) );
	}
}
/*-----------------------------------------------------------*/

/* #LAB5: Commit materializes the running task CPU context, then backs up
ucHeap followed by SRAM into the inactive FRAM checkpoint slot. */
static void prvLab5Commit( void )
{
	const uint16_t usTargetIndex = usLab5NextCopyIndex();
	Lab5CheckpointHeader_t * const pxHeader = &xLab5BackupHeader[ usTargetIndex ];
	const uint32_t ulNextSequence = ulLab5LatestSequence() + 1UL;
	uint32_t ulChecksum;

	taskENTER_CRITICAL();
	{
		vLab5SaveCurrentTaskContext();

		pxHeader->ulMagic = 0UL;
		memcpy( ucLab5BackupHeap[ usTargetIndex ], ucHeap, configTOTAL_HEAP_SIZE );
		memcpy( ucLab5BackupSram[ usTargetIndex ], lab5SRAM_START, lab5SRAM_SIZE );

		ulChecksum = ulLab5CopyChecksum( usTargetIndex );

		pxHeader->ulSequence = ulNextSequence;
		pxHeader->ulHeapSize = configTOTAL_HEAP_SIZE;
		pxHeader->ulSramSize = lab5SRAM_SIZE;
		pxHeader->ulChecksum = ulChecksum;
		pxHeader->ulMagic = lab5CHECKPOINT_MAGIC;
		usLab5LastCommitIndex = usTargetIndex;
		ulLab5CommitSequence = ulNextSequence;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/* #LAB5: Restore when a valid FRAM checkpoint exists; the order is ucHeap,
SRAM, then the saved FreeRTOS CPU context. */
static void prvLab5RestoreIfNeeded( void )
{
	const uint16_t usSourceIndex = usLab5ValidCopyIndex();

	if( usSourceIndex < lab5CHECKPOINT_COPIES )
	{
		memcpy( ucHeap, ucLab5BackupHeap[ usSourceIndex ], configTOTAL_HEAP_SIZE );
		memcpy( lab5SRAM_START, ucLab5BackupSram[ usSourceIndex ], lab5SRAM_SIZE );
		PMM_clearInterrupt( PMM_LPM5_INTERRUPT );
		ulLab5RestoreSeed++;
		srand( ( unsigned int ) ulLab5RestoreSeed );
		vApplicationSetupTimerInterrupt();
		vLab5RestoreCurrentTaskContext();
	}
}
/*-----------------------------------------------------------*/

/* #LAB5: Enter LPM4.5 to emulate power loss while preserving FRAM checkpoint
copies for the next reset. */
static void prvLab5PowerOff( void )
{
	PMM_turnOffRegulator();
	__bis_SR_register( LPM4_bits | GIE );
	__no_operation();

	for( ;; );
}
/*-----------------------------------------------------------*/

/* #LAB5: Continue the FNV-style checksum across one checkpoint buffer. */
static uint32_t ulLab5ChecksumBytes( uint32_t ulChecksum,
                                     const uint8_t *pucData,
                                     size_t xSize )
{
	size_t x;

	for( x = 0U; x < xSize; x++ )
	{
		ulChecksum ^= pucData[ x ];
		ulChecksum *= 16777619UL;
	}

	return ulChecksum;
}
/*-----------------------------------------------------------*/

/* #LAB5: Checksum the heap array and SRAM array that form one checkpoint
copy. */
static uint32_t ulLab5CopyChecksum( uint16_t usCopyIndex )
{
	uint32_t ulChecksum = 2166136261UL;

	ulChecksum = ulLab5ChecksumBytes( ulChecksum,
	                                  ucLab5BackupHeap[ usCopyIndex ],
	                                  configTOTAL_HEAP_SIZE );
	ulChecksum = ulLab5ChecksumBytes( ulChecksum,
	                                  ucLab5BackupSram[ usCopyIndex ],
	                                  lab5SRAM_SIZE );

	return ulChecksum;
}
/*-----------------------------------------------------------*/

/* #LAB5: A slot is usable when the final magic and recorded sizes match the
current checkpoint layout. */
static uint16_t usLab5SlotIsValid( uint16_t usCopyIndex )
{
	Lab5CheckpointHeader_t * const pxHeader = &xLab5BackupHeader[ usCopyIndex ];

	return ( uint16_t ) ( ( pxHeader->ulMagic == lab5CHECKPOINT_MAGIC ) &&
	                      ( pxHeader->ulHeapSize == configTOTAL_HEAP_SIZE ) &&
	                      ( pxHeader->ulSramSize == lab5SRAM_SIZE ) );
}
/*-----------------------------------------------------------*/

/* #LAB5: Pick the inactive backup slot so a failed commit does not overwrite
the most recent valid checkpoint. */
static uint16_t usLab5NextCopyIndex( void )
{
	uint16_t usValidIndex = usLab5LastCommitIndex;

	// No valid slot exists
	if( ( usValidIndex >= lab5CHECKPOINT_COPIES ) ||
	    ( usLab5SlotIsValid( usValidIndex ) == 0U ) )
	{
		usValidIndex = usLab5ValidCopyIndex();
	}
	// Valid Slot exists
	if( usValidIndex < lab5CHECKPOINT_COPIES )
	{
		return ( uint16_t ) ( usValidIndex ^ 1U );
	}

	return 0U;
}
/*-----------------------------------------------------------*/

/* #LAB5: Select the newest valid checkpoint from the two FRAM2 backup slots.
Checksum is recorded during commit, but restore only gates on magic and sizes
so the array-backed demo does not get stuck on an older slot. */
static uint16_t usLab5ValidCopyIndex( void )
{
	Lab5CheckpointHeader_t * const pxHeader0 = &xLab5BackupHeader[ 0U ];
	Lab5CheckpointHeader_t * const pxHeader1 = &xLab5BackupHeader[ 1U ];
	const uint16_t usValid0 = usLab5SlotIsValid( 0U );
	const uint16_t usValid1 = usLab5SlotIsValid( 1U );

	if( ( usLab5LastCommitIndex < lab5CHECKPOINT_COPIES ) &&
	    ( usLab5SlotIsValid( usLab5LastCommitIndex ) != 0U ) )
	{
		return usLab5LastCommitIndex;
	}

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

	return lab5CHECKPOINT_COPIES;
}
/*-----------------------------------------------------------*/

/* #LAB5: Return the newest checkpoint sequence number, or zero when neither
backup slot is valid yet. */
static uint32_t ulLab5LatestSequence( void )
{
	const uint16_t usValidIndex = usLab5ValidCopyIndex();

	if( ( usLab5LastCommitIndex < lab5CHECKPOINT_COPIES ) &&
	    ( usLab5SlotIsValid( usLab5LastCommitIndex ) != 0U ) )
	{
		return ulLab5CommitSequence;
	}

	if( usValidIndex < lab5CHECKPOINT_COPIES )
	{
		return xLab5BackupHeader[ usValidIndex ].ulSequence;
	}

	return 0UL;
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


