# Lab1

## 題目

- 模擬 periodic tasks RM (Rate Monotonic) scheduling
- 測試資料：
    - Task set 1 = { t1(1,3), t2(3,6) }
    - Task set 2 = { t1(1,3), t2(3,6), t3(4,9) }
- 其他假設或要求
    - Tasks all arrive at the same time
    - Show context switch behaviors
    - Show deadline violations if there is any

## 做法

### Application Code

- refer to : [TEST.C](/lab1/SOFTWARE/uCOS-II/LAB1/BC45/SOURCE/TEST.C)

### ucOS Source Code Modified

- refer to :
    - [uCOS_II.H](/lab1/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H)
    - [OS_CORE.C](./SOFTWARE/uCOS-II/SOURCE/OS_CORE.C)

> source code 的修改都有加上 === LAB1 MODIFIED === 註解，可以直接 Ctrl + F 搜尋 LAB1 看有改過哪些地方 ><

#### uCOS_II.H

- 擴充 TCB 欄位，新增：
    - compTime: task 裡面一個 job 執行時間多長
    - period: task 裡面每隔多久 release 一次 job
    - releaseTick: 每個 task 裡面 job 應該 release 的時間（絕對時間）
    - absDeadline: 每個 task 裡面 job 的 deadline (絕對時間)
- 新增 msg 結構，用來儲存 event log
- 新增 global variables:
    - OSMsgRing[OS_SCHED_LOG_SIZE]: ring buffer 存要 print 的訊息
    - OSMsgHead: buffer index，代表現在資料存到第幾格
    - OSMsgTail: buffer index，代表現在讀取到第幾格
    - OSMissTaskPrio: 紀錄是哪個 task 超時 （0: 沒有超時）
    - OSMissDeadlineTick: 紀錄在第幾個 tick 超時

#### OS_CORE.C

- 在 OS_InitMisc 初始化 global variables
- 在 OS_TCBInit 初始化自定義的 TCB 欄位
- 在 OS_Sched 紀錄 complete 事件

    ```c
    OS_AddMsg(1, OSPrioCur, OSPrioHighRdy); // 1: complete
    ```

- 在 OSIntExit 紀錄 preempt 事件
    ```c
    OS_AddMsg(0, OSPrioCur, OSPrioHighRdy);  // 0: Preempt
    ```
- 在 OSTimeTick
    - 遞減 compTime

        ```c
        if (OSTCBCur->compTime > 0) {
                OSTCBCur->compTime--;
            }
        ```

    - 檢查有沒有 task 超過 deadline，有的話紀錄到 global variable (OSMissTaskPrio, OSMissDeadlineTick)

- 新增 OS_AddMsg 把排程事件寫到 ring buffer
- 新增 OS_DumpMsg 從 ring buffer 印出訊息
