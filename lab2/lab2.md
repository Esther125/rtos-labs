# Lab2

## 1. 題目

- 模擬 periodic tasks EDF (Earliest Deadline First) scheduling
    - EDF: 動態依照每個 job 目前剩下的 deadline 遠近決定優先權，deadline 越近越優先執行的 dynamic-priority 排程演算法
- 測試資料：
    - Task set 1 = { t1(1,3), t2(3,5) }
    - Task set 2 = { t1(1,4), t2(2,5), t3(2,10) }
- 其他假設或要求
    - Tasks all arrive at the same time
    - Show context switch behaviors
    - Show deadline violations if there is any

## 2. 相關 Kernel Function

tick 中斷流程跟各函式說明跟 lab1 一樣，可以直接參考 [lab1.md](/lab1/lab1.md#2-相關-kernel-function)，這裡只額外列 lab1 沒提到的：

- **OSStart**：開始多工時呼叫，挑第一個要跑的 task

## 3. 做法

### 3.1 Recap（跟 lab1 相同的部分）

這個 lab 延續 lab1 的架構，只是把排程演算法從 RM 換成 EDF。以下這些跟 [lab1.md](/lab1/lab1.md) 完全一樣，細節不重複寫：

- TCB 擴充欄位：`compTime`（volatile）/ `period` / `releaseTick` / `deadline`，欄位名稱都跟 lab1 一樣
    - `deadline` 存的是絕對時間（不是相對 period 的時長）
- TaskStart / BaseTask 的整體流程（建立 task → 初始化 TCB → busy-wait 消耗 compTime → 準備下一個 job）
- msg 結構 + OSMsgRing/OSMsgHead/OSMsgTail ring buffer 機制
- OSMissTaskPrio / OSMissDeadlineTick 的 deadline miss 紀錄機制
- OS_InitMisc 初始化 global variables
- OS_AddMsg / OS_DumpMsg 的實作
- OSTimeTick 遞減 compTime、檢查 deadline 的邏輯（多了幾個 null check，整體跟 lab1 一樣）
- 一樣用 === LAB2 MODIFIED === 註解，可以 Ctrl + F 搜尋 LAB2 看改過哪些地方

### 3.2 Application Code

- refer to : [TEST.C](/lab2/SOFTWARE/uCOS-II/LAB2/BC45/SOURCE/TEST.C)

    大致上的流程跟 lab1 一樣，只是排程演算法從 RM 換成 EDF：

    ```mermaid
    flowchart LR
        A[main: OSInit] --> B[建立 TaskStart]
        B --> C[OSStart 開始多工]
        C --> D[TaskStart: 裝 tick ISR]
        D --> E[建立 application tasks]
        E --> F[OSTimeSet 歸零時間]
        F --> G[TaskStart 閒置迴圈]
        E --> H[task 執行 BaseTask]
        H --> I[初始化自己的 TCB 欄位]
        I --> J{有 deadline miss?}
        J -- 有 --> K[印出 log 並結束程式]
        J -- 沒有 --> L[busy-wait 消耗 compTime]
        L --> M[印出排程 log]
        M --> N[更新下一個 job 的 release / deadline]
        N --> O[delay 到下次 release]
        O --> J
    ```

> [!NOTE]
> 目前用註解切換測資：預設是 Task set 1（Task1 + Task2），Task set 2（+Task3）被註解掉了，要換測資去 [TEST.C](/lab2/SOFTWARE/uCOS-II/LAB2/BC45/SOURCE/TEST.C) 改註解。

### 3.3 ucOS Kernel Code Modified（只列跟 lab1 不一樣的地方）

- refer to :
    - [uCOS_II.H](/lab2/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H)
    - [OS_CORE.C](/lab2/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C)

#### OS_CORE.C

- 新增 OS_EDF：掃過 OSTCBList，在系統還沒發生過 deadline miss 的前提下，從「period > 0 且 compTime > 0 且目前 ready」的 task 中挑 deadline 最小的那個，挑不到就回傳 NULL：[L1260-1279](/lab2/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L1260-L1279)
- OSStart 改成先呼叫 OS_EDF 選第一個要跑的 task，挑不到（代表沒有 periodic task ready）才 fallback 回原本的 priority table：[L324-337](/lab2/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L324-L337)
- OS_Sched 同樣改成先呼叫 OS_EDF，挑不到才 fallback 回 priority table，排程成功才紀錄 complete 事件：[L942-963](/lab2/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L942-L963)
- OSIntExit 同樣改成呼叫 OS_EDF 決定下一個要跑的 task：[L190-217](/lab2/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L190-L217)

> [!IMPORTANT]
> `OSIntExit` 多加了一個 guard：如果目前這個 task 的 `compTime` 剛好被 tick ISR 扣到 0、但還沒執行到 `BaseTask()` 裡面的 `OSTimeDly()`，就先不重新排程直接 return。<br>
> 否則這個 task 還沒進入 delay 狀態，`OSIntExit` 就會提早觸發排程，導致同一個 tick 內重複記錄一次 Complete 事件（對應修正 commit：avoid same-tick duplicate Complete logs by deferring ISR reschedule when current periodic job just finished）。
