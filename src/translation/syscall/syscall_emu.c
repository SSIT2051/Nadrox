/*
 * Syscall Emulation
 * Emulation stubs for syscalls that need translation
 */

#define _GNU_SOURCE
#include "syscall_emu.h"
#include "syscall_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <poll.h>
#include <sys/epoll.h>
#include <linux/futex.h>
#include <signal.h>
#include <time.h>

/* Global state */
static pthread_mutex_t g_addr_space_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_single_addr_space = true;

/* Memory region tracking */
typedef struct mem_region {
    uintptr_t arm_addr;
    uintptr_t x86_addr;
    size_t size;
    int prot;
    int flags;
    int fd;
    off_t offset;
    struct mem_region *next;
} mem_region_t;

static mem_region_t *g_mem_regions = NULL;

/* Add memory region mapping */
static void add_mem_region(uintptr_t arm_addr, uintptr_t x86_addr, size_t size,
                          int prot, int flags, int fd, off_t offset)
{
    pthread_mutex_lock(&g_addr_space_mutex);
    mem_region_t *region = malloc(sizeof(mem_region_t));
    region->arm_addr = arm_addr;
    region->x86_addr = x86_addr;
    region->size = size;
    region->prot = prot;
    region->flags = flags;
    region->fd = fd;
    region->offset = offset;
    region->next = g_mem_regions;
    g_mem_regions = region;
    pthread_mutex_unlock(&g_addr_space_mutex);
}

