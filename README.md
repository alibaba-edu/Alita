# Alita
This repository contains the source code for a research paper that was submitted for publication at the International Conference for [High Performance Computing, Networking, Storage, and Analysis](https://sc20.supercomputing.org/) (SC20). 

## How to run

- Split Lock Isolation

    g++ test_split_lock_iso.cpp  -o test_split_lock_iso && sudo ./test_split_lock_iso

- LLC & Memory Bandwidth Isolation

    g++ test_mem_contention_iso.cpp -o test_mem_contention_iso && sudo ./test_mem_contention_iso

- Power Isolation

    g++ test_power_iso.cpp -o test_power_iso && sudo ./test_power_iso
	or
	g++ test_power_iso_beta.cpp -o test_power_iso_beta && sudo ./test_power_iso_beta
	
## Contributors

	Shuai Xue <xueshuai@sjtu.edu.cn>
	Shang Zhao <zhaoshangsjtu@sjtu.edu.cn>
	Quan Chen <chen-quan@cs.sjtu.edu.cn>
	Yihao Wu <wuyihao@linux.alibaba.com>
	Shanpei Chen <shanpeic@linux.alibaba.com>
	Yu Xu <xuyu@linux.alibaba.com>
	Zhen Ren <renzhen@linux.alibaba.com>

## Acknowledgement 
Benchmarks in our paper:
- [Parsec Suite](https://parsec.cs.princeton.edu/)
- [TailBench Suite](http://tailbench.csail.mit.edu/)
- [Cachebench](http://icl.cs.utk.edu/llcbench/cachebench.html)
- [Mprime](https://www.mersenne.org/download/)
- [Linpack](https://www.top500.org/project/linpack/)


## License

[MIT](/LICENCE)


