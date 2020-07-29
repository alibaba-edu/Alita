# Alita

This repository contains the source code for a research paper that was submitted for publication at the International Conference for [High Performance Computing, Networking, Storage, and Analysis](https://sc20.supercomputing.org/) (SC20). 

## What is Alita?

Alita is a holistic runtime system that improves the performance of normal applications by only throttling the resource usage of the polluters.

The public cloud abstracts a host machine into multiple virtual machines (instances) and delivers them to customers through virtualization technology. Resources are shared among multiple virtual machines in computing, IO, network and other dimensions. When the overall load of the machine is high, competition for shared resources often occurs.

A tenant is able to seriously degrade the performance of its neighbors on the same node through the shared memory bus, last level cache/memory bandwidth, and power. To eliminate such unfairness, we propose Alita, a runtime system that is comprised of an online interference identifier and an adaptive interference eliminator. The interference identifier monitors the hardware and system-level event statistics to identify the resource polluters. The eliminator improves the performance of normal applications by only throttling the resource usage of the polluters. Specifically, Alita adopts bus lock sparsification, bias LLC/bandwidth isolation, and selective power throttling to throttle the resource usage of the polluters. 

Our results on an experimental platform and an in-production Cloud with 30,000 nodes show that Alita significantly improves the performance of VMs at co-location in presence of resource polluters, based on system level knowledge.

## Working with Alita

### Build Alita

The current version of Alita contains three modules including the shared memory bus, last level cache/memory bandwidth, and power isolation. Run the following commands to build them separately:

- Split Lock Isolation

    g++ test_split_lock_iso.cpp  -o test_split_lock_iso && sudo ./test_split_lock_iso

- LLC & Memory Bandwidth Isolation

    g++ test_mem_contention_iso.cpp -o test_mem_contention_iso && sudo ./test_mem_contention_iso

- Power Isolation

    g++ test_power_iso.cpp -o test_power_iso && sudo ./test_power_iso
    or
	g++ test_power_iso_beta.cpp -o test_power_iso_beta && sudo ./test_power_iso_beta

### Run Alita

We are releasing code to isolate resources on a server with Intel Xeon Platinum 8163 CPUs. Note that this is not the exact code that was used in production  (the original code was written in Python, and had some additional complexity), but this code does perform as described in the paper. The hard coded parameters could be changed in source code as necessary.

## Evaluation of Alita

### Build Benchmarks

To perform the fine-grain exploration on the effects of various resource contentions, we use three programs that generate frequent split locks, use large LLC space, and consume high power as the `resource polluters` (`bus polluter`, `LLC polluter`, and `power polluter`):

- `Bus polluter`: [splitlock](./splitlock.c)
- `LLC polluter`: [Cachebench V2.1](http://icl.cs.utk.edu/llcbench/cachebench.html)
- `Power polluter`: [Mprime v29.8,build 6](https://www.mersenne.org/download/)
- [Linpack V1.0.0](https://www.top500.org/project/linpack/)

To evaluate Alta, we use representative workloads from **Parsec benchmark suite** and **TailBench** that includes both the scientific workloads and latency-sensitive Internet services as the benchmarks. Parsec focuses on emerging workloads and was designed to be representative of shared-memory programs for multiprocessors. Tailbench focuses on emerging user-facing Internet services that have stringent latency requirement.  *Xpian* (xp) and *shore* (sh) in the TailBench are web service and database workloads that are widely-used in real Clouds. We choose these benchmarks to reveal the real-system workloads.

- [Parsec Suite V2.1](https://parsec.cs.princeton.edu/)
- [TailBench Suite V0.9](http://tailbench.csail.mit.edu/)

Please refer to the documentations of each benchmark for more details.

### Setup Envrimont

In each test, we deploy each resource polluter in a VM, and co-located it with the normal VMs. %We refer the two VMs as the *attacker* and the *victim* respectively. We co-locate a resource polluter with a benchmark in each test. If more benchmarks present, the resource polluter affects more VMs and the analysis is valid.

Create a *attacker* and a *victim* virtual machine(VM) from an XML file:

```shell
virsh create attacker.xml
virsh create victim.xml
```

Please refer to [Domain XML format](https://libvirt.org/formatdomain.html) for more details.

### Evaluation

#### Alleviating Bus Lock Contention

In this experiment, we co-run each benchmark and a bus polluter on the different sockets of a node, and measure the normalized execution time of the benchmarks. Both the bus polluter and the benchmark use 4 cores here.

Run `Bus polluter` to generate split locks in the *attacker* VM:

```shell
splitlock
```

Run workloads from Parsec Suite to evaluate its performance in *victim* VM:

```shell
parsecmgmt -a run -x pre -i $input -p $workload 2>&1 | tee  ./$data/${workload}.log
```

We use `native` as `$input` which likes real inputs and execution time as the metrics. The execution time is collected in `${workload}.log`. And `canneal.log` is like this:

```shell
[PARSEC] Benchmarks to run:  canneal

[PARSEC] [========== Running benchmark canneal ==========]
[PARSEC] Deleting old run directory.
[PARSEC] Setting up run directory.
[PARSEC] Unpacking benchmark input 'native'.
2500000.nets
[PARSEC] Running 'time /path/canneal/inst/amd64-linux.gcc.pre/bin/canneal 1 15000 2000 2500000.nets 6000':
[PARSEC] [---------- Beginning of output ----------]
PARSEC Benchmark Suite Version 2.1
Threadcount: 1
15000 swaps per temperature step
start temperature: 2000
netlist filename: 2500000.nets
number of temperature steps: 6000
locs created
locs assigned
Just saw element: 100000
...
Just saw element: 2500000
netlist created. 2500000 elements.
Final routing is: 9.03315e+09

real    5m28.091s
user    5m27.200s
sys     0m0.850s
[PARSEC] [----------    End of output    ----------]
[PARSEC] Done.
```

Run workloads from TailBench Suite to evaluate its performance in *victim* VM:

```shell
/path/tailbench/$workload/run_networked.sh 2>&1 | tee ${workload}.log
```

The output is collected in `${workload}.log`. And `xapian.log` is like this:

``` shell
849 1.211 2.422 3.656 10.325 24.577
840 0.789 0.891 2.703 3.344 5.972
856 0.750 0.929 2.718 3.816 5.483
820 0.726 0.904 2.673 3.474 4.253
860 0.829 0.994 2.846 4.512 6.522
884 0.819 0.982 2.975 4.016 5.146
860 0.931 1.063 3.232 4.679 5.960
824 0.733 0.887 2.600 3.569 4.436
716 0.658 0.864 2.558 3.229 3.976
820 0.868 1.157 3.208 5.682 8.173
```

Each line includes tps/qps, average latency(ms), variance, 95th latency(ms), 99th latency(ms) and max latency(ms). We use average latency as the metric of performance.

Run Alita with split lock isolation on host(tuning the parameters as necessary):

```shell
test_split_lock_iso
```

Run `pcm-memory` to monitor memory bandwidth (per-channel and per-DRAM DIMM rank) on host:

```shell
pcm-memory.x 1 -csv="$workload.log"
```

`pcm-memory` is one of PCM Tools which provides a number of command-line utilities for real-time monitoring, please refer [Processor Counter Monitor (PCM)](https://github.com/opcm/pcm) for more details.

#### Alleviating LLC Contention

In this experiment, we use three VMs (two attacker VMs and one victim VM) to show the effectiveness of Alita in the complex LLC contention scenario. The benchmarks running in victim VM and the polluter running in two attacker VMs use four cores each in the same socket.

Run `LLC polluter` to generate split locks in the *attacker* VM:

```shell
cachebench  -m 29 -e 1 -x 2 -d 5 -w
```

Alita is compared with two policies including *Default policy* and *Static policy*. The Default policy shows the performance when LLC is freely shared and the Static policy shows the performance when the LLC is statically allocated to the VMs proportionally to their core numbers.

##### Setup Static policy
A user has a host machine running 3 guest VM's with 4 cores assigned to each guest.

Attacker VM1 - cores 0-3
Attacker VM2 - cores 4-7
Victim VM3 - cores 8-11

There are 11 exclusive LLC ways in  Intel Xeon Platinum 8163 CPUs.Attacker VM1(b111,1000,0000, 0x780) and Attacker VM2(b000,0111,1000, 0x78) will be assigned 4 exclusive LLC ways, respectively. Victim VM3(b000,0000,0111, 0x7) is assigned 3 ways.

- First, set the 3 COS bitmasks for each VM:

    ```shell
    pqos -e "llc:1=0x780;llc:2=0x78;llc:3=0x7;"
    ```

- Next, associate each COS with the cores where each VM is running:
  
    ```shell
    pqos -a "llc:1=0-3;llc:2=4-7;llc:3=8-11;"
    ```

VM1 and VM2 now have exclusive access to 4 LLC ways, VM3 has exclusive access to 4 ways. All other cores have access to all other ways. Please refer [intel CMT CAT](https://github.com/intel/intel-cmt-cat) for more details.

Run Alita with LLC isolation on host(tuning the parameters as necessary):

```shell
test_mem_contention_iso
```

#### Alleviating Power Contention

In the power test, two virtual machines need to be started first, which are the attacker and the victim respectively. Among them, attacker allocates more cores and victim allocates fewer cores.

Start VMs

    sudo virsh create attacker.xml
    sudo virsh create victim.xml   

Check VMs list

    sudo virsh list

Log in attacker

    sudo ssh -p <PORT_attacker> root@localhost

Before using mprime as a power polluter process, we need to configure parameters first. In this experiment, we use 44 vcpus to run mprime multithreading in the attacker VM, and 4 vcpus in the victim VM.

Edit linpack

    cd linpack

    vim lininput_xeon64
        1 # number of tests
        45000 # problem sizes
        45000 # leading dimensions
        1000 # times to run a test
        1 # alignment values (in KBytes)

    vim run.sh
        arch=xeon64
        {
            taskset -ac 0-43 ./xlinpack_$arch lininput_$arch
        }
    
Run linpack

    ./run.sh

Edit mprime

    cd mprime
    cat readme.txt

    vim local.txt
        WorkerThreads = 22
        CoresPerTest = 2
        NumCPUs = 44
        CpuNumHyperthreads = 1
        Affinity = 0-43
    
    vim run_mprime.sh
        ./mprime -t
    
    vim stop_mprime.sh
        kill -9 $(pgrep -f mprime)

We can also consider encapsulating the running instructions of mrpime into a script to call directly.

Run mprime 

    ./run_mprime.sh

In the victim VM, we use parsec as a victim process. After installing parsec, we can run different types of benchmarks in parsec through the parsecmmt instruction.

Log in victim

    sudo ssh -p <PORT_VICTIM> root@localhost

Run parsec

    parsec=('blackscholes' 'bodytrack' 'facesim' 'x264' 'raytrace' 'fluidanimate' 'canneal' 'streamcluster')

    for i in ${parsec[@]};do

        taskset -ac 0 /root/parsec-2.1/bin/parsecmgmt -a run -x pre -i native -p $i 2>&1 | tee  $filedir/log

    done

Next, we will introduce the experimental test process. To verify the power isolation program power_iso and power_iso_beta, first, let the victim process run separately in the victim VM for a period of time until it is stable, and then start the power router mprime process in the attacker VM. Through turbostat, it can be observed that the power consumption of socket increases and the operation efficiency of victim is seriously affected. At this time, the power isolation program is started, and it can be observed that the CPU frequency where the attacker is located is successfully reduced.

Edit test_power.sh

    parsec=('blackscholes' 'bodytrack' 'facesim' 'x264' 'raytrace' 'fluidanimate' 'canneal' 'streamcluster')

    for i in ${parsec[@]};do

        filedir = <PATH>/${i}
        if [ -d $filedir ]; then
        /usr/bin/rm -rf $filedir
        fi
        mkdir -p $filedir

        turbo -i 1 > $filedir/turbo &

        sshpass -p <PASSWORD> ssh -n -f -p <PORT_VICTIM> root@localhost taskset -ac 0 /root/parsec-2.1/bin/parsecmgmt -a run -x pre -i native -p $i 2>&1 | tee  $filedir/log &
        
        pid1=$!

        sleep 10s

        sshpass -p <PASSWORD> ssh -n -f -p <PORT_attacker> root@localhost "cd power-virus/mprime; ./run_mprime.sh 2>&1 1>/dev/null"  &

        sleep 10s

        g++ test_power_iso_beta.cpp -o test_power_iso_beta && sudo ./test_power_iso_beta

        wait $pid1

        sshpass -p <PASSWORD> ssh -n -f -p <PORT_attacker> root@localhost ./stop_mprime.sh

    done
    
Run test

    sudo ./test_power.sh

## Contributors

	Shuai Xue <xueshuai@sjtu.edu.cn>
	Shang Zhao <zhaoshangsjtu@sjtu.edu.cn>
	Quan Chen <chen-quan@cs.sjtu.edu.cn>
	Yihao Wu <wuyihao@linux.alibaba.com>
	Shanpei Chen <shanpeic@linux.alibaba.com>
	Yu Xu <xuyu@linux.alibaba.com>
	Zhen Ren <renzhen@linux.alibaba.com>

## Acknowledgement

We would like to acknowledge the hard work of the authors of the benchmarks programs.

- [Parsec Suite](https://parsec.cs.princeton.edu/)
- [TailBench Suite](http://tailbench.csail.mit.edu/)
- [Cachebench](http://icl.cs.utk.edu/llcbench/cachebench.html)
- [Mprime](https://www.mersenne.org/download/)
- [Linpack](https://www.top500.org/project/linpack/)

## License

[MIT](/LICENCE)
