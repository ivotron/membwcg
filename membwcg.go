package main

import (
	"bufio"
	"flag"
	"io/ioutil"
	"log"
	"math"
	"os/exec"
	"strconv"
	"strings"

	"gopkg.in/fsnotify.v1"
)

var quota = flag.Int("quota",
	500, "quota (number of ops)")

var schedulingPeriod = flag.Int("scheduling-period",
	1000, "number of samples that conform a throttle period")

var samplingPeriod = flag.Int("sample-period",
	100, "value for perf's -I")

var cidfile = flag.String("cidfile",
	"/tmp/watch", "path to file that docker writes (--cidfile flag)")

var ops = flag.String("ops",
	"LLC-prefetches,cache-misses", "value for perf's -e")

var cid string
var used = 0
var frozen = false
var currentSampleIteration = 0

func createPerfCounter() (r *bufio.Reader) {
	if math.Mod(float64(*schedulingPeriod), float64(*samplingPeriod)) != 0.0 {
		log.Fatalln("ERROR: scheduling period must be a multiple of sampling period")
	}

	groups := []string{}
	for _, _ = range *ops {
		groups = append(groups, "docker/"+cid)
	}

	cmd := exec.Command("perf", "stat",
		"-a",
		"-x,",
		"-I", strconv.Itoa(*samplingPeriod),
		"-e", *ops,
		"-G", strings.Join(groups, ","))

	stderr, err := cmd.StderrPipe()

	if err != nil {
		log.Fatalln(err)
	}

	cmd.Start()
	r = bufio.NewReader(stderr)

	return
}

func monitorAndLimit(reader *bufio.Reader) {
	cgfile := "/sys/fs/cgroup/freezer/docker/" + cid + "/freezer.state"
	samplesPerSchedulingPeriod := int(*schedulingPeriod / *samplingPeriod)

	for {
		line, _, err := reader.ReadLine()
		if err != nil {
			log.Fatalln(err)
		}

		currentSampleIteration++

		if frozen {
			if currentSampleIteration == samplesPerSchedulingPeriod {
				if err = ioutil.WriteFile(cgfile, []byte("THAWED"), 0644); err != nil {
					log.Fatalln(err)
				}
				used = 0
				frozen = false
				currentSampleIteration = 0
			}

			// no need to update stats:
			// - we've just thawed the cgroup or
			// - we know we went over the limit
			continue
		}

		for _, _ = range *ops {
			l := strings.Split(string(line), ",")
			i, err := strconv.Atoi(l[1])
			if err != nil {
				if strings.Contains(l[1], "<") {
					continue
				}
				log.Fatalln(err)
			}
			used += i
		}

		if used > *quota {
			if err = ioutil.WriteFile(cgfile, []byte("FREEZE"), 0644); err != nil {
				log.Fatalln(err)
			}
			frozen = true
		}

		currentSampleIteration++
	}
}

func controlBandwidth() {
	r := createPerfCounter()

	monitorAndLimit(r)
}

func init() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
}

func main() {

	flag.Parse()

	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		log.Fatalln(err)
	}

	done := make(chan bool)
	go func() {
		for {
			select {
			case event := <-watcher.Events:
				if event.Op&fsnotify.Write == fsnotify.Write {
					buf, err := ioutil.ReadFile(*cidfile)
					if err != nil {
						log.Fatalln(err)
					}
					cid = string(buf)
					controlBandwidth()
				}
			case err := <-watcher.Errors:
				log.Fatalln("error:", err)
			}
		}
	}()

	err = watcher.Add(*cidfile)
	if err != nil {
		log.Fatal(err)
	}
	<-done

}
