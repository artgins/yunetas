/*
 *  Code from:
 *
 * pidstat: Report statistics for Linux tasks
 * (C) 2007-2016 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */
#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************
 *              Constants
 **************************************************/
#define MAX_COMM_LEN    128
#define MAX_CMDLINE_LEN 128

#define F_NO_PID_IO 0x01
#define F_NO_PID_FD 0x02

#define STAT            "/proc/stat"
#define UPTIME          "/proc/uptime"
#define DISKSTATS       "/proc/diskstats"
#define INTERRUPTS      "/proc/interrupts"
#define MEMINFO         "/proc/meminfo"

#define PID_STAT        "/proc/%u/stat"
#define PID_STATUS      "/proc/%u/status"
#define PID_IO          "/proc/%u/io"
#define PID_CMDLINE     "/proc/%u/cmdline"
#define PID_SMAP        "/proc/%u/smaps"
#define PID_FD          "/proc/%u/fd"

#define PROC_TASK       "/proc/%u/task"
#define TASK_STAT       "/proc/%u/task/%u/stat"
#define TASK_STATUS     "/proc/%u/task/%u/status"
#define TASK_IO         "/proc/%u/task/%u/io"
#define TASK_CMDLINE    "/proc/%u/task/%u/cmdline"
#define TASK_SMAP       "/proc/%u/task/%u/smaps"
#define TASK_FD         "/proc/%u/task/%u/fd"

/*
 * kB <-> number of pages.
 * Page size depends on machine architecture (4 kB, 8 kB, 16 kB, 64 kB...)
 */
// #define KB_TO_PG(k) ((k) >> kb_shift)
#define PG_TO_KB(k) ((k) << get_kb_shift())

/***************************************************
 *              Structures
 **************************************************/
#ifdef __linux__
/* Structure for memory and swap space utilization statistics */
struct stats_memory {
    unsigned long frmkb __attribute__ ((aligned (8)));
    unsigned long bufkb __attribute__ ((aligned (8)));
    unsigned long camkb __attribute__ ((aligned (8)));
    unsigned long tlmkb __attribute__ ((aligned (8)));
    unsigned long frskb __attribute__ ((aligned (8)));
    unsigned long tlskb __attribute__ ((aligned (8)));
    unsigned long caskb __attribute__ ((aligned (8)));
    unsigned long comkb __attribute__ ((aligned (8)));
    unsigned long activekb  __attribute__ ((aligned (8)));
    unsigned long inactkb   __attribute__ ((aligned (8)));
    unsigned long dirtykb   __attribute__ ((aligned (8)));
    unsigned long anonpgkb  __attribute__ ((aligned (8)));
    unsigned long slabkb    __attribute__ ((aligned (8)));
    unsigned long kstackkb  __attribute__ ((aligned (8)));
    unsigned long pgtblkb   __attribute__ ((aligned (8)));
    unsigned long vmusedkb  __attribute__ ((aligned (8)));
};

#define STATS_MEMORY_SIZE   (sizeof(struct stats_memory))

