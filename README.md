# membwcontrol

Memory bandiwdth control.

```bash
echo $name $quota > /sys/kernel/debug/membw
```

where `$name` refers to a cgroup in the `freezer` group. Quota is the 
number of memory operations allowed per period.
