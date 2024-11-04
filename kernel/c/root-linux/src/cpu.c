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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "cpu.h"

#ifdef __linux__

#include <dirent.h>
#include <unistd.h>
#include <pwd.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/*
 ***************************************************************************
 * Get number of clock ticks per second.
 ***************************************************************************
 */
unsigned int get_HZ(void)
{
    long ticks = 0;
    unsigned int hz;

#if defined(__linux__)
    if ((ticks = sysconf(_SC_CLK_TCK)) == -1) {
        perror("sysconf");
    }
#endif
    hz = (unsigned int) ticks;
    return hz;
}

/*
 ***************************************************************************
 * Get page shift in kB.
 ***************************************************************************
 */
/* Number of bit shifts to convert pages to kB */
unsigned int get_kb_shift(void)
{
    unsigned int kb_shift;
    int shift = 0;
    long size = 0;

    /* One can also use getpagesize() to get the size of a page */
#if defined(__linux__)
    if ((size = sysconf(_SC_PAGESIZE)) == -1) {
        perror("sysconf");
    }
#endif
    size >>= 10;    /* Assume that a page has a minimum size of 1 kB */

    while (size > 1) {
        shift++;
        size >>= 1;
    }

    kb_shift = (unsigned int) shift;
    return kb_shift;
}

/*
 ***************************************************************************
 * Read CPU statistics and machine uptime.
 *
 * IN:
 * @st_cpu  Structure where stats will be saved.
 * @nbr     Total number of CPU (including cpu "all").
 *
 * OUT:
 * @st_cpu  Structure with statistics.
 * @uptime  Machine uptime multiplied by the number of processors.
 * @uptime0 Machine uptime. Filled only if previously set to zero.
 ***************************************************************************
 */
void read_stat_cpu(struct stats_cpu *st_cpu, int nbr,
           unsigned long long *uptime, unsigned long long *uptime0)
{
    FILE *fp;
    struct stats_cpu *st_cpu_i;
    struct stats_cpu sc;
    char line[8192];
    int proc_nb;

    if ((fp = fopen(STAT, "r")) == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "cpu ", 4)) {

            /*
             * All the fields don't necessarily exist,
             * depending on the kernel version used.
             */
            memset(st_cpu, 0, STATS_CPU_SIZE);

            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &st_cpu->cpu_user,
                   &st_cpu->cpu_nice,
                   &st_cpu->cpu_sys,
                   &st_cpu->cpu_idle,
                   &st_cpu->cpu_iowait,
                   &st_cpu->cpu_hardirq,
                   &st_cpu->cpu_softirq,
                   &st_cpu->cpu_steal,
                   &st_cpu->cpu_guest,
                   &st_cpu->cpu_guest_nice);

            /*
             * Compute the uptime of the system in jiffies (1/100ths of a second
             * if HZ=100).
             * Machine uptime is multiplied by the number of processors here.
             *
             * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
             * already include them.
             */
            *uptime = st_cpu->cpu_user + st_cpu->cpu_nice    +
                st_cpu->cpu_sys    + st_cpu->cpu_idle    +
                st_cpu->cpu_iowait + st_cpu->cpu_hardirq +
                st_cpu->cpu_steal  + st_cpu->cpu_softirq;
        }

        else if (!strncmp(line, "cpu", 3)) {
            if (nbr > 1) {
                /* All the fields don't necessarily exist */
                memset(&sc, 0, STATS_CPU_SIZE);
                /*
                 * Read the number of jiffies spent in the different modes
                 * (user, nice, etc) for current proc.
                 * This is done only on SMP machines.
                 */
                sscanf(line + 3, "%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &proc_nb,
                    &sc.cpu_user,
                    &sc.cpu_nice,
                    &sc.cpu_sys,
                    &sc.cpu_idle,
                    &sc.cpu_iowait,
                    &sc.cpu_hardirq,
                    &sc.cpu_softirq,
                    &sc.cpu_steal,
                    &sc.cpu_guest,
                    &sc.cpu_guest_nice
                );

                if (proc_nb < (nbr - 1)) {
                    st_cpu_i = st_cpu + proc_nb + 1;
                    *st_cpu_i = sc;
                }
                /*
                 * else additional CPUs have been dynamically registered
                 * in /proc/stat.
                 */

                if (!proc_nb && !*uptime0) {
                    /*
                     * Compute uptime reduced to one proc using proc#0.
                     * Done if /proc/uptime was unavailable.
                     *
                     * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
                     * already include them.
                     */
                    *uptime0 = sc.cpu_user + sc.cpu_nice  +
                        sc.cpu_sys     + sc.cpu_idle  +
                        sc.cpu_iowait  + sc.cpu_steal +
                        sc.cpu_hardirq + sc.cpu_softirq;
                }
            }
        }
    }

    fclose(fp);
}

