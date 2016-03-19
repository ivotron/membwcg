# membwcontrol

Memory bandwidth control

Flow:

  * when membwcg runs, it watches a file being created whose content 
    is the id of the container (see `--cidfile`).
  * when it gets a notification that the file it's been created, it 
    reads the container ID and starts a perf counter using `perf` to 
    watch the container's memory usage
  * when a container goes above its memory threshold, it is frozen 
    until the next period.

## Usage

```bash
membwcg -cidfile $file -quota $ops -period $ms
```

File is the file in which docker writes the container ID. Period is 
given in milliseconds and quota in the number of memory operations per 
period. Thus, to get a container's memory bandwidth managed:

```bash
docker run --cidfile /tmp/cidfile ...
```

## Requirements

We rely on `perf` and being able to write to `/sys/fs/cgroup/freezer`.
