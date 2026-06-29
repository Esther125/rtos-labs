# Lab 3 for uC/OS-II: Ceiling Priority Protocol

---

## Objective

- To implement **Ceiling Priority Protocol (CPP)** for uC/OS-II mutex locks.

---

## uC/OS Mutex Locks

- A mutex lock is associated with a **priority**.
- Its priority is higher than the highest-priority locker.
- Example:
    - If `T3` and `T4` share a lock, the priority of the lock should be set to `2`.
    - When `T4` blocks `T3`, then `T4`'s priority becomes `2`.

---

## Disadvantages of PIP

The **Priority Inheritance Protocol (PIP)** avoids uncontrolled priority inversion, but it has two disadvantages:

1. A high-priority task can be blocked multiple times.
2. Deadlocks are possible.

---

## Scenario 1: Multiple Blocking in uC/OS-II PIP

### Timeline

Under PIP, high-priority task `H` is blocked by both `L1` and `L2`.

```text
Task H:
8  - 10    CPU execution
10         lock R1
15 - 17    use R1
17         lock R2
20 - 22    use R2

Task L1:
4  - 6     CPU execution
6  - 8     use R2
17 - 20    use R2

Task L2:
0  - 2     CPU execution
2  - 4     use R1
10 - 15    use R1
```

### Key Point

```text
Task H is in turn blocked by task L1 and task L2.
```

---

## Scenario 2: Deadlock in uC/OS-II PIP

### Timeline

Under PIP, a deadlock can occur when tasks lock resources in different orders.

```text
Task H:
5  - 7     CPU execution
7  - 10    use R2
10         lock R1

Task L:
0  - 2     CPU execution
2  - 5     use R1
10 - 13    use R1
13         lock R2
```

### Key Point

```text
A deadlock
```

---

## Ceiling Priority Protocol

- Also called **Highest-Locker Protocol**.
- When a task acquires a mutex lock, its priority becomes the highest among all lockers' priorities.
- In uC/OS-II, use the mutex's priority as the highest-locker's priority.
    - The mutex priority should be immediately higher than all lockers' priorities.

---

## S1 CPP: Removing Multiple Blockings

### Timeline After Applying CPP

```text
Task H:
8  - 9     CPU execution
9  - 11    use R1
11 - 13    use R2
13 - 15    CPU execution

Task L1:
4          arrival
15 - 17    CPU execution
17 - 21    use R2

Task L2:
0  - 2     CPU execution
2  - 9     use R1
```

### Key Point

```text
The result of applying CPP
```

CPP removes the multiple-blocking problem because the task holding the resource is boosted early enough to finish its critical section.

---

## S2 CPP: Avoiding Deadlocks

### Timeline After Applying CPP

```text
Task H:
5          arrival
12 - 14    CPU execution
14 - 17    use R2
17 - 20    use R1
20 - 23    use R2
20         unlock R1
23         unlock R2

Task L:
0  - 2     CPU execution
2  - 8     use R1
8  - 10    use R2
10 - 12    use R1
10         unlock R2
12         unlock R1
```

### Key Point

```text
Deadlocks are avoided by using CPP.
```

---

## Implementation

### Requirements

- Reuse your code from **Lab 1**.
- Do **not** reuse EDF.
- Modify the following two functions:
    - `OSMutexPend()`
    - `OSMutexPost()`

### `OSMutexPend()`

- If the mutex is free, boost the locker's priority.
- The locker means the caller task.

### `OSMutexPost()`

- Restore the original priority of the locker.

### Important Notice

Do **not** use:

```c
OSTaskChangePrio()
```

Reason:

```text
OSTaskChangePrio() calls OS_Sched() and results in unexpected behaviors.
```

---

## Implementation Details

- All tasks should add proper `OSTimeDly()` at their beginning to emulate their arrival times.
- Emulate durations of CPU execution and resource use with your code from Lab 0.

Example execution pattern:

```text
2 ticks -> lock R1 -> 6 ticks -> lock R2 -> 2 ticks -> unlock R2 -> 2 ticks -> unlock R1
```

---

## Output

- Similar to previous labs.
- Add `lock` and `unlock` events.
- Output the results of using CPP for:
    - Scenario 1
    - Scenario 2

---

## Output Example of S1

```text
Priority initialization:
R1: 1
R2: 2
Task1: 3
Task2: 4
Task3: 5

Task arrival time:
Task1: 8
Task2: 4
Task3: 0

Task execution time and resource used:
Task1:
1
2
10

Task2:
2
10

Task3:
2
10
```

---

## Expected Output

```text
20    lock      R1    (Prio=5 changes to=1)
90    unlock    R1    (Prio=1 changes to=5)
90    complete        5           3
------------------------------------------------------------
110   lock      R1    (Prio=3 changes to=1)
130   lock      R2    (Prio=1 changes to=1)
150   unlock    R2    (Prio=1 changes to=1)
150   unlock    R1    (Prio=1 changes to=3)
150   complete        3           4
170   lock      R2    (Prio=4 changes to=2)
220   unlock    R2    (Prio=2 changes to=4)
220   complete        4           19
```

---

## Note About Output Interpretation

At time `90`, it is actually a **preempt** because `T3` calls:

```c
OSMutexPost(R1)
```

Internally, `OSMutexPost(R1)` calls:

```c
OS_Sched()
```

This makes the task surrender the CPU to `T1`, the highest-priority task.

Because the prior lab defines `OS_Sched()` as `complete`, it is okay. `T3` ends after unlocking `R1` anyway.

The same interpretation applies to times:

```text
150
220
```