/*
 ***************************************************************************
 * Read interrupts statistics from /proc/stat.
 *
 * IN:
 * @st_irq  Structure where stats will be saved.
 * @nbr     Number of interrupts to read, including the total number
 *      of interrupts.
 *
 * OUT:
 * @st_irq  Structure with statistics.
 ***************************************************************************
 */
void read_stat_irq(struct stats_irq *st_irq, int nbr)
{
    FILE *fp;
    struct stats_irq *st_irq_i;
    char line[8192];
    int i, pos;

    if ((fp = fopen(STAT, "r")) == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "intr ", 5)) {
            /* Read total number of interrupts received since system boot */
            sscanf(line + 5, "%llu", &st_irq->irq_nr);
            pos = strcspn(line + 5, " ") + 5;

            for (i = 1; i < nbr; i++) {
                st_irq_i = st_irq + i;
                sscanf(line + pos, " %llu", &st_irq_i->irq_nr);
                pos += strcspn(line + pos + 1, " ") + 1;
            }
        }
    }

    fclose(fp);
}

/*
 ***************************************************************************
 * Read memory statistics from /proc/meminfo.
 *
 * IN:
 * @st_memory   Structure where stats will be saved.
 *
 * OUT:
 * @st_memory   Structure with statistics.
 ***************************************************************************
 */
void read_meminfo(struct stats_memory *st_memory)
{
    FILE *fp;
    char line[128];

    if ((fp = fopen(MEMINFO, "r")) == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "MemTotal:", 9)) {
            /* Read the total amount of memory in kB */
            sscanf(line + 9, "%lu", &st_memory->tlmkb);
        }
        else if (!strncmp(line, "MemFree:", 8)) {
            /* Read the amount of free memory in kB */
            sscanf(line + 8, "%lu", &st_memory->frmkb);
        }
        else if (!strncmp(line, "Buffers:", 8)) {
            /* Read the amount of buffered memory in kB */
            sscanf(line + 8, "%lu", &st_memory->bufkb);
        }
        else if (!strncmp(line, "Cached:", 7)) {
            /* Read the amount of cached memory in kB */
            sscanf(line + 7, "%lu", &st_memory->camkb);
        }
        else if (!strncmp(line, "SwapCached:", 11)) {
            /* Read the amount of cached swap in kB */
            sscanf(line + 11, "%lu", &st_memory->caskb);
        }
        else if (!strncmp(line, "Active:", 7)) {
            /* Read the amount of active memory in kB */
            sscanf(line + 7, "%lu", &st_memory->activekb);
        }
        else if (!strncmp(line, "Inactive:", 9)) {
            /* Read the amount of inactive memory in kB */
            sscanf(line + 9, "%lu", &st_memory->inactkb);
        }
        else if (!strncmp(line, "SwapTotal:", 10)) {
            /* Read the total amount of swap memory in kB */
            sscanf(line + 10, "%lu", &st_memory->tlskb);
        }
        else if (!strncmp(line, "SwapFree:", 9)) {
            /* Read the amount of free swap memory in kB */
            sscanf(line + 9, "%lu", &st_memory->frskb);
        }
        else if (!strncmp(line, "Dirty:", 6)) {
            /* Read the amount of dirty memory in kB */
            sscanf(line + 6, "%lu", &st_memory->dirtykb);
        }
        else if (!strncmp(line, "Committed_AS:", 13)) {
            /* Read the amount of commited memory in kB */
            sscanf(line + 13, "%lu", &st_memory->comkb);
        }
        else if (!strncmp(line, "AnonPages:", 10)) {
            /* Read the amount of pages mapped into userspace page tables in kB */
            sscanf(line + 10, "%lu", &st_memory->anonpgkb);
        }
        else if (!strncmp(line, "Slab:", 5)) {
            /* Read the amount of in-kernel data structures cache in kB */
            sscanf(line + 5, "%lu", &st_memory->slabkb);
        }
        else if (!strncmp(line, "KernelStack:", 12)) {
            /* Read the kernel stack utilization in kB */
            sscanf(line + 12, "%lu", &st_memory->kstackkb);
        }
        else if (!strncmp(line, "PageTables:", 11)) {
            /* Read the amount of memory dedicated to the lowest level of page tables in kB */
            sscanf(line + 11, "%lu", &st_memory->pgtblkb);
        }
        else if (!strncmp(line, "VmallocUsed:", 12)) {
            /* Read the amount of vmalloc area which is used in kB */
            sscanf(line + 12, "%lu", &st_memory->vmusedkb);
        }
    }

    fclose(fp);
}

