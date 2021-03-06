/*
  Copyright (C) 2010-2012 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "stack.h"
#include "regs.h"
#include "globals.h"
#include "record.h"
#include "user_vm.h"
#include "time.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>      /* for kmap()/kunmap() */
#include <linux/pagemap.h>      /* for page_cache_release() */
#include <asm/page.h>
#include <asm/processor.h>

#include "unwind.c"

#ifndef VTSS_STACK_LIMIT
#define VTSS_STACK_LIMIT 0x200000 /* 2Mb */
#endif

#if defined(CONFIG_X86_64) && defined(VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK)
#include <asm/stacktrace.h>

#ifdef VTSS_AUTOCONF_STACKTRACE_OPS_WARNING
static void vtss_warning(void *data, char *msg)
{
}

static void vtss_warning_symbol(void *data, char *msg, unsigned long symbol)
{
}
#endif

static int vtss_stack_stack(void *data, char *name)
{
    return 0;
}

static void vtss_stack_address(void *data, unsigned long addr, int reliable)
{
    TRACE("%s%pB", reliable ? "" : "? ", (void*)addr);
}

static unsigned long vtss_stack_walk(
    struct thread_info *tinfo,
    unsigned long *stack,
    unsigned long bp,
    const struct stacktrace_ops *ops,
    void *data,
    unsigned long *end,
    int *graph)
{
    unsigned long* pbp = (unsigned long*)data;
    unsigned long kstart = (unsigned long)__START_KERNEL_map + ((CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1));

    TRACE("bp=0x%p, stack=0x%p, end=0x%p", (void*)bp, stack, end);
    bp = print_context_stack(tinfo, stack, bp, ops, data, end, graph);
    if (pbp != NULL && bp < kstart) {
        TRACE("user bp=0x%p", (void*)bp);
        *pbp = bp;
    }
    return bp;
}

static const struct stacktrace_ops vtss_stack_ops = {
#ifdef VTSS_AUTOCONF_STACKTRACE_OPS_WARNING
    .warning        = vtss_warning,
    .warning_symbol = vtss_warning_symbol,
#endif
    .stack          = vtss_stack_stack,
    .address        = vtss_stack_address,
    .walk_stack     = vtss_stack_walk,
};

#endif /* CONFIG_X86_64 && VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK */

int vtss_stack_dump(struct vtss_transport_data* trnd, stack_control_t* stk, struct task_struct* task, struct pt_regs* regs, void* reg_fp, int in_irq)
{
    int rc;
    user_vm_accessor_t* acc;
    void* stack_base = stk->bp.vdp;
    void *reg_ip, *reg_sp;

    if (unlikely(regs == NULL)) {
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x: incorrect regs",
                        task->pid, smp_processor_id());
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        }
        return -EFAULT;
    }
    stk->dbgmsg[0] = '\0';

    /* Get IP and SP registers from current space */
    reg_ip = (void*)REG(ip, regs);
    reg_sp = (void*)REG(sp, regs);

#if defined(CONFIG_X86_64) && defined(VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK)
    { /* Unwind kernel stack and get user BP if possible */
        unsigned long bp = 0UL;
        unsigned long kstart = (unsigned long)__START_KERNEL_map + ((CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1));

#ifdef VTSS_AUTOCONF_DUMP_TRACE_HAVE_BP
        dump_trace(task, NULL, NULL, 0, &vtss_stack_ops, &bp);
#else
        dump_trace(task, NULL, NULL, &vtss_stack_ops, &bp);
#endif
//        TRACE("bp=0x%p <=> fp=0x%p", (void*)bp, reg_fp);
        reg_fp = bp ? (void*)bp : reg_fp;
#ifdef VTSS_DEBUG_TRACE
        if (reg_fp > (void*)kstart) {
            printk("Warning: bp=0x%p in kernel\n", reg_fp);
            dump_stack();
            rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p]: User bp=0x%p inside kernel space",
                            task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
            if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                stk->dbgmsg[rc] = '\0';
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            }
        }
#endif
    }
