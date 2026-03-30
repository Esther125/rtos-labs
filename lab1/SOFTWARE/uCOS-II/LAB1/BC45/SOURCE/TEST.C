/* ==================== LAB1 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks

/* VARIABLES */
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks
OS_STK TaskStartStk[TASK_STK_SIZE];

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); 
void BaseTask(INT16U compTime, INT16U period); 
void Task1(void *pdata);
void Task2(void *pdata);
void Task3(void *pdata);
void CheckEscAndExit(void);

/* MAIN */
void  main (void){
    // Initialize uC/OS-II
    OSInit();
    // Save environment to return to DOS
    PC_DOSSaveReturn();
    // Install uC/OS-II's context switch vector 
    PC_VectSet(uCOS, OSCtxSw);

    OS_ENTER_CRITICAL();
    // Install uC/OS-II's clock tick ISR
    PC_VectSet(0x08, OSTickISR);
    // Reprogram tick rate
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    // Create application tasks directly before OSStart
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 1);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 2);
    OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 3);
    
    // Start multitasking
    OSStart(); 
}

void CheckEscAndExit(void){
    INT16S key;

    if (PC_GetKey(&key) == TRUE) {
        if (key == 0x1B) {
            OS_DumpMsg();
            PC_DOSReturn();
        }
    }
}

/* BASETASK */
void BaseTask(INT16U compTime, INT16U period){
    INT32U start;
    INT32U end;
    INT32U elapsed;
    INT32U toDelay; // 等多久下一次 task 才會 release
    INT16U lastCtr;
    INT8U priority;

    start = 0;
    OS_ENTER_CRITICAL(); 
    OSTCBCur->period = period;
    OSTCBCur->compTimeCtr = compTime;
    OSTCBCur->deadlineTick = period;
    priority = OSTCBCur->OSTCBPrio;
    OS_EXIT_CRITICAL();

    while(1){
        lastCtr = OSTCBCur->compTimeCtr;
        while(OSTCBCur->compTimeCtr > 0){ // c ticks
            if (OSMissDeadlineFlag) {
                while (OS_DumpMsgOne() == TRUE) {
                }
                printf("time:%lu task%u exceed deadline\n",
                       (unsigned long)OSMissDeadlineTick,
                       (unsigned int)OSMissTaskPrio);
                PC_DOSReturn();
            }
            if (OSTCBCur->compTimeCtr != lastCtr) {
                lastCtr = OSTCBCur->compTimeCtr;
                CheckEscAndExit();
            }
        }
        // c ticks 都用完
        end = OSTimeGet();
        elapsed = end - start;
        if (elapsed >= OSTCBCur->period) {
            toDelay = 0;
        } else {
            toDelay = OSTCBCur->period - elapsed;
        }
        start = start + OSTCBCur->period; // next start time

        OS_ENTER_CRITICAL(); 
        OSTCBCur -> compTimeCtr = compTime; // reset the counter
        OSTCBCur -> deadlineTick = OSTCBCur->deadlineTick + OSTCBCur->period;
        OS_EXIT_CRITICAL();

        OSTimeDly(toDelay);
    }
   
}

/* APPLICATION TASKS */
void Task1(void *pdata){
    pdata = pdata;
    BaseTask(1, 3);
}

void Task2(void *pdata){
    pdata = pdata;
    BaseTask(3, 6);
}

void Task3(void *pdata){
    pdata = pdata;
    BaseTask(4, 9);
}
