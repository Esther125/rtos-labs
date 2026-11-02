# Lab5

## 1. 題目

- 幫 FreeRTOS 實作 **checkpoint / restore**
- 開發板: MSP430FR5994（FRAM MCU）
- 大致流程（from 老師的投影片）：

    ```c
    main() {
        prvSetupHardware();
        restore(); // 如果有 power fail 過，restore 上次的 checkpoint
        // 建 test task、啟動 scheduler
    }

    test_task() {
        while (i++) {
            if (random_condition()) power_off();
            if (i % 10 == 0) commit();
            print(i);
        }
    }
    ```

- Commit 跟 restore 都規定要照順序做：
    - Commit：`ucHeap` → SRAM → CPU registers
    - Restore：`ucHeap` → SRAM → CPU registers
- 其他要求
    - 用 `rand()` 觸發 power off，但 power fail 的間隔不能一直比 commit 間隔短，否則會 stagnation（永遠卡在同一個點重複失敗）
    - 真的要備份/還原完整的 SRAM（`.bss`/`.data`）、`ucHeap`、CPU registers——只備份一個計數器 `i` 是不合格的，會被扣分
    - LPM4.5 只是方便測試用的模擬，demo 時還要能承受真的拔電源
        - LPM4.5（Low Power Mode 4.5）：MSP430 最深的低功耗模式，會把 CPU、SRAM、I/O 的電源整個關掉——SRAM 內容會直接消失，只有 FRAM（非揮發性）裡的資料還留著，所以才需要把 SRAM、`ucHeap`、CPU registers 都先 backup 進 FRAM，不然一進 LPM4.5 這些東西就沒了

### 1.1 ucHeap

1. 它是 FRAM 裡**唯一**真正會被程式持續寫入的 RW 區域（`.text`/`.const`/`.cinit` 本質上是唯讀、或只在開機時寫一次）。FRAM 非揮發，斷電不會消失，但 SRAM、CPU 暫存器會（這個 project 的 task stack 其實配置在 `ucHeap` 裡，不是獨立的 SRAM 區域）——`ucHeap` 的內容又會隨程式執行不斷變動，**不能只靠它天生在 FRAM 裡就當作是一個 checkpoint**，必須在固定時間點（commit）額外拷貝一份快照，搭配同時存下來的 SRAM、CPU 暫存器，才能湊成一套完整、一致、能接續執行的狀態
2. `ucHeap` 裡面裝的是 FreeRTOS 用 `pvPortMalloc()` 動態配置出來的東西，包括**每個 task 的 TCB 跟 stack**——這些都是從這個 heap 裡切出來的，不是另外獨立宣告的全域變數
3. 正因為 TCB、stack 是 runtime 動態配置出來的，不是編譯期就有名字的符號，**`.map` 檔裡只會看到 `ucHeap` 這整塊陣列本身，看不到裡面個別 task 的 TCB 或 stack 被配置在哪個位置**——那是配置器在執行期才決定的，linker 不知道

## 2. 相關 Kernel/Port Function

這個 lab 沒有改 FreeRTOS 本身的排程邏輯，是在 port 層（`portext.asm`）外加兩個新函式，建立在既有的 context-switch 機制上：

- **`portSAVE_CONTEXT` / `portRESTORE_CONTEXT`**：port 層的組語 macro，原本是給一般 context switch 用的——把目前所有暫存器 push 到目前 task 自己的 stack 上（save），或是從 stack pop 回暫存器（restore）
- **`pxCurrentTCB`**：FreeRTOS 內部指向「目前執行的 task」的 TCB，`portSAVE_CONTEXT` 會把算出來的 stack pointer 存回這裡
- **`vTaskStartScheduler()`**：啟動 scheduler，讓 FreeRTOS 開始做 context switch、tick interrupt 開始跑

## 3. 做法

### 3.1 Application Code

- refer to : [main.c](/lab5/main.c)

    ```mermaid
    flowchart LR
        A[main] --> B[main_lab]
        B --> C[prvLab5RestoreIfNeeded]
        C --> D[xTaskCreate prvLab5Task]
        D --> E[vTaskStartScheduler]
        E --> F[prvLab5Task 迴圈]
        F --> G{要 power off?}
        G -- 是 --> H[prvLab5PowerOff 進 LPM4.5]
        G -- 否 --> I{counter % 10 == 0?}
        I -- 是 --> J[prvLab5Commit]
        J --> K[printf 印出 counter]
        I -- 否 --> K
        K --> L[vTaskDelay]
        L --> F
    ```

- **`main_lab()`**：先呼叫 `prvLab5RestoreIfNeeded()`，再建立 `prvLab5Task`、啟動 scheduler
- **`prvLab5Task()`**：每輪迴圈計數 `usCounter++`，用 `rand()` 決定要不要 power off，每 10 次呼叫一次 `prvLab5Commit()`，其餘時候單純印出目前的 counter

> [!NOTE]
> `usCounter` 是 `prvLab5Task()` 裡的 `static` 變數，住在一般的 SRAM（`.bss`），不是額外存在 `ucHeap` 或 checkpoint 專用區——它能在 restore 後接續原本的數字，純粹是因為「完整 SRAM 備份/還原」這個機制本身就會把它包含進去，不是另外特別處理的。

### 3.2 FreeRTOS / Port Modified