#endif /* CONFIG_X86_64 && VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK */

    if (unlikely(!user_mode_vm(regs))) {
        /* kernel mode regs, so get a user mode regs */
#if defined(CONFIG_X86_64) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
        regs = task_pt_regs(task); /*< get user mode regs */
        if (regs == NULL || !user_mode_vm(regs))
#endif
        {
#ifdef VTSS_DEBUG_TRACE
            strcat(stk->dbgmsg, "Cannot get user mode regs");
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            printk("Warning: %s\n", stk->dbgmsg);
            dump_stack();
#endif
            return -EFAULT;
        }
    }

    /* Get IP and SP registers from user space */
    reg_ip = (void*)REG(ip, regs);
    reg_sp = (void*)REG(sp, regs);

    { /* Check for correct stack range in task->mm */
        struct vm_area_struct* vma;

#ifdef VTSS_CHECK_IP_IN_MAP
        /* Check IP in module map */
        vma = find_vma(task->mm, (unsigned long)reg_ip);
        if (likely(vma != NULL)) {
            unsigned long vm_start = vma->vm_start;
            unsigned long vm_end   = vma->vm_end;

            if ((unsigned long)reg_ip < vm_start ||
                (!((vma->vm_flags & (VM_EXEC | VM_WRITE)) == VM_EXEC &&
                    vma->vm_file && vma->vm_file->f_dentry) &&
                 !(vma->vm_mm && vma->vm_start == (long)vma->vm_mm->context.vdso)))
            {
#ifdef VTSS_DEBUG_TRACE
                rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p, found_vma=[0x%lx,0x%lx]: Unable to find executable module",
                                task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp, vm_start, vm_end);
                if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                    stk->dbgmsg[rc] = '\0';
                    vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                }
#endif
                return -EFAULT;
            }
        } else {
#ifdef VTSS_DEBUG_TRACE
            rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Unable to find executable region",
                            task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
            if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                stk->dbgmsg[rc] = '\0';
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            }
#endif
            return -EFAULT;
        }
#endif /* VTSS_CHECK_IP_IN_MAP */

        /* Check SP in module map */
        vma = find_vma(task->mm, (unsigned long)reg_sp);
        if (likely(vma != NULL)) {
            unsigned long vm_start = vma->vm_start + ((vma->vm_flags & VM_GROWSDOWN) ? PAGE_SIZE : 0UL);
            unsigned long vm_end   = vma->vm_end;

//            TRACE("vma=[0x%lx - 0x%lx], flags=0x%lx", vma->vm_start, vma->vm_end, vma->vm_flags);
            if ((unsigned long)reg_sp < vm_start ||
                (vma->vm_flags & (VM_READ | VM_WRITE)) != (VM_READ | VM_WRITE))
            {
#ifdef VTSS_DEBUG_TRACE
                rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p, found_vma=[0x%lx,0x%lx]: Unable to find user stack boundaries",
                                task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp, vm_start, vm_end);
                if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                    stk->dbgmsg[rc] = '\0';
                    vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                }
#endif
                return -EFAULT;
            }
            if (!((unsigned long)stack_base >= vm_start &&
                  (unsigned long)stack_base <= vm_end)  ||
                 ((unsigned long)stack_base <= (unsigned long)reg_sp))
            {
                if ((unsigned long)stack_base != 0UL) {
                    TRACE("Fixup stack base to 0x%lx instead of 0x%lx", vm_end, (unsigned long)stack_base);
                }
                stack_base = (void*)vm_end;
                stk->clear(stk);
#ifdef VTSS_STACK_LIMIT
                stack_base = (void*)min((unsigned long)reg_sp + VTSS_STACK_LIMIT, vm_end);
                if ((unsigned long)stack_base != vm_end) {
                    TRACE("Limiting stack base to 0x%lx instead of 0x%lx, drop 0x%lx bytes", (unsigned long)stack_base, vm_end, (vm_end - (unsigned long)stack_base));
                }
            } else {
                stack_base = (void*)min((unsigned long)reg_sp + VTSS_STACK_LIMIT, vm_end);
                if ((unsigned long)stack_base != vm_end) {
                    TRACE("Limiting stack base to 0x%lx instead of 0x%lx, drop 0x%lx bytes", (unsigned long)stack_base, vm_end, (vm_end - (unsigned long)stack_base));
                }
#endif /* VTSS_STACK_LIMIT */
            }
        }
    }

