# membwcg

Simulates, in user space, a cgroup subsystem for managing memory 
bandwidth.

Flow:

 1. Creates performance counter using `perf` to watch a container's 
    memory bandwidth usage.
 2. When a container goes above its memory quota, it is frozen until 
    the next period.

## Usage

```bash
docker-run $period $quota $timeout --rm -e VAR=3 img ...
```

The `docker-run` script wraps `docker run` and adds three mandatory 
positional arguments. Option `period` is given in milliseconds (in 
multiples of 100), `quota` in the number of memory operations per 
period and `timeout` in seconds (0 to disable it). Memory usage is 
accounted by tracking the number of `LLC-prefetch-misses` and 
events registered for the container. For example

```bash
docker-run 1000 500 $timeout --rm alpine dd if=/dev/zero of=/dev/null
```

The above specifies that the corresponding container should only use 
500 memory-related operations every 1s. If it goes aboves this 
threshold, the group is frozen and it's not thawed until the next 
period.

## Requirements

We rely on `perf` 3.19+ and being able to write to 
`/sys/fs/cgroup/freezer`.
