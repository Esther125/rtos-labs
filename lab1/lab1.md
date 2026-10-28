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

    大致上的流程可以參考以下流程圖：

    ```mermaid
    flowchart LR
        A[main: OSInit] --> B[建立 TaskStart]
        B --> C[OSStart 開始多工]
        C --> D[TaskStart: 裝 tick ISR]
        D --> E[建立 Task1/2/3]
        E --> F[OSTimeSet 歸零時間]
        F --> G[TaskStart 閒置迴圈]
        E --> H[Task1/2/3 執行 BaseTask]
        H --> I[初始化自己的 TCB 欄位]
        I --> J{有 deadline miss?}
        J -- 有 --> K[印出 log 並結束程式]
        J -- 沒有 --> L[busy-wait 消耗 compTime]
        L --> M[印出排程 log]
        M --> N[更新下一個 job 的 release / deadline]
        N --> O[delay 到下次 release]
        O --> J
    ```

### ucOS Kernel Code Modified

- refer to :
    - [uCOS_II.H](/lab1/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H)
    - [OS_CORE.C](./SOFTWARE/uCOS-II/SOURCE/OS_CORE.C)

> [!NOTE]
> source code 的修改都有加上 === LAB1 MODIFIED === 註解，可以直接 Ctrl + F 搜尋 LAB1 看有改過哪些地方

#### uCOS_II.H

- 擴充 TCB 欄位，新增：[L411-418](/lab1/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H#L411-L418)
    - compTime: task 裡面一個 job 執行時間多長
    - period: task 裡面每隔多久 release 一次 job
    - releaseTick: 每個 task 裡面 job 應該 release 的時間（絕對時間）
    - absDeadline: 每個 task 裡面 job 的 deadline (絕對時間)
- 新增 msg 結構，用來儲存 event log：[L427-435](/lab1/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H#L427-L435)
- 新增 global variables：[L437-442](/lab1/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H#L437-L442)
    - OSMsgRing[OS_SCHED_LOG_SIZE]: ring buffer 存要 print 的訊息
    - OSMsgHead: buffer index，代表現在資料存到第幾格
    - OSMsgTail: buffer index，代表現在讀取到第幾格
    - OSMissTaskPrio: 紀錄是哪個 task 超時 （0: 沒有超時）
    - OSMissDeadlineTick: 紀錄在第幾個 tick 超時

> [!WARNING]
> TCB 擴充的欄位中，`compTime` 要宣告成 `volatile`
> 因為它會在 tick ISR（`OSTimeTick()`）裡被遞減，而 `BaseTask()` 又用 `while (OSTCBCur->compTime > 0) {}` busy-wait 讀取它。
> 如果不加 `volatile`，compiler 可能會把這個值快取在暫存器裡做優化，讀不到 ISR 更新後的新值，導致無窮迴圈。

#### OS_CORE.C

- 在 OS_InitMisc 初始化 global variables：[L699-704](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L699-L704)
- 在 OS_TCBInit 初始化自定義的 TCB 欄位：[L1155-1160](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L1155-L1160)
- 在 OS_Sched 紀錄 complete 事件：[L919-921](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L919-L921)
    ```c
    OS_AddMsg(1, OSPrioCur, OSPrioHighRdy); // 1: complete
    ```
- 在 OSIntExit 紀錄 preempt 事件：[L190-192](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L190-L192)
    ```c
    OS_AddMsg(0, OSPrioCur, OSPrioHighRdy);  // 0: Preempt
    ```
- 在 OSTimeTick
    - 遞減 compTime：[L400-405](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L400-L405)
        ```c
        if (OSTCBCur->compTime > 0) {
                OSTCBCur->compTime--;
            }
        ```
    - 檢查有沒有 task 超過 deadline，有的話紀錄到 global variable (OSMissTaskPrio, OSMissDeadlineTick)：[L406-422](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L406-L422)
- 新增 OS_AddMsg 把排程事件寫到 ring buffer：[L929-957](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L929-L957)
- 新增 OS_DumpMsg 從 ring buffer 印出訊息：[L959-990](/lab1/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L959-L990)
