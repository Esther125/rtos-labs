#include "includes.h"

typedef struct {
    INT32U time;
    INT8U  type;
    INT8U  from;
    INT8U  to;
} sched_event_t;

extern volatile sched_event_t sched_trace[];
extern volatile int sched_trace_idx;
volatile INT8U trace_done = 0;
volatile INT8U trace_printed = 0;
#define TASK_STK_SIZE 512

OS_STK TaskStk[3][TASK_STK_SIZE];
OS_STK TaskStartStk[TASK_STK_SIZE];
OS_STK PrintTaskStk[TASK_STK_SIZE];

volatile int total_runs = 0;
#define MAX_RUNS 30

INT32U trace_start_time = 0;
INT8U err1, err2, err3, err4;

void Task(void *pdata);
void TaskStart(void *pdata);
void PrintTrace(void);
void PrintTask(void *pdata);

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

    INT16S     key;
    INT32U     now;
    pdata = pdata;

    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    //OSStatInit();

    trace_start_time = OSTimeGet();
    now = trace_start_time;

    OSSchedLock();

    err1 = OSTaskCreate(Task, (void *)1, &TaskStk[0][TASK_STK_SIZE - 1], 1);
    err2 = OSTaskCreate(Task, (void *)2, &TaskStk[1][TASK_STK_SIZE - 1], 2);
    err3 = OSTaskCreate(Task, (void *)3, &TaskStk[2][TASK_STK_SIZE - 1], 3);
    err4 = OSTaskCreate(PrintTask, (void *)4, &PrintTaskStk[TASK_STK_SIZE - 1], 4);

    printf("create err1=%d err2=%d err3=%d err4=%d\n", err1, err2, err3, err4);

    OSTCBPrioTbl[1]->OSTCBCompTime = 1;
    OSTCBPrioTbl[1]->OSTCBPeriod   = 4;
    OSTCBPrioTbl[1]->OSTCBDeadline = now + 4;

    OSTCBPrioTbl[2]->OSTCBCompTime = 2;
    OSTCBPrioTbl[2]->OSTCBPeriod   = 5;
    OSTCBPrioTbl[2]->OSTCBDeadline = now + 5;

    OSTCBPrioTbl[3]->OSTCBCompTime = 2;
    OSTCBPrioTbl[3]->OSTCBPeriod   = 10;
    OSTCBPrioTbl[3]->OSTCBDeadline = now + 10;

    OSTCBPrioTbl[4]->OSTCBCompTime = 10;
    OSTCBPrioTbl[4]->OSTCBPeriod   = 100;
    OSTCBPrioTbl[4]->OSTCBDeadline = now + 100;

    OSSchedUnlock();

    OSTaskSuspend(OS_PRIO_SELF);
}

void PrintTrace(void)
{
    int i;
    for (i = 0; i < sched_trace_idx; i++) {

        char *type_str;

        if(sched_trace[i].time >= trace_start_time){
            if (sched_trace[i].type == 3) {
                printf("time : %lu task %d exceed deadline\n",
                sched_trace[i].time - trace_start_time,
                sched_trace[i].from);
            } else {
                if (sched_trace[i].type == 1)
                    type_str = "Complete";
                else
                    type_str = "Preempt";

                printf("%3lu  %-8s  %3d  %3d\n",
                    sched_trace[i].time - trace_start_time,
                    type_str,
                    sched_trace[i].from,
                    sched_trace[i].to);
            }
        }
    }
}

void Task(void *pdata)
{
    INT8U  id = (INT8U)pdata;
    INT16U C;
    INT16U P;
    INT32U start;
    INT32U end;
    INT16U toDelay;

    if (id == 1) {
        C = 1;
        P = 4;
    } else if (id == 2) {
        C = 2;
        P = 5;
    } else {
        C = 2;
        P = 10;
    }

    //start = OSTimeGet();

    for (;;) {
        start = OSTCBCur->OSTCBDeadline - OSTCBCur->OSTCBPeriod;
        while (OSTCBCur->OSTCBCompTime > 0) {
            /* busy wait: CPU time is charged in OSTimeTick() */
        }

        end = OSTimeGet();

        if ((end - start) >= P) {
            toDelay = 0;
        } else {
            toDelay = (INT16U)(P - (end - start));
        }
        /*printf("[DONE] T%d | start=%lu end=%lu exec=%lu | toDelay=%u | compute_time=%u | deadline=%lu | now=%lu\n",
       id,
       start,
       end,
       end - start,
       toDelay,
       C,
       OSTCBCur->OSTCBDeadline,
       OSTimeGet());*/

       start += P;

        OS_ENTER_CRITICAL();
        OSTCBCur->OSTCBCompTime = C;
        OSTCBCur->OSTCBDeadline += P;
        OS_EXIT_CRITICAL();

        OS_ENTER_CRITICAL();
        total_runs++;
        if (total_runs >= MAX_RUNS) {
            trace_done = 1;
        }
        OS_EXIT_CRITICAL();

        if (trace_done) {
            OSTaskSuspend(OS_PRIO_SELF);
        }

        if (toDelay > 0) {
            OSTimeDly(toDelay);
        }
    }
}

void PrintTask(void *pdata)
{
    INT16S key;
    pdata = pdata;

    //printf("PrintTask started\n");

    PrintTrace();
    PC_DOSReturn();
}