/*
 ***************************************************************************
 * Read machine uptime, independently of the number of processors.
 *
 * OUT:
 * @uptime  Uptime value in jiffies.
 ***************************************************************************
 */
void read_uptime(unsigned long long *uptime)
{
    FILE *fp;
    char line[128];
    unsigned long up_sec, up_cent;

    if ((fp = fopen(UPTIME, "r")) == NULL)
        return;

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return;
    }

    sscanf(line, "%lu.%lu", &up_sec, &up_cent);
    *uptime = (unsigned long long) up_sec * get_HZ() +
              (unsigned long long) up_cent * get_HZ() / 100;

    fclose(fp);

}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/stat.
 *
 * IN:
 * @pid     Process whose stats are to be read.
 * @pst     Pointer on structure where stats will be saved.
 * @tgid    If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst     Pointer on structure where stats have been saved.
 * @thread_nr   Number of threads of the process.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************
 */
int read_proc_pid_stat(unsigned int pid, struct pid_stats *pst,
               unsigned int *thread_nr, unsigned int tgid)
{
    int fd, sz, rc, commsz;
    char filename[128];
    static char buffer[1024 + 1];
    char *start, *end;

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_STAT, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_STAT, pid);
    }

    if ((fd = open(filename, O_RDONLY)) < 0)
        /* No such process */
        return 1;

    sz = read(fd, buffer, 1024);
    close(fd);
    if (sz <= 0)
        return 1;
    buffer[sz] = '\0';

    if ((start = strchr(buffer, '(')) == NULL)
        return 1;
    start += 1;
    if ((end = strrchr(start, ')')) == NULL)
        return 1;
    commsz = end - start;
    if (commsz >= MAX_COMM_LEN)
        return 1;
    memcpy(pst->comm, start, commsz);
    pst->comm[commsz] = '\0';
    start = end + 2;

    rc = sscanf(start,
            "%*s %*d %*d %*d %*d %*d %*u %llu %llu"
            " %llu %llu %llu %llu %lld %lld %*d %*d %u %*u %*d %llu %llu"
            " %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u"
            " %*u %u %u %u %llu %llu %lld\n",
            &pst->minflt, &pst->cminflt, &pst->majflt, &pst->cmajflt,
            &pst->utime,  &pst->stime, &pst->cutime, &pst->cstime,
            thread_nr, &pst->vsz, &pst->rss, &pst->processor,
            &pst->priority, &pst->policy,
            &pst->blkio_swapin_delays, &pst->gtime, &pst->cgtime);

    if (rc < 15)
        return 1;

    if (rc < 17) {
        /* gtime and cgtime fields are unavailable in file */
        pst->gtime = pst->cgtime = 0;
    }

    /* Convert to kB */
    pst->vsz >>= 10;
    pst->rss = PG_TO_KB(pst->rss);

    pst->pid = pid;
    pst->tgid = tgid;
    return 0;
}

/*
 *****************************************************************************
 * Read stats from /proc/#[/task/##]/status.
 *
 * IN:
 * @pid     Process whose stats are to be read.
 * @pst     Pointer on structure where stats will be saved.
 * @tgid    If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst     Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 *****************************************************************************
 */
int read_proc_pid_status(unsigned int pid, struct pid_stats *pst,
             unsigned int tgid)
{
    FILE *fp;
    char filename[128], line[256];

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_STATUS, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_STATUS, pid);
    }

    if ((fp = fopen(filename, "r")) == NULL)
        /* No such process */
        return 1;

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "Uid:", 4)) {
            sscanf(line + 5, "%u", &pst->uid);
        }
        else if (!strncmp(line, "Threads:", 8)) {
            sscanf(line + 9, "%u", &pst->threads);
        }
        else if (!strncmp(line, "voluntary_ctxt_switches:", 24)) {
            sscanf(line + 25, "%lu", &pst->nvcsw);
        }
        else if (!strncmp(line, "nonvoluntary_ctxt_switches:", 27)) {
            sscanf(line + 28, "%lu", &pst->nivcsw);
        }
    }

    fclose(fp);

    pst->pid = pid;
    pst->tgid = tgid;
    return 0;
}

