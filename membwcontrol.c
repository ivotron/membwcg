#include "membwcontrol.h"

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/perf_event.h>

/**************************************************************************
 * Module Types
 **************************************************************************/

/* per-task accounting */
struct task_stats {
        int id;
        int quota;
        int used;
};

/* global info */
struct membw_stats {
        char arch[3];
        int period;
        struct task_stats task[128];
        struct perf_event counter[128];
};

/*************************************************************************
 Global Variables
************************************************************************/

// parameters
static char *hw_arch = "";
static int period_us = 1000;

// global stats
static struct membw_stats stats;

// debugfs entry
static struct dentry *membwcg_dir;

/*************************************************************************
 Module parameters
************************************************************************/

module_param(hw_arch, charp,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(hw_arch, "hardware type");

module_param(period_us, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(period_us, "period in microseconds");


/*************************************************************************
 * debugfs
 ************************************************************************/

static ssize_t membw_quotas_write(struct file *filp,
                                  const char __user *ubuf,
                                  size_t cnt, loff_t *ppos)
{
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

/**************************************************************************
 * main code
 **************************************************************************/

// static void unload_counter(void *info)
// {
//         struct core_info *cinfo = this_cpu_ptr(core_info);
//         BUG_ON(!cinfo->event);
//
//         /* stop the counter */
//         cinfo->event->pmu->stop(cinfo->event, PERF_EF_UPDATE);
//         cinfo->event->pmu->del(cinfo->event, 0);
//
//         pr_info("LLC bandwidth throttling disabled\n");
// }

/*************************************************************************
 * init/exit
 ************************************************************************/

static int __init _init_module(void) {
        stats.period = period_us;

        // TODO: check that hw_arch is only 3 chars
        strncpy(stats.arch, hw_arch, 3);

        membwcg_init_debugfs();

        return NOTIFY_OK;
}

static void __exit _exit_module(void) {
        debugfs_remove_recursive(membwcg_dir);
        //unload_counter();
}


module_init(_init_module);
module_exit(_exit_module);

MODULE_AUTHOR("Ivo Jimenez, http://cs.ucsc.edu/~ivo");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Memory bandwidth control.");
