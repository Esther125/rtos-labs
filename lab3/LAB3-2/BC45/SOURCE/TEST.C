#include "includes.h"

extern volatile sched_event_t sched_trace[];
extern volatile int sched_trace_idx;
volatile INT8U task_done = 0;
volatile INT8U trace_printed = 0;
#define TASK_STK_SIZE 512

OS_STK TaskStk[3][TASK_STK_SIZE];
OS_STK TaskStartStk[TASK_STK_SIZE];

INT32U trace_start_time = 0;

OS_EVENT *R1;
OS_EVENT *R2;

void Task1(void *pdata);
void Task2(void *pdata);

void TaskStart(void *pdata);
void PrintTrace(void);
void Run(INT16U ticks);

void main(void)
{
    OSInit();
    OSTimeSet(0);

    PC_DOSSaveReturn();
    PC_VectSet(uCOS, OSCtxSw);

    OSTaskCreate(TaskStart, 0, &TaskStartStk[TASK_STK_SIZE - 1], 0);

    OSStart();
}

void TaskStart(void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR cpu_sr;
#endif
    
    INT8U err;
    INT16S     key;
    INT32U     now;
    pdata = pdata;

    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    R1 = OSMutexCreate(1, &err);
    R2 = OSMutexCreate(2, &err);

    trace_start_time = OSTimeGet();

    now = trace_start_time;


    OSTaskCreate(Task1, (void *)1, &TaskStk[0][TASK_STK_SIZE - 1], 3);
    OSTaskCreate(Task2, (void *)2, &TaskStk[1][TASK_STK_SIZE - 1], 4);
    OSTCBPrioTbl[3]->OSTCBCompTime = 0;
    OSTCBPrioTbl[3]->OSTCBPeriod   = 100;
    OSTCBPrioTbl[3]->OSTCBDeadline = now + 100;

    OSTCBPrioTbl[4]->OSTCBCompTime = 0;
    OSTCBPrioTbl[4]->OSTCBPeriod   = 100;
    OSTCBPrioTbl[4]->OSTCBDeadline = now + 100;

    for (;;) {

        if (task_done == 2 && !trace_printed) {
            trace_printed = 1;
            PrintTrace();
            PC_DOSReturn();
        }

        if (PC_GetKey(&key) == TRUE) {
            if (key == 0x1B) {
                PC_DOSReturn();
            }
        }

        OSTimeDly(50);
    }
}

void PrintTrace(void)
{
    int i;
    for (i = 0; i < sched_trace_idx; i++) {

        char *type_str;

        if(sched_trace[i].time >= trace_start_time){
            if (sched_trace[i].type == 1) {
                type_str = "Complete";
                printf("%3lu  %-8s  %3d  %3d\n",
                    sched_trace[i].time - trace_start_time,
                    type_str,
                    sched_trace[i].from,
                    sched_trace[i].to);
            } else if (sched_trace[i].type == 4) {
                type_str = "lock";
                printf("%3lu  %-8s  R%-1d  (Prio=%d changes to=%d)\n",
                    sched_trace[i].time - trace_start_time,
                    type_str,
                    sched_trace[i].resource,
                    sched_trace[i].from,
                    sched_trace[i].to);
            } else if (sched_trace[i].type == 5) {
                type_str = "unlock";
                printf("%3lu  %-8s  R%-1d  (Prio=%d changes to=%d)\n",
                    sched_trace[i].time - trace_start_time,
                    type_str,
                    sched_trace[i].resource,
                    sched_trace[i].from,
                    sched_trace[i].to);
            }
        }
    }
}

void Run(INT16U ticks)
{
    OS_ENTER_CRITICAL();
    OSTCBCur->OSTCBCompTime = ticks;
    OS_EXIT_CRITICAL();

    while (OSTCBCur->OSTCBCompTime > 0) {
        /* busy wait */
    }
}

void Task1(void *pdata)
{
    INT8U err;
    INT16U toStart;

    toStart = 5;
    OSTimeDly(toStart);

    Run(2);

    OSMutexPend(R2, 0, &err);
    Run(3);

    OSMutexPend(R1, 0, &err);
    Run(3);
    OSMutexPost(R1);

    Run(3);
    OSMutexPost(R2);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
    
}

void Task2(void *pdata)
{
    INT8U err;
    INT16U toStart;

    toStart = 0;
    //OSTimeDly(toStart);

    Run(2);

    OSMutexPend(R1, 0, &err);
    Run(6);

    OSMutexPend(R2, 0, &err);
    Run(2); 
    OSMutexPost(R2);
    
    Run(2);
    OSMutexPost(R1);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}