/*
  *****************************************************************************
  * Read information from /proc/#[/task/##}/smaps.
  *
  * @pid        Process whose stats are to be read.
  * @pst        Pointer on structure where stats will be saved.
  * @tgid       If !=0, thread whose stats are to be read.
  *
  * OUT:
  * @pst        Pointer on structure where stats have been saved.
  *
  * RETURNS:
  * 0 if stats have been successfully read, and 1 otherwise.
  *****************************************************************************
  */
int read_proc_pid_smap(unsigned int pid, struct pid_stats *pst, unsigned int tgid)
{
    FILE *fp;
    char filename[128], line[256];
    int state = 0;

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_SMAP, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_SMAP, pid);
    }

    if ((fp = fopen(filename, "rt")) == NULL)
        /* No such process */
        return 1;

    while ((state < 3) && (fgets(line, sizeof(line), fp) != NULL)) {
        switch (state) {
            case 0:
                if (strstr(line, "[stack]")) {
                    state = 1;
                }
                break;
            case 1:
                if (strstr(line, "Size:")) {
                    sscanf(line + sizeof("Size:"), "%lu", &pst->stack_size);
                    state = 2;
                }
                break;
            case 2:
                if (strstr(line, "Referenced:")) {
                    sscanf(line + sizeof("Referenced:"), "%lu", &pst->stack_ref);
                    state = 3;
                }
                break;
        }
    }

    fclose(fp);

    pst->pid = pid;
    pst->tgid = tgid;
    return 0;
}

/*
 *****************************************************************************
 * Read process command line from /proc/#[/task/##]/cmdline.
 *
 * IN:
 * @pid     Process whose command line is to be read.
 * @pst     Pointer on structure where command line will be saved.
 * @tgid    If !=0, thread whose command line is to be read.
 *
 * OUT:
 * @pst     Pointer on structure where command line has been saved.
 *
 * RETURNS:
 * 0 if command line has been successfully read (even if the /proc/.../cmdline
 * is just empty), and 1 otherwise (the process has terminated).
 *****************************************************************************
 */
int read_proc_pid_cmdline(unsigned int pid, struct pid_stats *pst,
              unsigned int tgid)
{
    FILE *fp;
    char filename[128], line[MAX_CMDLINE_LEN];
    size_t len;
    int i;

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_CMDLINE, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_CMDLINE, pid);
    }

    if ((fp = fopen(filename, "r")) == NULL)
        /* No such process */
        return 1;

    memset(line, 0, MAX_CMDLINE_LEN);

    len = fread(line, 1, MAX_CMDLINE_LEN - 1, fp);
    fclose(fp);

    for (i = 0; i < len; i++) {
        if (line[i] == '\0') {
            line[i] = ' ';
        }
    }

    if (len) {
        strncpy(pst->cmdline, line, MAX_CMDLINE_LEN - 1);
        pst->cmdline[MAX_CMDLINE_LEN - 1] = '\0';
    }
    else {
        /* proc/.../cmdline was empty */
        pst->cmdline[0] = '\0';
    }
    return 0;
}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/io.
 *
 * IN:
 * @pid     Process whose stats are to be read.
 * @pst     Pointer on structure where stats will be saved.
 * @tgid    If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst     Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read.
 * Also returns 0 if current process has terminated or if its io file
 * doesn't exist, but in this case, set process' F_NO_PID_IO flag to
 * indicate that I/O stats should no longer be read for it.
 ***************************************************************************
 */
