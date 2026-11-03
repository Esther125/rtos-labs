# rtos-labs

114-2 學期 **嵌入式即時系統** 課程 Labs (A+)

- Lab1~3：
    - uC/OS-II，開發板用 FPGA 板子 + Nios II soft-core CPU（Altera/Intel）
    - 先用 DOSBox 模擬 DOS 環境寫 & 驗證 scheduling logic，再上真實硬體驗證
- Lab4~5：
    - FreeRTOS，開發板用 TI MSP430FR5994 LaunchPad
    - 用 Code Composer Studio 建置，沒有適合的模擬器，直接上板子驗證

| Lab  | RTOS     | 說明                                                                          | 文件                     |
| ---- | -------- | ----------------------------------------------------------------------------- | ------------------------ |
| Lab1 | uC/OS-II | 實作 RM (Rate Monotonic) scheduling，模擬 periodic tasks 並偵測 deadline miss | [lab1.md](/lab1/lab1.md) |
| Lab2 | uC/OS-II | 把排程演算法換成 EDF (Earliest Deadline First)                                | [lab2.md](/lab2/lab2.md) |
| Lab3 | uC/OS-II | 幫 mutex lock 實作 Ceiling Priority Protocol (CPP)，避免 priority inversion   | [lab3.md](/lab3/lab3.md) |
| Lab4 | FreeRTOS | 實作 EDF (Earliest Deadline First) scheduler                                  | [lab4.md](/lab4/lab4.md) |
| Lab5 | FreeRTOS | 實作 checkpoint / restore，斷電後能從 FRAM 還原執行狀態                       | [lab5.md](/lab5/lab5.md) |