- refer to :
    - [main.c](/lab5/main.c)
    - [FreeRTOS_Source/portable/CCS/MSP430X/portext.asm](/lab5/FreeRTOS_Source/portable/CCS/MSP430X/portext.asm)
    - [lnk_msp430fr5994.cmd](/lab5/lnk_msp430fr5994.cmd)

#### main.c

- checkpoint 相關常數、兩份 backup array（`ucLab5BackupHeap`/`ucLab5BackupSram`/`xLab5BackupHeader`）、persistent selector（`usLab5LastCommitIndex`/`ulLab5CommitSequence`）宣告：[L69-139](/lab5/main.c#L69-L139)
- `main_lab()`：開機先 restore，再建立 demo task、啟動 scheduler：[L166-182](/lab5/main.c#L166-L182)
- `prvLab5Task()`：demo task 本體：[L185-221](/lab5/main.c#L185-L221)
- `prvLab5Commit()`：先存 CPU context，再把 `ucHeap`、SRAM 複製到 inactive slot，算 checksum，最後才寫 `magic` 跟更新 selector：[L224-252](/lab5/main.c#L224-L252)
- `prvLab5RestoreIfNeeded()`：依序還原 `ucHeap`、SRAM，重新設定 tick timer，最後還原 CPU context：[L255-271](/lab5/main.c#L255-L271)
- `prvLab5PowerOff()`：進 LPM4.5 模擬斷電：[L274-283](/lab5/main.c#L274-L283)
- checksum / slot 有效性 / 下一個 slot / 最新 sequence 的 helper：[L286-407](/lab5/main.c#L286-L407)

    ```mermaid
    flowchart LR
        A[usLab5NextCopyIndex 被呼叫] --> B{selector 指的 slot 有效?}
        B -- 是 --> K[用 selector 指的 slot]
        B -- 否 --> C[usLab5ValidCopyIndex]
        C --> D{selector 指的 slot 有效?}
        D -- 是 --> E[用 selector 指的 slot]
        D -- 否 --> F{兩個 slot 都有效?}
        F -- 是 --> G[用 sequence 較大的那個]
        F -- 否 --> H{只有一個有效?}
        H -- 是 --> I[用那一個]
        H -- 否 --> J[都無效，視為第一次開機]
        K --> L[回傳另一個 slot，當下次 commit 要寫的]
        E --> L
        G --> L
        I --> L
        J --> M[回傳 slot 0，當下次 commit 要寫的]
    ```

> [!IMPORTANT]
> Commit 的實際程式碼順序是「先存 CPU context，再複製 heap/SRAM」，跟題目寫的「`ucHeap` → SRAM → CPU registers」順序看起來不一樣，但這是必要的：`vLab5SaveCurrentTaskContext()` 是把暫存器 push 到**目前 task 自己的 stack 上**。因為這個 project 用 `xTaskCreate()` 動態配置 task（`configAPPLICATION_ALLOCATED_HEAP=1`），task 的 stack 其實是 `pvPortMalloc()` 從 **`ucHeap`** 裡切出來的一塊，不是獨立的 SRAM 區域——所以必須先把暫存器 push 進 stack，接著複製 `ucHeap` 才會包含這份「CPU registers」的內容；SRAM 備份則是另外一塊 `.bss`/`.data` 範圍，跟這個順序依賴無關。Restore 的順序完全照題目的順序：先還原 `ucHeap`（連同裡面的暫存器快照）、SRAM，最後才呼叫 `vLab5RestoreCurrentTaskContext()` 把快照 pop 回暫存器，順序不能反過來。

#### Port Assembly (portext.asm)

- 新增 `vLab5SaveCurrentTaskContext`：呼叫 `portSAVE_CONTEXT` 把目前暫存器存到 task 自己的 stack，緊接著呼叫 `portRESTORE_CONTEXT` 立刻還原，讓 C code 可以照常往下執行（不是真正切換 task，只是把「現在的狀態」做成一份可以留在 stack 上的快照）：[L142-153](/lab5/FreeRTOS_Source/portable/CCS/MSP430X/portext.asm#L142-L153)
- 新增 `vLab5RestoreCurrentTaskContext`：只呼叫 `portRESTORE_CONTEXT`，把 restore 進來的 SRAM（裡面有上次存的那份快照）pop 回暫存器，執行流程會接回當初呼叫 `vLab5SaveCurrentTaskContext()` 的地方繼續跑：[L156-163](/lab5/FreeRTOS_Source/portable/CCS/MSP430X/portext.asm#L156-L163)

#### Link Command File (lnk_msp430fr5994.cmd)

- 新增 `.lab5_checkpoint : type = NOINIT {} > FRAM2`，讓三個 backup array 透過 `#pragma DATA_SECTION` 放進 FRAM2，且開機時 C runtime 不會把它們清零/重新初始化：[L169-171](/lab5/lnk_msp430fr5994.cmd#L169-L171)

> [!NOTE]
> 三個 backup array 用 `#pragma DATA_SECTION(..., ".lab5_checkpoint")` 綁進這個自訂 section，linker 知道這塊記憶體被佔用、大小是多少，跟只是硬寫一個固定位址（`(uint8_t *) 0x20000`）比起來，不會被日後其他 FRAM2 配置悄悄覆寫，放不下也會在連結階段直接報錯。