int read_proc_pid_io(unsigned int pid, struct pid_stats *pst,
             unsigned int tgid)
{
    FILE *fp;
    char filename[128], line[256];

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_IO, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_IO, pid);
    }

    if ((fp = fopen(filename, "r")) == NULL) {
        /* No such process... or file non existent! */
        pst->flags |= F_NO_PID_IO;
        /*
         * Also returns 0 since io stats file doesn't necessarily exist,
         * depending on the kernel version used.
         */
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "read_bytes:", 11)) {
            sscanf(line + 12, "%llu", &pst->read_bytes);
        }
        else if (!strncmp(line, "write_bytes:", 12)) {
            sscanf(line + 13, "%llu", &pst->write_bytes);
        }
        else if (!strncmp(line, "cancelled_write_bytes:", 22)) {
            sscanf(line + 23, "%llu", &pst->cancelled_write_bytes);
        }
    }

    fclose(fp);

    pst->pid = pid;
    pst->tgid = tgid;
    pst->flags &= ~F_NO_PID_IO;
    return 0;
}

/*
 ***************************************************************************
 * Count number of file descriptors in /proc/#[/task/##]/fd directory.
 *
 * IN:
 * @pid     Process whose stats are to be read.
 * @pst     Pointer on structure where stats will be saved.
 * @tgid    If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst     Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read.
 * Also returns 0 if current process has terminated or if we cannot read its
 * fd directory, but in this case, set process' F_NO_PID_FD flag to
 * indicate that fd directory couldn't be read.
 ***************************************************************************
 */
int read_proc_pid_fd(unsigned int pid, struct pid_stats *pst,
             unsigned int tgid)
{
    DIR *dir;
    struct dirent *drp;
    char filename[128];

    if (tgid) {
        snprintf(filename, sizeof(filename), TASK_FD, tgid, pid);
    }
    else {
        snprintf(filename, sizeof(filename), PID_FD, pid);
    }

    if ((dir = opendir(filename)) == NULL) {
        /* Cannot read fd directory */
        pst->flags |= F_NO_PID_FD;
        return 0;
    }

    pst->fd_nr = 0;

    /* Count number of entries if fd directory */
    while ((drp = readdir(dir)) != NULL) {
        if (isdigit(((unsigned char *)drp->d_name)[0])) {
            (pst->fd_nr)++;
        }
    }

    closedir(dir);

    pst->pid = pid;
    pst->tgid = tgid;
    pst->flags &= ~F_NO_PID_FD;
    return 0;
}

/*
 ***************************************************************************
 *  Total memory in kB
 ***************************************************************************
 */
unsigned long total_ram_in_kb(void)
{
    unsigned long tlmkb;        /* Total memory in kB */
    struct stats_memory st_mem;

    memset(&st_mem, 0, STATS_MEMORY_SIZE);
    read_meminfo(&st_mem);
    tlmkb = st_mem.tlmkb;
    return tlmkb;
}

/*
 ***************************************************************************
 *  Free memory in kB
 ***************************************************************************
 */
unsigned long free_ram_in_kb(void)
{
    struct stats_memory st_mem;

    memset(&st_mem, 0, STATS_MEMORY_SIZE);
    read_meminfo(&st_mem);

    return st_mem.frmkb + st_mem.camkb; // Include cache memory too
}

/*
 ***************************************************************************
 *  Cpu usage
 ***************************************************************************
 */
int cpu_usage(unsigned int pid, uint64_t *system_time, uint64_t *process_time)
{
    struct stats_cpu st_cpu;
    unsigned long long uptime;
    unsigned long long uptime0=0;
    read_stat_cpu(&st_cpu, 0, &uptime, &uptime0);

    if(system_time) {
        *system_time = uptime;
    }

    struct pid_stats pst;
    unsigned int thread_nr;
    read_proc_pid_stat(pid, &pst, &thread_nr, 0);

    uint64_t uptime_pid = pst.utime + pst.stime + pst.cutime + pst.cstime;
    if(process_time) {
        *process_time = uptime_pid;
    }

    return 0;
}

/*
 ***************************************************************************
 *  Current process memory in kB
 ***************************************************************************
 */
unsigned long proc_vmem_in_kb(unsigned int pid)
{
    struct pid_stats pst = {0};
    unsigned int thread_nr;
    read_proc_pid_stat(pid, &pst, &thread_nr, 0);

    return pst.vsz;
}

#else /* WIN32 */

/*
 ***************************************************************************
 *  Free memory in kB
 ***************************************************************************
 */
unsigned long free_ram_in_kb(void)
{
    // TODO
    return 0;
}

/*
 ***************************************************************************
 *  Cpu usage
 ***************************************************************************
 */
int cpu_usage(unsigned int pid, uint64_t *system_time, uint64_t *process_time)
{
    return 0;
}

#endif /* WIN32 */
