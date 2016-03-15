#include "membwcontrol.h"

#include <linux/cgroup.h>
#include <linux/debugfs.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/kernfs.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/slab.h>
#include <linux/string.h>

#define MODULE_NAME "membwcontrol"

MODULE_AUTHOR("Ivo Jimenez, http://cs.ucsc.edu/~ivo");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Memory bandwidth control");
MODULE_VERSION("0.1");

/**************************************************************************
 * Module Types
 **************************************************************************/

struct cgroup_stats {
    long quota;
    long used;
    struct cgroup *cgrp;
    struct perf_event __percpu **counters;
    struct hlist_node hlist;
};

/*************************************************************************
 * Global Variables
 ************************************************************************/

static int period_us = 1000;

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
 * Module parameters
 ************************************************************************/

module_param(period_us, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(period_us, "period in microseconds");

/**************************************************************************
 * main code
 **************************************************************************/

/* adapted from 4.5rc7 */
/*
struct cgroup *get_cgroup_from_path(const char *path)
{
    struct kernfs_node *kn;
    struct cgroup *cgrp;

    kn = kernfs_find_and_get(cgrp_dfl_root.cgrp.kn, path);
    if (kn) {
        if (kernfs_type(kn) == KERNFS_DIR) {
            cgrp = kn->priv;
            css_get(&cgrp->self);
        } else {
            cgrp = ERR_PTR(-ENOTDIR);
        }
        kernfs_put(kn);
    } else {
        cgrp = ERR_PTR(-ENOENT);
    }

    return cgrp;
}
*/

/*
 * overflow callback
 */
// static void oflow(struct perf_event *event,
//           struct perf_sample_data *data, struct pt_regs *regs)
// {
//     int id;
//     u32 cpu = smp_processor_id();
//
//     for (id = 0; id < num_counters; ++id)
//     if (per_cpu(perf_events, cpu)[id] == event)
//         break;
//
//     if (id != num_counters)
//     oprofile_add_sample(regs, id);
//     else
//     pr_warning("oprofile: ignoring spurious overflow "
//         "on cpu %u\n", cpu);
//     return;
// }

// static int create_counter(struct cgroup_stats *stat)
// {
//
//     int ret = 0;
//
//     /*
//     int cpu, ret = 0;
//     struct perf_event *e;
//
//     for_each_online_cpu(cpu) {
//         e = perf_event_create_kernel_counter(
//             &hw_attr, cpu, NULL, &oflow, NULL);
//
//         // then create a perf_cgroup
//         //
//         //
//         // then attach it to the cgroup
//         //
//         //    perf_cgroup_from_task(e->hw.target)
//     }
//
//
//     if (!e)
//         return -1;
//
//     */
//
//     return ret;
// }

// static void remove_counter(void *info)
// {
// }

static DEFINE_HASHTABLE(stats_table, 5);

static struct cgroup_stats *init_stat(struct cgroup* cgrp)
{
    struct cgroup_stats *stats;
    size_t size;

    size = sizeof(struct cgroup_stats);
    stats = (struct cgroup_stats *) kmalloc(size, GFP_KERNEL);

    stats->cgrp = cgrp;

    hash_add(stats_table, &stats->hlist, cgrp->id);

    //create_counter(stat);

    return stats;
}


/*************************************************************************
 * debugfs
 ************************************************************************/

//static struct dentry *bwdir;

static ssize_t membw_limits_write(struct file *filp,
                  const char __user *ubuf,
                  size_t cnt, loff_t *ppos)
{
    struct cgroup_stats *e;
    struct cgroup_stats *stat;
    struct cgroup *cgrp;
    char buf[128];
    char cgrp_path[128];
    char* pbuf;
    char* cgrp_name;
    char* quota_str;
    long quota;
    int buf_size;
    int err;

    // parse given values {
    buf_size = min(cnt, (sizeof(buf) - 1));
    if (strncpy_from_user(buf, ubuf, buf_size) < 0)
        printk(KERN_ALERT "Error copying user string.\n");
        return -EFAULT;
    pbuf = buf;

    cgrp_name = strsep(&pbuf, " ");
    quota_str = strsep(&pbuf, " ");

    if (!quota_str) {
        printk(KERN_ALERT "Unable to read quota value.\n");
        return -EFAULT;
    }

    err = kstrtol(quota_str, 10, &quota);
    if (err < 0) {
        printk(KERN_ALERT "Invalid quota value %s.\n", quota_str);
        return -EFAULT;
    }
    // }

    // find values {
    strcpy(cgrp_path, "freezer/");
    strcpy(cgrp_path + 8, cgrp_name);

    cgrp = cgroup_get_from_path(cgrp_name);

    if (IS_ERR(cgrp)) {
        pr_err("%s:%d (%ld)\n", __func__, __LINE__, PTR_ERR(cgrp));
        printk(KERN_ALERT "Likely non-existent cgroup.\n");
        return -EFAULT;
    }
    // }

    // check if we've seen this group before. If we have, we just
    // update the quota for the group with the provided one
    hash_for_each_possible(stats_table, e, hlist, cgrp->id) {
        if (e->cgrp->id == cgrp->id) {
            e->quota = quota;
            return cnt;
        }
    }

    stat = init_stat(cgrp);

    return cnt;
}

static int membw_limits_show(struct seq_file *m, void *v)
{
    return 0;
}

static int membw_limits_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, membw_limits_show, NULL);
}

static const struct file_operations limits_file = {
    .open    = membw_limits_open,
    .write   = membw_limits_write,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

// static int membwcg_init_debugfs(void)
// {
//     bwdir = debugfs_create_dir("membw", NULL);
//     BUG_ON(!bwdir);
//     debugfs_create_file("limits", 0444, bwdir, NULL, &limits_file);
//     return 0;
// }

/*************************************************************************
 * init/exit
 ************************************************************************/

static int __init init_mod(void) {
    struct cgroup *cgrp;
    struct cgroup_subsys_state *css;

    hw_attr.sample_period = period_us;

    //membwcg_init_debugfs();
    cgrp = cgroup_get_from_path("7:/freezer:/user/1000.user");

    if (IS_ERR(cgrp)) {
        pr_err("%s:%d (%ld)\n", __func__, __LINE__, PTR_ERR(cgrp));
        printk(KERN_ALERT "Likely non-existent cgroup %s.\n", "7:/freezer:/user");
        return -EFAULT;
    }

    css_has_online_children(css);

    return NOTIFY_OK;
}

static void __exit exit_mod(void) {
    // int bkt;
    // struct cgroup_stats *e;
    // struct hlist_node *tmp;

    // debugfs_remove_recursive(bwdir);

    // hash_for_each_safe(stats_table, bkt, tmp, e, hlist) {
    //     //remove_counter();
    //     hash_del(&e->hlist);
    //     kfree(e);
    // }
}

module_init(init_mod);
module_exit(exit_mod);
