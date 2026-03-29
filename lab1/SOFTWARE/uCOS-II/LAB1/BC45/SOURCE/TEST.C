/* ==================== LAB1 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks

/* VARIABLES */
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks
OS_STK TaskStartStk[TASK_STK_SIZE];
char TaskData[N_TASKS]; // Parameters to pass to each task

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); 
void BaseTask(INT16U compTime, INT16U period); 
void Task1(void *pdata);
void Task2(void *pdata);
void Task3(void *pdata);

/* MAIN */
void  main (void){
    // Initialize uC/OS-II
    OSInit();
    // Save environment to return to DOS
    PC_DOSSaveReturn();
    // Install uC/OS-II's context switch vector 
    PC_VectSet(uCOS, OSCtxSw);
    
    // Create start up task
    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], 10);
    
    // Start multitasking
    OSStart(); 
}

/* STARTUP TASK */
void  TaskStart (void *pdata){
// Allocate storage for CPU status register
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif
    INT16S key;
    pdata = pdata;

    OS_ENTER_CRITICAL();
    // Install uC/OS-II's clock tick ISR
    PC_VectSet(0x08, OSTickISR);
    // Reprogram tick rate
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    // Create application tasks
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 1);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 2);
    OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 3);

    for (;;) {
        // ESC 退出設定
        if(PC_GetKey(&key) == TRUE){ // See if key has been pressed
            if(key == 0x1B){ // See if it's the ESC key
                PC_DOSReturn();
            }
        }

        // Startup task 永遠不應該 return
        // 以 delay 方式把 CPU 讓給其他 ready tasks
        OSTimeDly(OS_TICKS_PER_SEC); // Wait one second
    }
}

/* BASETASK */
void BaseTask(INT16U compTime, INT16U period){
    INT32U start;
    INT32U end;
    INT16U toDelay; // 等多久下一次 task 才會 release

    start = 0;
    OS_ENTER_CRITICAL(); 
    OSTCBCur->period = period;
    OSTCBCur->compTimeCtr = compTime;
    OS_EXIT_CRITICAL();

    while(1){
        while(OSTCBCur->compTimeCtr > 0){ // c ticks
            // do nothing
        }
        // c ticks 都用完
        end = OSTimeGet(); 
        toDelay = (OSTCBCur->period - (end - start));
        start = start + OSTCBCur->period; // next start time

        OS_ENTER_CRITICAL(); 
        OSTCBCur -> compTimeCtr = compTime; // reset the counter
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
