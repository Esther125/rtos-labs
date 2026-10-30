# rtos-labs

Lab1~3 都是基於 **uC/OS-II** 這套 real-time kernel 做的練習，原始碼用 Borland C++ 編譯，實際跑在 DOS 環境（這裡用 DOSBox 模擬）上。

| Lab  | 說明                                                                          | 文件                     |
| ---- | ----------------------------------------------------------------------------- | ------------------------ |
| Lab1 | 實作 RM (Rate Monotonic) scheduling，模擬 periodic tasks 並偵測 deadline miss | [lab1.md](/lab1/lab1.md) |
| Lab2 | 把排程演算法換成 EDF (Earliest Deadline First)                                | [lab2.md](/lab2/lab2.md) |
| Lab3 | 幫 mutex lock 實作 Ceiling Priority Protocol (CPP)，避免 priority inversion   | [lab3.md](/lab3/lab3.md) |
