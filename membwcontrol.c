#include "membwcontrol.h"

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/perf_event.h>

/**************************************************************************
 * Module Types
 **************************************************************************/

struct cgroup_stats {
        int quota;
        int used;
        struct perf_event __percpu **counters;
};

struct perf_event_attr hw_attr = {
        .type           = PERF_TYPE_HARDWARE,
        .config         = PERF_COUNT_HW_CACHE_MISSES,
        .size           = sizeof(struct perf_event_attr),
        .pinned         = 1,
        .disabled       = 1,
        .exclude_kernel = 1,
        .pinned         = 1,
        .sample_period  = 0,
};

/*************************************************************************
 * Global Variables
 ************************************************************************/

// parameters
static int period_us = 1000;

// global stats
struct cgroup_stats stats[128];

/*************************************************************************
 * Module parameters
 ************************************************************************/

module_param(period_us, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(period_us, "period in microseconds");

/**************************************************************************
 * main code
 **************************************************************************/

/*
 * overflow callback
 */
static void oflow(struct perf_event *event,
                  struct perf_sample_data *data, struct pt_regs *regs)
{
    /*
    int id;
    u32 cpu = smp_processor_id();

    for (id = 0; id < num_counters; ++id)
        if (per_cpu(perf_events, cpu)[id] == event)
            break;

    if (id != num_counters)
        oprofile_add_sample(regs, id);
    else
        pr_warning("oprofile: ignoring spurious overflow "
                "on cpu %u\n", cpu);
    */
    return;
}

static int create_counter(struct cgroup_stats *stat)
{
        int cpu, ret = 0;
        struct perf_event *e;

        for_each_online_cpu(cpu) {
                e = perf_event_create_kernel_counter(
                        &hw_attr, cpu, NULL, &oflow, NULL);

                // then create a perf_cgroup
                //
                //
                // then attach it to the cgroup
                //
                //    perf_cgroup_from_task(e->hw.target)
        }


        if (!e)
                return -1;

        return ret;
}

// static void remove_counter(void *info)
// {
// }

/*************************************************************************
 * debugfs
 ************************************************************************/

// debugfs entry
static struct dentry *membwcg_dir;

static struct cgroup_stats *stats_for_cgroup(char* cgroup_name)
{
    return NULL;
}

static struct cgroup_stats *init_stat(char* cgroup_name)
{
    // kmalloc()
    return NULL;
}

static ssize_t membw_quotas_write(struct file *filp,
                                  const char __user *ubuf,
                                  size_t cnt, loff_t *ppos)
{
        // 0. parse cgroup name and quota
        char cgroup_name[10] = "name";
        int quota = 0;

        // 1. check if cgroup hierarchy exists
        // TODO: we won't check for overlaps with others
        //       but this should definitely be done

        // 2. check if we have stats for the group

        struct cgroup_stats *stat = stats_for_cgroup(cgroup_name);

        if (stat) {
            // we've seen it before, so let's just reassign the quota
            stat->quota = quota;
        } else {
            stat = init_stat(cgroup_name);
            create_counter(stat);
        }

        return cnt;
}

static int membw_quotas_show(struct seq_file *m, void *v)
{
       return 0;
}

static int membw_quotas_open(struct inode *inode, struct file *filp)
{
        return single_open(filp, membw_quotas_show, NULL);
}

static const struct file_operations quotas_dbfs = {
        .open    = membw_quotas_open,
        .write   = membw_quotas_write,
        .read    = seq_read,
        .llseek  = seq_lseek,
        .release = single_release,
};

static int membwcg_init_debugfs(void)
{
        membwcg_dir = debugfs_create_dir("membw", NULL);
        BUG_ON(!membwcg_dir);
        debugfs_create_file("quotas", 0444, membwcg_dir, NULL, &quotas_dbfs);
        return 0;
}

/*************************************************************************
 * init/exit
 ************************************************************************/

static int __init _init_module(void) {
        hw_attr.sample_period = period_us;

        membwcg_init_debugfs();

        return NOTIFY_OK;
}

static void __exit _exit_module(void) {
        debugfs_remove_recursive(membwcg_dir);
        //remove_counter();
}


module_init(_init_module);
module_exit(_exit_module);

MODULE_AUTHOR("Ivo Jimenez, http://cs.ucsc.edu/~ivo");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Memory bandwidth control.");