#ifdef VTSS_DEBUG_TRACE
    /* Create a common header for debug message */
    rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: USER STACK: ",
                    task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
    if (!(rc > 0 && rc < sizeof(stk->dbgmsg)-1))
        rc = 0;
    stk->dbgmsg[rc] = '\0';
#else
    stk->dbgmsg[0] = '\0';
#endif

    if (stk->ip.vdp == reg_ip &&
        stk->sp.vdp == reg_sp &&
        stk->bp.vdp == stack_base &&
        stk->fp.vdp == reg_fp)
    {
        strcat(stk->dbgmsg, "The same context");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return 0; /* Assume that nothing was changed */
    }

    /* Try to lock vm accessor */
    acc = vtss_user_vm_accessor_init(in_irq, vtss_time_limit);
    if (unlikely((acc == NULL) || acc->trylock(acc, task))) {
        vtss_user_vm_accessor_fini(acc);
        strcat(stk->dbgmsg, "Unable to lock vm accessor");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return -EBUSY;
    }

    /* stk->setup(stk, acc, reg_ip, reg_sp, stack_base, reg_fp, stk->wow64); */
    stk->acc    = acc;
    stk->ip.vdp = reg_ip;
    stk->sp.vdp = reg_sp;
    stk->bp.vdp = stack_base;
    stk->fp.vdp = reg_fp;
    VTSS_PROFILE(unw, rc = stk->unwind(stk));
    /* Check unwind result */
    if (unlikely(rc == VTSS_ERR_NOMEMORY)) {
        /* Try again with realloced buffer */
        while (rc == VTSS_ERR_NOMEMORY && !stk->realloc(stk)) {
            VTSS_PROFILE(unw, rc = stk->unwind(stk));
        }
        if (rc == VTSS_ERR_NOMEMORY) {
            strcat(stk->dbgmsg, "Not enough memory - ");
        }
    }
    vtss_user_vm_accessor_fini(acc);
    if (unlikely(rc)) {
        stk->clear(stk);
        strcat(stk->dbgmsg, "Unwind error");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
    }
    return rc;
}

int vtss_stack_record(struct vtss_transport_data* trnd, stack_control_t* stk, pid_t tid, int cpu, int is_safe)
{
    int rc = -EFAULT;
    unsigned short sample_type;
    int sktlen = stk->compress(stk);

    if (unlikely(sktlen == 0)) {
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Huge or Zero stack after compression",
                        tid, smp_processor_id(), stk->ip.vdp, stk->sp.vdp, stk->bp.vdp, stk->fp.vdp);
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, is_safe);
        }
        return -EFAULT;
    }

    if (stk->is_full(stk)) { /* full stack */
        sample_type = (sizeof(void*) == 8 && !stk->wow64) ? UECSYSTRACE_STACK_CTX64_V0 : UECSYSTRACE_STACK_CTX32_V0;
    } else { /* incremental stack */
        sample_type = (sizeof(void*) == 8 && !stk->wow64) ? UECSYSTRACE_STACK_CTXINC64_V0 : UECSYSTRACE_STACK_CTXINC32_V0;
    }

