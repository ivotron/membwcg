package main

import (
	"bufio"
	"flag"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

var quota = flag.Int("quota", 10000, "quota (number of ops)")
var sampleCnt = flag.Int("num-samples", 10, "number of samples that conform a throttle period")
var samplePeriod = flag.String("sample-period", "100", "value for perf's -I")
var cgroup = flag.String("cgroup", "docker", "cgroup path")
var ops = flag.String("ops", "LLC-prefetches,cache-misses", "value for perf's -e")

func main() {

	flag.Parse()

	cgfile := "/sys/fs/cgroup/freezer/" + cgroup

	// check that cgroup folder exists
	if _, err := os.Stat(cgfile); err != nil {
		log.Fatal(err)
	}

	cmd := exec.Command("perf",
		"-I", samplePeriod,
		"-e", ops,
		"-x", ",",
		"-G", "/freezer/"+cgroup)

	stdout, err := cmd.StdoutPipe()

	if err != nil {
		log.Fatal(err)
	}

	cmd.Start()
	r := bufio.NewReader(stdout)

	used := 0
	currentSampleIteration := 0
	frozen := false
	for {

		line, _, err := r.ReadLine()
		if err != nil {
			log.Fatal(err)
		}

		currentSampleIteration++

		if frozen {
			if currentSampleIteration == *sampleCnt {
				if err = ioutil.WriteFile(cgfile, []byte("THAWED"), 0644); err != nil {
					log.Fatal(err)
				}
				used = 0
				frozen = false
				currentSampleIteration = 0
			}

			// no need to update stats
			continue
		}

		for _, _ = range ops {
			l := strings.Split(string(line), ",")
			i, err := strconv.Atoi(l[1])
			if err != nil {
				log.Fatal(err)
			}
			used += i
		}

		if used > *quota {
			if err = ioutil.WriteFile(cgfile, []byte("FREEZE"), 0644); err != nil {
				log.Fatal(err)
			}
			//echo FREEZE > /sys/fs/freezer/
			frozen = true
		}

		*sampleCnt++

	}
}
