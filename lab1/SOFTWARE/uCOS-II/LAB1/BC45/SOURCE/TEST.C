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

    // Install uC/OS-II's clock tick ISR
    // - 因為 tick ISR 隨時可能被硬體 timer 觸發
    // - 所以要用 critical section 保護避免 race condition
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC); // 把 time 頻率調成指定頻率
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

    // 檢查有沒有按鍵
    if (PC_GetKey(&key) == TRUE) {
        // 檢查是不是 esc
        if (key == 0x1B) { // 0x1B: ASCII code of esc
            OS_DumpMsg();
            PC_DOSReturn();
        }
    }
}

/* BASETASK */
void BaseTask(INT16U compTime, INT16U period){
    INT32U start; // task 開始時間
    INT32U end; // task 結束時間
    INT32U elapsed; // task 從開始到結束經過多久
    INT32U toDelay; // 等多久下一次 task 才會 release

    start = 0;

    // 初始化 task 的 TCB 欄位
    // LAB1 新增的 TCB 欄位：
    // - compTime: task 執行的總時間
    // - period: task 執行的週期
    // - deadline: task 截止時間
    OS_ENTER_CRITICAL(); 
    OSTCBCur->compTime = compTime;
    OSTCBCur->period = period;
    OSTCBCur->deadline = period;
    OS_EXIT_CRITICAL();

    while(1){
        while(OSTCBCur -> compTime > 0){ 
            // busy waiting
        }

        // 執行時間全部使用完畢 
        end = OSTimeGet();
        elapsed = end - start;

        // 判斷有沒有超時
        if (elapsed >= period){
            OS_DumpMsg();
            printf("time:%lu task%u exceed deadline\n", 
                (unsigned long)OSTCBCur->deadline, 
                (unsigned int)OSTCBCur -> OSTCBPrio
            );
            PC_DOSReturn(); 
        }
        
        // 沒有超時
        toDelay = period - elapsed;
        start += period; // 增加一個週期的時間即下個任務開始的時間
        
        // 重設 compTime
        OS_ENTER_CRITICAL();
        OSTCBCur -> compTime = compTime;
        OSTCBCur -> deadline += period;
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
