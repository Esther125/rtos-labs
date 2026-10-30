# Lab3

## 1. 題目

- 幫 uC/OS-II 的 mutex lock 實作 **Ceiling Priority Protocol (CPP)**，解決 PIP (Priority Inheritance Protocol) 的兩個問題：
    - 高優先權 task 可能被多個 task 連續 block（multiple blocking）
    - 可能 deadlock
    - CPP：task 拿到 lock 的那一刻，就把自己的優先權拉到這個 lock 的 ceiling priority（比所有可能的 locker 都高一階），不用等真的發生 contention 才拉
- 規定
    - 只能改 `OSMutexPend()` / `OSMutexPost()`
    - 不能用 `OSTaskChangePrio()`（內部會呼叫 `OS_Sched()`，行為會跑掉）
- 測試資料（對應講義的兩個 scenario）：
    - Scenario 1：3 個 task 搶 2 個 mutex，驗證 CPP 解決 multiple blocking
    - Scenario 2：2 個 task 用不同順序搶鎖，驗證 CPP 避免 deadlock
- 其他假設或要求
    - 用 `OSTimeDly()` 模擬每個 task 的 arrival time
    - Output 格式跟之前的 lab 一樣，多加 lock / unlock 事件

## 2. 相關 Kernel Function

`OSStart`、`OS_Sched`、`OSTimeTick`、`OSIntExit`、`OS_TCBInit`、`OS_InitMisc` 跟 lab1 一樣，可以參考 [lab1.md](/lab1/lab1.md#2-相關-kernel-function)。這次真正會碰到、修改的是 mutex 相關的兩個函式：

- **OSMutexPend**：task 要拿 mutex 鎖時呼叫
    - mutex 可用就直接把自己設成 owner
    - mutex 已經被別人拿走，就看目前 owner 的優先權夠不夠高，不夠的話先拉高它，再把自己掛到這個 mutex 上並讓出 CPU
- **OSMutexPost**：task 放開 mutex 鎖時呼叫
    - 如果有其他 task 在等這個 mutex，就挑優先權最高的那個叫醒
    - 沒人等的話就把 mutex 標記成可用

## 3. 做法

### 3.1 Recap（跟 lab1 相同的部分）

這個 lab 延續 lab1 的 RM 版本。以下這些跟 [lab1.md](/lab1/lab1.md) 完全一樣，細節不重複寫：

- TCB 擴充欄位（compTime / period / releaseTick / deadline）跟初始化方式——這個 lab 的 task 都沒有設 `period`，所以 deadline 相關的檢查邏輯其實是關閉狀態，不會被觸發
- OSMsgRing ring buffer 機制、OS_DumpMsg 印 log 的方式
- OSIntExit / OS_Sched 紀錄 preempt / complete 事件的邏輯

### 3.2 Application Code

- refer to : [TEST.C](/lab3/SOFTWARE/uCOS-II/LAB3/BC45/SOURCE/TEST.C)

    ```mermaid
    flowchart LR
        A[main: OSInit] --> B[建立 TaskStart]
        B --> C[OSStart 開始 multitasking]
        C --> D[TaskStart: 裝 tick ISR]
        D --> E[建立 R1/R2 mutex]
        E --> F[依 SCENARIO 建立 task]
        F --> G[歸零 tick / log buffer]
        G --> H{task_done >= task_total?}
        H -- 否 --> I[OSTimeDly 等待]
        I --> H
        H -- 是 --> J[印出 log 並結束程式]
        F --> K[task: OSTimeDly 模擬 arrival]
        K --> L[RunTicks 模擬執行]
        L --> M[LockResource / UnlockResource]
        M --> L
        M --> N[task_done++，OSTaskSuspend 自己]
    ```

- **RunTicks(ticks)**：跟 lab1/lab2 的 busy-wait 邏輯一樣，設好 `compTime` 後忙等到 0，用來模擬 task 的執行時間（含 critical section 內的耗時）
- **LockResource(resource, resourceId) / UnlockResource(...)**：包在 `OSMutexPend` / `OSMutexPost` 外面的 wrapper，呼叫前後各讀一次自己的 priority，再把「lock/unlock 前後 priority 有沒有變」寫進 log（`OS_AddMsg(4/5, resourceId, oldPrio, newPrio)`）

> [!NOTE]
> 用 `#define SCENARIO` 切換要跑哪個情境（1 或 2）。

> [!NOTE]
> 解鎖時如果看到 log 印成 `Complete` 而不是 `Preempt`，是因為 `UnlockResource` 呼叫 `OSMutexPost()` 內部觸發的 `OS_Sched()` 固定記成 `Complete`（跟 lab1/lab2 一樣，`OS_Sched()` 不分青紅皂白都紀錄 complete 事件）——不代表這個 task 真的執行完畢結束，只是它剛好在解鎖的那一刻把 CPU 讓給了優先權更高的 task。

### 3.3 ucOS Kernel Code Modified（跟 lab1 不一樣的地方）

- refer to :
    - [OS_MUTEX.C](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_MUTEX.C)
    - [OS_CORE.C](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C)
    - [uCOS_II.H](/lab3/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H)

#### OS_MUTEX.C

- 新增 `OS_MutexChangeCurPrio(old_prio, new_prio)`：把 `OSTCBCur` 的 priority 從 `old_prio` 換成 `new_prio`，同步更新 ready table / priority table / `OSPrioCur`，等同手動做一次 `OSTaskChangePrio()` 的事，但不呼叫它（避免內部觸發 `OS_Sched()`）。`OSMutexPend`/`OSMutexPost` 的 boost 跟 restore 都共用這個函式：[L276-296](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_MUTEX.C#L276-L296)
- OSMutexPend：mutex 可用時的分支，CPP 的核心。新增 `old_prio` 記住 boost 前的原始優先權，如果 mutex 的 ceiling priority 比自己目前的優先權高（數字較小），呼叫上面的 helper boost 上去：[L356-371](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_MUTEX.C#L356-L371)
- OSMutexPost：解鎖時如果自己目前正用 ceiling priority 在跑，呼叫 helper 還原成原本的優先權：[L463-467](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_MUTEX.C#L463-L467)

#### OS_CORE.C

- OS_AddMsg 多一個 `resource` 參數，用來記錄 lock/unlock 事件是對哪個 mutex 做的：[L929-958](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L929-L958)
- OS_DumpMsg 多印 lock（event=4）跟 unlock（event=5）兩種格式：`R{resourceId} (Prio={from} changes to={to})`：[L987-1009](/lab3/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C#L987-L1009)

#### uCOS_II.H

- msg 結構多一個 `resource` 欄位，event 多兩個值（4=lock, 5=unlock）：[L430-436](/lab3/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H#L430-L436)
