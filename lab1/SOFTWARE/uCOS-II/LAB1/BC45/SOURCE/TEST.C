/* ==================== LAB1 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks

/* VARIABLES */
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks
OS_STK TaskStartStk[TASK_STK_SIZE];

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); // start up task
void BaseTask(INT8U taskId, INT16U compTime, INT16U period); 
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
    // Create start up task
    OSTaskCreate(TaskStart, (void*) 0,
        (void*) &TaskStartStk[TASK_STK_SIZE-1],0
    );
    // Start multitasking
    OSStart(); 
}

/* START UP TASK */
void TaskStart(void *pdata){
    pdata = pdata;

    // Install uC/OS-II's clock tick ISR
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC); // 把 time 頻率調成指定頻率
    OS_EXIT_CRITICAL();

    // Create application tasks 
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 1);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 2);
    OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 3);
    
    for(;;){
        CheckEscAndExit();
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
}

void CheckEscAndExit(void){
    INT16S key;

    // 檢查有沒有按鍵
    if (PC_GetKey(&key) == TRUE) {
        // 檢查是不是 esc
        if (key == 0x1B) { // 0x1B: ASCII code of esc
            PC_DOSReturn();
        }
    }
}

/* BASETASK */
void BaseTask(INT8U taskId, INT16U compTime, INT16U period){
    INT32U start; // task 開始時間
    INT32U end; // task 結束時間
    INT32U elapsed; // end - start
    INT32U toDelay; // 等多久下一次 task 才會 release

    // 初始化 task 的 TCB 欄位
    OS_ENTER_CRITICAL(); 
    OSTCBCur->compTime = compTime;
    OSTCBCur->period = period;
    OS_EXIT_CRITICAL();

    while(1){
        CheckEscAndExit();

        start = OSTimeGet();

        while(OSTCBCur -> compTime > 0){ 
            // busy waiting
        }

        // 執行時間全部使用完畢 
        end = OSTimeGet();
        elapsed = end - start;

        // 判斷有沒有超時
        if (elapsed > period){
            printf("time:%lu task%lu exceed deadline\n", 
                (unsigned long)end, 
                (unsigned long)taskId
            );
            PC_DOSReturn(); 
        }
        
        // 沒有超時
        OS_DumpMsg();
        toDelay = period - elapsed;
        start += period; // 增加一個週期的時間即下個任務開始的時間
        
        // 重設 compTime
        OS_ENTER_CRITICAL();
        OSTCBCur -> compTime = compTime;
        OS_EXIT_CRITICAL();

        if (toDelay > 0){
            OSTimeDly(toDelay);
        }
    }
   
}

/* APPLICATION TASKS */
void Task1(void *pdata){
    pdata = pdata;
    BaseTask(1, 1, 3);
}

void Task2(void *pdata){
    pdata = pdata;
    BaseTask(2, 3, 6);
}

void Task3(void *pdata){
    pdata = pdata;
    BaseTask(3, 4, 9);
}
