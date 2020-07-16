#ifndef UTILITY_H
#define UTILITY_H

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <iostream>
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <set>
#include <assert.h>
using namespace std;

#ifndef DEBUG 
#define DEBUG 0 // set debug mode
#endif

#if DEBUG
#define DEBUG_LOG(...) {\
    char str[100];\
    sprintf(str, __VA_ARGS__);\
    std::cout << "[" << __FILE__ << "][" << __FUNCTION__ << "][Line " << __LINE__ << "] " << str << std::endl;\
    }
#else
#define DEBUG_LOG(...)
#endif

unsigned int high_bit = 63, low_bit = 0;

void write_msr(int cpu, uint32_t reg, uint64_t val)
{
	uint64_t data;
	int fd;
	char msr_file_name[64];

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_WRONLY);
	if (fd < 0)
	{
		if (errno == ENXIO)
		{
			fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
			exit(2);
		}
		else if (errno == EIO)
		{
			fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
					cpu);
			exit(3);
		}
		else
		{
			perror("wrmsr: open");
			exit(127);
		}
	}

	data = val;
	if (pwrite(fd, &data, sizeof data, reg) != sizeof data)
	{
		if (errno == EIO)
		{
			fprintf(stderr,
					"wrmsr: CPU %d cannot set MSR "
					"0x%08" PRIx32 " to 0x%016" PRIx64 "\n",
					cpu, reg, data);
			exit(4);
		}
		else
		{
			perror("wrmsr: pwrite");
			exit(127);
		}
	}

	close(fd);

	return;
}

uint64_t read_msr(int cpu, uint32_t reg)
{
	uint64_t data;
	int fd;
	// char *pat;
	// int width;
	char msr_file_name[64];
	unsigned int bits;

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_RDONLY);
	if (fd < 0)
	{
		if (errno == ENXIO)
		{
			fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
			exit(2);
		}
		else if (errno == EIO)
		{
			fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
					cpu);
			exit(3);
		}
		else
		{
			perror("rdmsr: open");
			exit(127);
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data)
	{
		if (errno == EIO)
		{
			fprintf(stderr, "rdmsr: CPU %d cannot read "
							"MSR 0x%08" PRIx32 "\n",
					cpu, reg);
			exit(4);
		}
		else
		{
			perror("rdmsr: pread");
			exit(127);
		}
	}

	close(fd);

	bits = high_bit - low_bit + 1;
	if (bits < 64)
	{
		/* Show only part of register */
		data >>= low_bit;
		data &= (1ULL << bits) - 1;
	}

	return (uint64_t)data;
}


// # core_vote_cpu  => { (socket, core} : vote ht }
// # socket_vote_cpu => { socket : vote cpu }
// # socket_vote_core => { socket: set([vote cpu of core]) }
// # topo_cpu => { cpu : (socket, core) }
// # socket_topo => { socket: set([cpu]) }
map<vector<int>, int> core_vote_cpu;
map<int, int> socket_vote_cpu;
map<int, set<int>> socket_vote_core;
map<int, vector<int>>topo_cpu;
map<int, set<int>> socket_topo;
void init_topology()
{
	

	string base = "/sys/devices/system/cpu/";
	DIR *dirp = opendir(base.c_str());
	struct dirent *dp;
	while ((dp = readdir(dirp)) != NULL)
	{
		std::smatch sm;
		string dirname = string(dp->d_name);
		

		if (! regex_match(dirname, sm, regex("cpu([0-9]+)")))
			continue;
		//cout << (dp->d_name) << endl;
		int cpu = stoi(sm[1].str());

		string topodir = base + string(dp->d_name) + "/topology";
		struct stat info;
		if (stat(topodir.c_str(), &info) != 0)
			continue;

		std::ifstream so_input(topodir + "/physical_package_id");
		int socket;
		so_input >> socket;
		std::ifstream co_input(topodir + "/core_id");
		int core;
		co_input >> core;
		//cout << "socket:" << socket << ", core:" << core << ", cpu:" << cpu << endl;

		if (core_vote_cpu.find(vector<int>{socket, core}) == core_vote_cpu.end())
		{
			// not found
			core_vote_cpu[vector<int>{socket, core}] = cpu;
		}
		else
		{
			// found
			if(core_vote_cpu[vector<int>{socket, core}] > cpu)
				core_vote_cpu[vector<int>{socket, core}] = cpu;
		}

		if (socket_vote_cpu.find(socket) == socket_vote_cpu.end())
		{
			// not found
			socket_vote_cpu.insert(make_pair(socket, cpu));
		}
		else
		{
			// found
			if(socket_vote_cpu[socket] > cpu)
				socket_vote_cpu[socket] = cpu;
		}

		//socket => set(cpu)
		socket_topo[socket].insert(cpu);

		//cpu => (socket, core)
		topo_cpu[cpu] = vector<int>{socket, core};
	}

	set<int> vote_cpus;
	for (auto it = core_vote_cpu.begin(); it != core_vote_cpu.end();it++)
	{
		vote_cpus.insert(it->second);
	}
	for (auto it = socket_topo.begin(); it != socket_topo.end();it++)
	{
		set<int> inter;
		set_intersection(it->second.begin(),it->second.end(),vote_cpus.begin(),vote_cpus.end(),inserter(inter,inter.begin()));
		socket_vote_core[it->first] = inter;
	}

	
	closedir(dirp);
}

template <typename T, int MaxLen>
class FixedQueue : public std::deque<T>
{
public:
    void push_back(const T &value)
    {
        if (this->size() == MaxLen)
        {
            this->pop_front();
        }
        std::deque<T>::push_back(value);
    }
    void clear()
    {
        while(this->size())
        {
            this->pop_front();
        }
    }
};

int nr_sockets;
int nr_cpus;

// unit: MB
int get_llcinfo()
{
		std::ifstream input("/sys/devices/system/cpu/cpu0/cache/index3/size");
		std::smatch sm;
		string tmp;
		input >> tmp;
		regex_match(tmp, sm, regex("(\\d+)K"));

		int llc_size = std::stoi(sm[0].str()) /1024.0;
        return llc_size;
}

#endif