/* Find memory region */
static mem_region_t *find_mem_region(uintptr_t addr)
{
    pthread_mutex_lock(&g_addr_space_mutex);
    mem_region_t *cur = g_mem_regions;
    while (cur) {
        if (addr >= cur->arm_addr && addr < cur->arm_addr + cur->size) {
            pthread_mutex_unlock(&g_addr_space_mutex);
            return cur;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_addr_space_mutex);
    return NULL;
}

/* Translate address from ARM space to host space */
uintptr_t translate_addr_arm_to_host(uintptr_t arm_addr)
{
    if (!g_single_addr_space) {
        mem_region_t *region = find_mem_region(arm_addr);
        if (region) {
            return region->x86_addr + (arm_addr - region->arm_addr);
        }
    }
    return arm_addr;
}

/* Translate address from host space to ARM space */
uintptr_t translate_addr_host_to_arm(uintptr_t host_addr)
{
    if (!g_single_addr_space) {
        pthread_mutex_lock(&g_addr_space_mutex);
        mem_region_t *cur = g_mem_regions;
        while (cur) {
            if (host_addr >= cur->x86_addr && host_addr < cur->x86_addr + cur->size) {
                uintptr_t result = cur->arm_addr + (host_addr - cur->x86_addr);
                pthread_mutex_unlock(&g_addr_space_mutex);
                return result;
            }
            cur = cur->next;
        }
        pthread_mutex_unlock(&g_addr_space_mutex);
    }
    return host_addr;
}

long emu_mmap(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=addr, r1=len, r2=prot, r3=flags, r4=fd, r5=offset */
    unsigned long addr = regs->r[0];
    unsigned long len = regs->r[1];
    unsigned long prot = regs->r[2];
    unsigned long flags = regs->r[3];
    unsigned long fd = regs->r[4];
    unsigned long offset = regs->r[5];
    
    void *result = NULL;
    
    if (g_single_addr_space && addr != 0) {
        result = mmap((void *)(uintptr_t)addr, len, prot, flags | MAP_FIXED_NOREPLACE, (int)fd, (off_t)offset);
        if (result == MAP_FAILED) {
            result = mmap((void *)(uintptr_t)addr, len, prot, flags, (int)fd, (off_t)offset);
        }
        if (result == MAP_FAILED) {
            result = mmap(NULL, len, prot, flags, (int)fd, (off_t)offset);
        }
    } else {
        result = mmap((void *)(uintptr_t)addr, len, prot, flags, (int)fd, (off_t)offset);
    }
    
    if (result == MAP_FAILED) {
        return -errno;
    }
    
    add_mem_region((uintptr_t)result, (uintptr_t)result, len, (int)prot, (int)flags, (int)fd, (off_t)offset);
    
    return (long)(uintptr_t)result;
}

long emu_munmap(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=addr, r1=len */
    unsigned long addr = regs->r[0];
    unsigned long len = regs->r[1];
    
    int ret = munmap((void *)(uintptr_t)addr, len);
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

long emu_mprotect(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=addr, r1=len, r2=prot */
    unsigned long addr = regs->r[0];
    unsigned long len = regs->r[1];
    unsigned long prot = regs->r[2];
    
    int ret = mprotect((void *)(uintptr_t)addr, len, (int)prot);
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

static void *clone_thread_entry(void *arg)
{
    clone_args_t *args = (clone_args_t *)arg;
    if (!args) return NULL;
    
    void *ret = NULL;
    if (args->fn) {
        ret = (void *)(long)args->fn(args->arg);
    }
    
    free(args);
    return ret;
}

long emu_clone(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=flags, r1=child_stack, r2=ptid, r3=tls, r4=ctid */
    unsigned long flags = regs->r[0];
    unsigned long child_stack = regs->r[1];
    unsigned long ptid = regs->r[2];
    unsigned long tls = regs->r[3];
    unsigned long ctid = regs->r[4];
    
    if (flags & CLONE_THREAD) {
        pthread_t thread;
        clone_args_t *args = malloc(sizeof(clone_args_t));
        args->flags = flags;
        args->fn = NULL;
        args->arg = (void *)(uintptr_t)child_stack;
        args->stack = (void *)(uintptr_t)child_stack;
        args->ptid = (pid_t *)(uintptr_t)ptid;
        args->ctid = (pid_t *)(uintptr_t)ctid;
        args->tls = tls;
        
        int ret = pthread_create(&thread, NULL, clone_thread_entry, args);
        if (ret < 0) {
            free(args);
            return -ret;
        }
        return (long)thread;
    }
    
    pid_t pid = fork();
    if (pid < 0) return -errno;
    return pid;
}

long emu_futex(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=uaddr, r1=op, r2=val, r3=timeout, r4=uaddr2, r5=val3 */
    unsigned long uaddr = regs->r[0];
    int op = (int)regs->r[1];
    unsigned long val = regs->r[2];
    unsigned long timeout = regs->r[3];
    unsigned long uaddr2 = regs->r[4];
    unsigned long val3 = regs->r[5];
    
    void *uaddr_trans = (void *)(uintptr_t)translate_addr_arm_to_host(uaddr);
    void *uaddr2_trans = (void *)(uintptr_t)(uaddr2 ? translate_addr_arm_to_host(uaddr2) : 0);
    
    struct timespec *ts = (struct timespec *)(uintptr_t)(timeout ? timeout : 0);
    
    long ret = syscall(SYS_futex, uaddr_trans, op, val, ts, uaddr2_trans, val3);
    
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

long emu_ioctl(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=fd, r1=request, r2=arg */
    int fd = (int)regs->r[0];
    unsigned long request = regs->r[1];
    unsigned long arg = regs->r[2];
    
    long ret = ioctl(fd, (unsigned long)request, (void *)(uintptr_t)translate_addr_arm_to_host(arg));
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

long emu_fcntl(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=fd, r1=cmd, r2=arg */
    int fd = (int)regs->r[0];
    int cmd = (int)regs->r[1];
    unsigned long arg = regs->r[2];
    
    long ret;
    if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
        struct flock *flock_arg = (struct flock *)(uintptr_t)translate_addr_arm_to_host(arg);
        ret = fcntl(fd, cmd, flock_arg);
    } else {
        ret = fcntl(fd, cmd, (void *)(uintptr_t)arg);
    }
    
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

long emu_poll(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=ufds, r1=nfds, r2=timeout */
    struct pollfd *ufds = (struct pollfd *)(uintptr_t)translate_addr_arm_to_host(regs->r[0]);
    nfds_t nfds = (nfds_t)regs->r[1];
    int timeout = (int)regs->r[2];
    
    long ret = poll(ufds, nfds, timeout);
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

long emu_epoll_create(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    int size = (int)regs->r[0];
    int ret = epoll_create(size);
    if (ret < 0) return -errno;
    return ret;
}

long emu_epoll_ctl(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    int epfd = (int)regs->r[0];
    int op = (int)regs->r[1];
    int fd = (int)regs->r[2];
    struct epoll_event *event = NULL;
    if (regs->r[3]) {
        event = (struct epoll_event *)(uintptr_t)translate_addr_arm_to_host(regs->r[3]);
    }
    int ret = epoll_ctl(epfd, op, fd, event);
    if (ret < 0) return -errno;
    return ret;
}

long emu_epoll_wait(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    int epfd = (int)regs->r[0];
    struct epoll_event *events = (struct epoll_event *)(uintptr_t)translate_addr_arm_to_host(regs->r[1]);
    int maxevents = (int)regs->r[2];
    int timeout = (int)regs->r[3];
    int ret = epoll_wait(epfd, events, maxevents, timeout);
    if (ret < 0) return -errno;
    return ret;
}

/* Main syscall dispatcher */
long syscall_emulate(uint32_t arm_syscall, arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    switch (arm_syscall) {
        case 192: /* mmap */
            return emu_mmap(regs);
        case 191: /* munmap */
            return emu_munmap(regs);
        case 193: /* mprotect */
            return emu_mprotect(regs);
        case 120: /* clone */
            return emu_clone(regs);
        case 240: /* futex */
            return emu_futex(regs);
        case 54:  /* ioctl */
            return emu_ioctl(regs);
        case 55:  /* fcntl */
            return emu_fcntl(regs);
        case 168: /* poll */
            return emu_poll(regs);
        case 257: /* epoll_create */
            return emu_epoll_create(regs);
        case 258: /* epoll_ctl */
            return emu_epoll_ctl(regs);
        case 259: /* epoll_wait */
            return emu_epoll_wait(regs);
        case 173: /* rt_sigreturn */
        case 174: /* rt_sigaction */
            return 0;
        default:
            return -ENOSYS;
    }
}

/* Setup single address space mode */
void syscall_emu_set_single_addr_space(bool enable)
{
    g_single_addr_space = enable;
}
