package main

import (
	"bufio"
	"bytes"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

// flags
var quota = flag.Int("quota",
	500, "quota (number of ops)")
var schedulingPeriod = flag.Int("period",
	1000, "number of milliseconds per period, as a multiple of sample-period")
var samplingPeriod = flag.Int("sampling-period",
	100, "value for perf's -I")
var cid = flag.String("cid",
	"000", "container id to control")
var ops = flag.String("ops",
	"LLC-prefetch-misses", "value for perf's -e")
var verbose = flag.Bool("verbose", false, "whether to print perf output")

// internal variables
var used = 0
var frozen = false
var currentIteration = 0
var linesPerPeriod = 0
var cgfile = ""
var logBuf bytes.Buffer
var logCnt = 0

func updateStatsAndLimit(line string) {
	if *verbose {
		logBuf.WriteString(line + "\n")
	}
	currentIteration++
	logCnt++

	if *verbose && logCnt == 50 {
		fmt.Println(logBuf.String())
		logBuf.Reset()
		logCnt = 0
	}
	if currentIteration == linesPerPeriod {
		used = 0
		currentIteration = 0
		if frozen {
			if err := ioutil.WriteFile(cgfile, []byte("THAWED"), 0644); err != nil {
				if os.IsNotExist(err) {
					// if file is gone, container was removed, so we're done
					os.Exit(0)
				}
				log.Fatalln(err)
			}
			frozen = false
		}

		// no need to update stats since we've just reset counters
		return
	}

	cols := strings.Split(line, ",")
	if len(cols) < 3 {
		return
	}
	i, err := strconv.Atoi(cols[1])
	if err != nil {
		if strings.Contains(cols[1], "<") {
			return
		}
		log.Fatalln(err)
	}
	used += i
	if *verbose {
		logBuf.WriteString(fmt.Sprintf("used: %d\n", used))
	}

	if used > *quota {
		if *verbose {
			logBuf.WriteString("process went above its threshold\n")
		}

		if err := ioutil.WriteFile(cgfile, []byte("FROZEN"), 0644); err != nil {
			if os.IsNotExist(err) {
				// if file is gone, container was removed, so we're done
				os.Exit(0)
			}
			log.Fatalln(err)
		}
		frozen = true
	}
}

func preparePerfCounter() (cmd *exec.Cmd) {
	if math.Mod(float64(*schedulingPeriod), float64(*samplingPeriod)) != 0.0 {
		log.Fatalln(
			fmt.Sprintf(
				"ERROR: scheduling period (%d) must be a multiple of sampling period (%d)",
				*schedulingPeriod,
				*samplingPeriod))
	}

	groups := []string{}
	for _, _ = range strings.Split(*ops, ",") {
		groups = append(groups, *cid)
	}

	cgfile = "/sys/fs/cgroup/freezer/" + *cid + "/freezer.state"
	numOps := len(strings.Split(*ops, ","))
	linesPerPeriod = int(*schedulingPeriod / *samplingPeriod) * numOps

	// for every sample, there are as many lines as number of operations being
	// counted. In other words, every sample is N lines in the output of perf,
	// where N is the number of ops. Thus, there are `numOps` lines times the
	// number of samples per period, which is the schedulingPeriod divided by the
	// samplingPeriod

	cmd = exec.Command("perf", "stat",
		"-a",
		"-x,",
		"-I", strconv.Itoa(*samplingPeriod),
		"-e", *ops,
		"-G", strings.Join(groups, ","))

	if *verbose {
		logBuf.WriteString(strings.Join(cmd.Args, " ") + "\n")
	}

	return
}

func controlBandwidth() {
	cmd := preparePerfCounter()
	stderr, err := cmd.StderrPipe()
	if err != nil {
		log.Fatalln(err)
	}

	scanner := bufio.NewScanner(stderr)
	go func() {
		for scanner.Scan() {
			updateStatsAndLimit(scanner.Text())
		}
	}()

	if err := cmd.Start(); err != nil {
		log.Fatalln(err)
	}
	if err := cmd.Wait(); err != nil {
		log.Fatalln(fmt.Sprintf("ERROR: while executing perf (%s)", err))
	}

	return
}

func init() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
}

func main() {
	flag.Parse()
	controlBandwidth()
}
