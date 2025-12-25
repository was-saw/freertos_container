/******************************************************************************
*
* Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
* Copyright (C) 2014 - 2021 Xilinx, Inc. All rights reserved.
* Copyright (c) 2022 - 2023 Advanced Micro Devices, Inc. All Rights Reserved.
*
* SPDX-License-Identifier: MIT
*
******************************************************************************/
/*****************************************************************************/
/**
* @file port_asm_vectors.S
*
* FreeRTOS vector table for the Cortex A53 processor.
* Modified for use with standalone BSP - only defines _freertos_vector_table.
* The default _vector_table is provided by the standalone BSP.
*
* Call vPortInstallFreeRTOSVectorTable() before starting the scheduler to
* switch from the standalone BSP vector table to the FreeRTOS vector table.
*
******************************************************************************/

#include "bspconfig.h"

#if defined (versal) && !defined(ARMR5)
#define GICv3
#else
#define GICv2
#endif

.org 0
.text

.globl _freertos_vector_table
.globl vPortInstallFreeRTOSVectorTable

.globl FreeRTOS_SWI_Handler
.globl FreeRTOS_IRQ_Handler
.globl SErrorInterrupt
.globl SynchronousInterrupt
.globl SynchronousInterruptHandler
.globl SErrorInterruptHandler

.macro exception_return
	eret
#if defined (versal)
	dsb nsh
	isb
#endif
.endm

/******************************************************************************
 * FreeRTOS vector table.
 * This will be installed by vPortInstallFreeRTOSVectorTable() before the
 * scheduler starts, replacing the standalone BSP's vector table.
 *****************************************************************************/
.section .freertos_vectors, "ax"

.align 11
_freertos_vector_table:
	b	FreeRTOS_SWI_Handler

.org (_freertos_vector_table + 0x80)
	b	FreeRTOS_IRQ_Handler

.org (_freertos_vector_table + 0x100)
#if defined(GICv3)
	b	FreeRTOS_IRQ_Handler
#else
	b	.
#endif

.org (_freertos_vector_table + 0x180)
	b	SErrorInterruptHandler

.org (_freertos_vector_table + 0x200)
	b	FreeRTOS_SWI_Handler

.org (_freertos_vector_table + 0x280)
	b	FreeRTOS_IRQ_Handler

.org (_freertos_vector_table + 0x300)
	b	FreeRTOS_IRQ_Handler

.org (_freertos_vector_table + 0x380)
	b	SErrorInterruptHandler

.org (_freertos_vector_table + 0x400)
	b	.

.org (_freertos_vector_table + 0x480)
	b	.

.org (_freertos_vector_table + 0x500)
	b	.

.org (_freertos_vector_table + 0x580)
	b	.

.org (_freertos_vector_table + 0x600)
	b	.

.org (_freertos_vector_table + 0x680)
	b	.

.org (_freertos_vector_table + 0x700)
	b	.

.org (_freertos_vector_table + 0x780)
	b	.

.org (_freertos_vector_table + 0x800)

/******************************************************************************
 * vPortInstallFreeRTOSVectorTable
 *
 * Install the FreeRTOS vector table. Call this function before starting the
 * FreeRTOS scheduler to switch from the standalone BSP vector table to the
 * FreeRTOS vector table.
 *
 * void vPortInstallFreeRTOSVectorTable( void );
 *****************************************************************************/
.text
.align 4
.type vPortInstallFreeRTOSVectorTable, %function
vPortInstallFreeRTOSVectorTable:
	/* Load the address of the FreeRTOS vector table */
	ldr	x0, =_freertos_vector_table

	/* Determine current exception level and set appropriate VBAR */
	mrs	x1, CurrentEL
	cmp	x1, #0x0C		/* EL3? */
	b.eq	install_el3
	cmp	x1, #0x08		/* EL2? */
	b.eq	install_el2
	cmp	x1, #0x04		/* EL1? */
	b.eq	install_el1
	b	install_done		/* Should not happen */

install_el3:
	msr	VBAR_EL3, x0
	b	install_done

install_el2:
	msr	VBAR_EL2, x0
	b	install_done

install_el1:
	msr	VBAR_EL1, x0

install_done:
	dsb	sy
	isb
	ret

/******************************************************************************
 * Exception Handlers
 *****************************************************************************/
.text

SynchronousInterruptHandler:
	stp	X0,X1, [sp,#-0x10]!
	stp	X2,X3, [sp,#-0x10]!
	stp	X4,X5, [sp,#-0x10]!
	stp	X6,X7, [sp,#-0x10]!
	stp	X8,X9, [sp,#-0x10]!
	stp	X10,X11, [sp,#-0x10]!
	stp	X12,X13, [sp,#-0x10]!
	stp	X14,X15, [sp,#-0x10]!
	stp	X16,X17, [sp,#-0x10]!
	stp	X18,X19, [sp,#-0x10]!
	stp	X29,X30, [sp,#-0x10]!

	bl	SynchronousInterrupt

	ldp	X29,X30, [sp], #0x10
	ldp	X18,X19, [sp], #0x10
	ldp	X16,X17, [sp], #0x10
	ldp	X14,X15, [sp], #0x10
	ldp	X12,X13, [sp], #0x10
	ldp	X10,X11, [sp], #0x10
	ldp	X8,X9, [sp], #0x10
	ldp	X6,X7, [sp], #0x10
	ldp	X4,X5, [sp], #0x10
	ldp	X2,X3, [sp], #0x10
	ldp	X0,X1, [sp], #0x10

	exception_return


SErrorInterruptHandler:
	stp	X0,X1, [sp,#-0x10]!
	stp	X2,X3, [sp,#-0x10]!
	stp	X4,X5, [sp,#-0x10]!
	stp	X6,X7, [sp,#-0x10]!
	stp	X8,X9, [sp,#-0x10]!
	stp	X10,X11, [sp,#-0x10]!
	stp	X12,X13, [sp,#-0x10]!
	stp	X14,X15, [sp,#-0x10]!
	stp	X16,X17, [sp,#-0x10]!
	stp	X18,X19, [sp,#-0x10]!
	stp	X29,X30, [sp,#-0x10]!

	bl	SErrorInterrupt

	ldp	X29,X30, [sp], #0x10
	ldp	X18,X19, [sp], #0x10
	ldp	X16,X17, [sp], #0x10
	ldp	X14,X15, [sp], #0x10
	ldp	X12,X13, [sp], #0x10
	ldp	X10,X11, [sp], #0x10
	ldp	X8,X9, [sp], #0x10
	ldp	X6,X7, [sp], #0x10
	ldp	X4,X5, [sp], #0x10
	ldp	X2,X3, [sp], #0x10
	ldp	X0,X1, [sp], #0x10

	exception_return

.end
