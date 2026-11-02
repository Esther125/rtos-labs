# rtos-labs

Lab1~3 都是基於 **uC/OS-II** 這套 real-time kernel 做的練習，原始碼用 Borland C++ 編譯，實際跑在 DOS 環境（這裡用 DOSBox 模擬）上。

Lab4~5 則是基於 **FreeRTOS**，目標板是 TI MSP430FR5994 LaunchPad（FRAM MCU），用 Code Composer Studio 建置。這兩個沒有現成的模擬器可以跑——QEMU 主線從未支援 MSP430（官方 maintainer 2021 年在 mailing list 上明確證實），唯一找到的 out-of-tree `qemu-system-msp430` fork（2016 年、QEMU 2.4 時代）實際拿 FreeRTOS 去跑也是失敗的；Renode 則只有通用 MSP430 ISA、沒有 FR5994 的平台描述檔。所以這兩個 lab 是單純讀 code 整理，沒有實際編譯驗證過。

| Lab  | 說明                                                                          | 文件                     |
| ---- | ----------------------------------------------------------------------------- | ------------------------ |
| Lab1 | 實作 RM (Rate Monotonic) scheduling，模擬 periodic tasks 並偵測 deadline miss | [lab1.md](/lab1/lab1.md) |
| Lab2 | 把排程演算法換成 EDF (Earliest Deadline First)                                | [lab2.md](/lab2/lab2.md) |
| Lab3 | 幫 mutex lock 實作 Ceiling Priority Protocol (CPP)，避免 priority inversion   | [lab3.md](/lab3/lab3.md) |
| Lab4 | 幫 FreeRTOS 實作 EDF scheduler（跟 Lab2 同概念，換到 FreeRTOS 上）            | [lab4.md](/lab4/lab4.md) |
| Lab5 | 幫 FreeRTOS 實作 checkpoint / restore，斷電後能從 FRAM 還原執行狀態           | [lab5.md](/lab5/lab5.md) |
