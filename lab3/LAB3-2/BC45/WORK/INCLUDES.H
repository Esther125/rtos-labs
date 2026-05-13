/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                           (c) Copyright 1992-1999, Jean J. Labrosse, Weston, FL
*                                           All Rights Reserved
*
*                                           MASTER INCLUDE FILE
*********************************************************************************************************
*/

#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <conio.h>
#include    <dos.h>
#include    <setjmp.h>

#include    "\software\ucos-ii\ix86l\bc45\os_cpu.h"
#include    "os_cfg.h"
#include    "\software\ucos-ii\source\ucos_ii.h"
#include    "\software\blocks\pc\bc45\pc.h"

#ifndef SCHED_TRACE_H
#define SCHED_TRACE_H

#define SCHED_TRACE_MAX 50

typedef struct {
    INT32U time;
    INT8U  type;
    INT8U  resource;
    INT8U  from;
    INT8U  to;
} sched_event_t;

extern volatile sched_event_t sched_trace[SCHED_TRACE_MAX];
extern volatile int sched_trace_idx;

#endif