#ifdef VTSS_USE_UEC
    {
        stk_trace_record_t stkrec;
        /// save current alt. stack:
        /// [flagword - 4b][residx][cpuidx - 4b][tsc - 8b]
        /// ...[sampled address - 8b][systrace{sts}]
        ///                       [length - 2b][type - 2b]...
        stkrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_SYSTRACE;
        stkrec.residx   = tid;
        stkrec.cpuidx   = cpu;
        stkrec.cputsc   = vtss_time_cpu();
        stkrec.execaddr = (unsigned long long)stk->ip.szt;
        stkrec.type     = sample_type;

        if (!stk->wow64) {
            stkrec.size = 4 + sizeof(void*) + sizeof(void*);
            stkrec.sp   = stk->sp.szt;
            stkrec.fp   = stk->fp.szt;
        } else { /// a 32-bit stack in a 32-bit process on a 64-bit system
            stkrec.size = 4 + sizeof(unsigned int) + sizeof(unsigned int);
            stkrec.sp32 = (unsigned int)stk->sp.szt;
            stkrec.fp32 = (unsigned int)stk->fp.szt;
        }
        rc = 0;
        if (sktlen > 0xfffb) {
            lstk_trace_record_t lstkrec;

            TRACE("ip=0x%p, sp=0x%p, fp=0x%p: Large Trace %d bytes", stk->ip.vdp, stk->sp.vdp, stk->fp.vdp, sktlen);
            lstkrec.size = (unsigned int)(stkrec.size + sktlen + 2); /* 2 = sizeof(int) - sizeof(short) */
            lstkrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_LARGETRACE;
            lstkrec.residx   = stkrec.residx;
            lstkrec.cpuidx   = stkrec.cpuidx;
            lstkrec.cputsc   = stkrec.cputsc;
            lstkrec.execaddr = stkrec.execaddr;
            lstkrec.type     = stkrec.type;
            lstkrec.sp       = stkrec.sp;
            lstkrec.fp       = stkrec.fp;
            if (vtss_transport_record_write(trnd, &lstkrec, sizeof(lstkrec) - (stk->wow64*8), stk->data(stk), sktlen, is_safe)) {
                TRACE("STACK_record_write() FAIL");
                rc = -EFAULT;
            }
        } else {
            /// correct the size of systrace
            stkrec.size += (unsigned short)sktlen;
            if (vtss_transport_record_write(trnd, &stkrec, sizeof(stkrec) - (stk->wow64*8), stk->data(stk), sktlen, is_safe)) {
                TRACE("STACK_record_write() FAIL");
                rc = -EFAULT;
            }
        }
    }
#else  /* VTSS_USE_UEC */
    if (unlikely(sktlen > 0xfffb)) {
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Large Stack Trace %d bytes",
                        tid, smp_processor_id(), stk->ip.vdp, stk->sp.vdp, stk->bp.vdp, stk->fp.vdp, sktlen);
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, is_safe);
        }
        return -EFAULT;
    } else {
        void* entry;
        stk_trace_record_t* stkrec = (stk_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(stk_trace_record_t) - (stk->wow64*8) + sktlen);
        if (likely(stkrec)) {
            /// save current alt. stack:
            /// [flagword - 4b][residx][cpuidx - 4b][tsc - 8b]
            /// ...[sampled address - 8b][systrace{sts}]
            ///                       [length - 2b][type - 2b]...
            stkrec->flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_SYSTRACE;
            stkrec->residx   = tid;
            stkrec->cpuidx   = cpu;
            stkrec->cputsc   = vtss_time_cpu();
            stkrec->execaddr = (unsigned long long)stk->ip.szt;
            stkrec->size     = (unsigned short)sktlen + sizeof(stkrec->size) + sizeof(stkrec->type);
            stkrec->type     = sample_type;
            if (!stk->wow64) {
                stkrec->size += sizeof(void*) + sizeof(void*);
                stkrec->sp   = stk->sp.szt;
                stkrec->fp   = stk->fp.szt;
            } else { /* a 32-bit stack in a 32-bit process on a 64-bit system */
                stkrec->size += sizeof(unsigned int) + sizeof(unsigned int);
                stkrec->sp32 = (unsigned int)stk->sp.szt;
                stkrec->fp32 = (unsigned int)stk->fp.szt;
            }
            memcpy((char*)stkrec+sizeof(stk_trace_record_t)-(stk->wow64*8), stk->compressed, sktlen);
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
    }
#endif /* VTSS_USE_UEC */
    return rc;
}
