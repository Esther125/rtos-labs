# Lab for FreeRTOS — EDF Scheduler

> Source: **FreeRTOS Lab (EDF) - Google 簡報.pdf**  
> The text below is transcribed slide by slide. Each slide image is also embedded to preserve diagrams, screenshots, spacing, and other visual information.

---

## Slide 1 — Lab for FreeRTOS

# Lab for FreeRTOS

## EDF Scheduler

**Prof. Li-Pin Chang**  
**ESSLab@NCTU**

![Slide 1 — Title slide](assets/slide-01.png)

---

## Slide 2 — Fixed-Priority Scheduling

# Fixed-Priority Scheduling

- FreeRTOS supports priority scheduling
- There is no EDF support

![Slide 2 — Fixed-Priority Scheduling](assets/slide-02.png)

---

## Slide 3 — Adding Support for EDF

# Adding Support for EDF

- Identify where/when scheduling decisions are made
    - `vTaskSwitchContext`
- Add proper deadline information to task information (i.e., in TCB)
- Add code to pickup a ready job with the earliest deadline at the re-scheduling points
    - In this project, it is okay to use linear search

![Slide 3 — Adding Support for EDF](assets/slide-03.png)

---

## Slide 4 — Taksets for Test

# Taksets for Test

- Task set1 = `{ t1(1,3), t2(3,5) }`

    The diagram shows the EDF execution timeline for `t1` and `t2`, including job releases/deadlines and execution intervals over time.

- Task set2 = `{ t1(1,4), t2(2,5), t3(2,10) }`

![Slide 4 — Task sets and EDF timeline diagram](assets/slide-04.png)

---

## Slide 5 — Output

# Output

- For task set 1

| Time | event    | from |  to |
| ---: | -------- | ---: | --: |
|    1 | Complete |    1 |   2 |
|    4 | Complete |    2 |   1 |
|    5 | Complete |    1 |   2 |
|    6 | Preempt  |    2 |   1 |

The right side of the slide repeats the task-set-1 timeline and marks the corresponding completion and preemption points.

![Slide 5 — Expected output events and timeline](assets/slide-05.png)

---

## Slide 6 — Example Output

# Example Output

### Task Set 1

```text
T1 c:1 p:3
T2 c:3 p:5
1 complete T1 T2
4 complete T2 T1
5 complete T1 T2
6 preempt  T2 T1
7 complete T1 T2
9 complete T2 T1
10 complete T1 T2
13 complete T2 T1
14 complete T1 Idle
15 preempt  Idle T1
16 complete T1 T2
19 complete T2 T1
20 complete T1 T2
```

### Task Set 2

```text
T1 c:1 p:4
T2 c:2 p:5
T3 c:2 p:10
1 complete T1 T2
3 complete T2 T3
4 preempt  T3 T1
5 complete T1 T3
6 complete T3 T2
8 complete T2 T1
9 complete T1 Idle
10 preempt  Idle T2
12 complete T2 T1
13 complete T1 T3
15 complete T3 T2
17 complete T2 T1
18 complete T1 Idle
20 preempt  Idle T1
```

![Slide 6 — Console screenshots for Task Set 1 and Task Set 2](assets/slide-06.png)

---

## Slide 7 — More Hints

# More Hints

- Add deadline information in the TCB
    - You can pass them to tasks upon creation via the user-provided parameter
- Upon re-scheduling, visit the TCB list linearly; find the ready task whose deadline is the earliest
- Before a task delays for the next period, advance its deadline to the next period

![Slide 7 — More Hints](assets/slide-07.png)