struct pid_stats {
    unsigned long long read_bytes           __attribute__ ((aligned (16)));
    unsigned long long write_bytes          __attribute__ ((packed));
    unsigned long long cancelled_write_bytes    __attribute__ ((packed));
    unsigned long long total_vsz            __attribute__ ((packed));
    unsigned long long total_rss            __attribute__ ((packed));
    unsigned long long total_stack_size     __attribute__ ((packed));
    unsigned long long total_stack_ref      __attribute__ ((packed));
    unsigned long long total_threads        __attribute__ ((packed));
    unsigned long long total_fd_nr          __attribute__ ((packed));
    unsigned long long blkio_swapin_delays      __attribute__ ((packed));
    unsigned long long minflt           __attribute__ ((packed));
    unsigned long long cminflt          __attribute__ ((packed));
    unsigned long long majflt           __attribute__ ((packed));
    unsigned long long cmajflt          __attribute__ ((packed));
    unsigned long long utime            __attribute__ ((packed));
    long long          cutime           __attribute__ ((packed));
    unsigned long long stime            __attribute__ ((packed));
    long long          cstime           __attribute__ ((packed));
    unsigned long long gtime            __attribute__ ((packed));
    long long          cgtime           __attribute__ ((packed));
    unsigned long long vsz              __attribute__ ((packed));
    unsigned long long rss              __attribute__ ((packed));
    unsigned long      nvcsw            __attribute__ ((packed));
    unsigned long      nivcsw           __attribute__ ((packed));
    unsigned long      stack_size           __attribute__ ((packed));
    unsigned long      stack_ref            __attribute__ ((packed));
    /* If pid is null, the process has terminated */
    unsigned int       pid              __attribute__ ((packed));
    /* If tgid is not null, then this PID is in fact a TID */
    unsigned int       tgid             __attribute__ ((packed));
    unsigned int       rt_asum_count        __attribute__ ((packed));
    unsigned int       rc_asum_count        __attribute__ ((packed));
    unsigned int       uc_asum_count        __attribute__ ((packed));
    unsigned int       tf_asum_count        __attribute__ ((packed));
    unsigned int       sk_asum_count        __attribute__ ((packed));
    unsigned int       delay_asum_count     __attribute__ ((packed));
    unsigned int       processor            __attribute__ ((packed));
    unsigned int       priority         __attribute__ ((packed));
    unsigned int       policy           __attribute__ ((packed));
    unsigned int       flags            __attribute__ ((packed));
    unsigned int       uid              __attribute__ ((packed));
    unsigned int       threads          __attribute__ ((packed));
    unsigned int       fd_nr            __attribute__ ((packed));
    char               comm[MAX_COMM_LEN];
    char               cmdline[MAX_CMDLINE_LEN];
};

#define PID_STATS_SIZE  (sizeof(struct pid_stats))

/*
 * Structure for CPU statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_cpu {
    unsigned long long cpu_user     __attribute__ ((aligned (16)));
    unsigned long long cpu_nice     __attribute__ ((aligned (16)));
    unsigned long long cpu_sys      __attribute__ ((aligned (16)));
    unsigned long long cpu_idle     __attribute__ ((aligned (16)));
    unsigned long long cpu_iowait       __attribute__ ((aligned (16)));
    unsigned long long cpu_steal        __attribute__ ((aligned (16)));
    unsigned long long cpu_hardirq      __attribute__ ((aligned (16)));
    unsigned long long cpu_softirq      __attribute__ ((aligned (16)));
    unsigned long long cpu_guest        __attribute__ ((aligned (16)));
    unsigned long long cpu_guest_nice   __attribute__ ((aligned (16)));
};

#define STATS_CPU_SIZE  (sizeof(struct stats_cpu))

/*
 * Structure for interrupts statistics.
 * In activity buffer: First structure is for total number of interrupts ("SUM").
 * Following structures are for each individual interrupt (0, 1, etc.)
 *
 * NOTE: The total number of interrupts is saved as a %llu by the kernel,
 * whereas individual interrupts are saved as %u.
 */
struct stats_irq {
    unsigned long long irq_nr   __attribute__ ((aligned (16)));
};

#define STATS_IRQ_SIZE  (sizeof(struct stats_irq))

/***************************************************
 *              Prototypes
 **************************************************/

unsigned long total_ram_in_kb(void); /* Total memory in kB */
unsigned long proc_vmem_in_kb(unsigned int pid); /* virtual memory in kB */
int read_proc_pid_cmdline(unsigned int pid, struct pid_stats *pst, unsigned int tgid);
int read_proc_pid_fd(unsigned int pid, struct pid_stats *pst, unsigned int tgid);
int read_proc_pid_io(unsigned int pid, struct pid_stats *pst, unsigned int tgid);
int read_proc_pid_smap(unsigned int pid, struct pid_stats *pst, unsigned int tgid);
int read_proc_pid_status(unsigned int pid, struct pid_stats *pst, unsigned int tgid);
int read_proc_pid_stat(unsigned int pid, struct pid_stats *pst, unsigned int *thread_nr, unsigned int tgid);
void read_uptime(unsigned long long *uptime);
void read_meminfo(struct stats_memory *st_memory);
void read_stat_cpu(struct stats_cpu *st_cpu, int nbr, unsigned long long *uptime, unsigned long long *uptime0);

unsigned long free_ram_in_kb(void); /* Free memory in kB */
int read_proc_pid_cmdline(unsigned int pid, struct pid_stats *pst, unsigned int tgid);

#endif /* __linux__ */

#ifdef __cplusplus
}
#endif
