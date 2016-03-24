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

Option `period` is given in milliseconds (in multiples of 100) and 
`quota` in the number of memory operations per period. For example

```bash
docker-run 1000 500 $timeout --rm -e VAR=3 img ...
```

The memory operations that are tracked are `LLC-prefetches` and 
`cache-misses`.

## Requirements

We rely on `perf` and being able to write to `/sys/fs/cgroup/freezer`.
