/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#include <interrupts.h>

#include <cpu.h>
#include <vm.h>
#include <bitmap.h>
#include <string.h>

BITMAP_ALLOC(hyp_interrupt_bitmap, MAX_INTERRUPTS);

irq_handler_t interrupt_handlers[MAX_INTERRUPTS];

void interrupts_cpu_glbenable(bool en)
{
    interrupts_arch_cpu_enable(en);
}

void interrupts_cpu_sendipi(uint64_t target_cpu, uint64_t ipi_id)
{
    interrupts_arch_ipi_send(target_cpu, ipi_id);
}

void interrupts_cpu_enable(uint64_t int_id, bool en)
{
    interrupts_arch_enable(int_id, en);
}

bool interrupts_check(uint64_t int_id)
{
    return interrupts_arch_check(int_id);
}

void interrupts_clear(uint64_t int_id)
{
    interrupts_arch_clear(int_id);
}

void interrupts_init()
{
    interrupts_arch_init();

    if (cpu.id == CPU_MASTER) {
        /**
         * At this point interrupts must be in a known but disabled from
         * the point of view of each cpu.
         */
        interrupts_cpu_glbenable(true);

        interrupts_reserve(IPI_CPU_MSG, cpu_msg_handler);
    }

    interrupts_cpu_enable(IPI_CPU_MSG, true);
}

static inline bool interrupt_is_reserved(int int_id)
{
    return bitmap_get(hyp_interrupt_bitmap, int_id);
}

inline void interrupts_vm_inject(vm_t *vm, uint64_t id, uint64_t source)
{
    interrupts_arch_vm_inject(vm, id, source);
}

enum irq_res interrupts_handle(uint64_t int_id, uint64_t source)
{
    if (vm_has_interrupt(cpu.vcpu->vm, int_id)) {
        interrupts_vm_inject(cpu.vcpu->vm, int_id, source);

        return FORWARD_TO_VM;

    } else if (interrupt_is_reserved(int_id)) {
        interrupt_handlers[int_id](int_id, source);

        return HANDLED_BY_HYP;

    } else {
        ERROR("received unknown interrupt id = %d", int_id);
    }
}

void interrupts_vm_assign(vm_t *vm, uint64_t id)
{
    // TODO: make sure a hardware interrupt is assigned to a single VM

    interrupts_arch_vm_assign(vm, id);

    bitmap_set(vm->interrupt_bitmap, id);
}

void interrupts_reserve(uint64_t int_id, irq_handler_t handler)
{
    if (int_id < MAX_INTERRUPTS) {
        interrupt_handlers[int_id] = handler;
        bitmap_set(hyp_interrupt_bitmap, int_id);
    }
}
