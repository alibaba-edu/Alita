#ifndef VM_STATUA_H
#define VM_STATUA_H

// from blacklist import g_blacklist
// from config_path import ConfigPath
// from exception import FileNotExist, VmStatusException
// from utility import parse_section
#include <bits/stdc++.h>
#include "utility.h"

// # topo_cpu => { cpu : {socket, core} }
// # socket_topo => { socket: set([cpu]) }
// # vm_cpu => { vm : set[cpu] } , is tmp, then no use in power_iso strategy
// # vm_socket_cpu => { vm : socket : set[cpu] }
// # socket_vm => { socket : set[vm] }

// map<int, vector<int>> topo_cpu;
// map<int, set<int>> socket_topo;
map<int, set<int>> vm_cpu; //, is tmp, then no use in power_iso strategy
map<int, map<int, set<int>>> vm_socket_cpu;
map<int, set<int>> socket_vm;

vector<string> vm_name; // record vm id[0~n-1] with names(string)

void get_vm_topo()
{

	string base = "/sys/fs/cgroup/cpuset/machine.slice/";
	DIR *dirp_vm = opendir(base.c_str());
	struct dirent *dp_vm;
	int vm_cnt = 0;

	while ((dp_vm = readdir(dirp_vm)) != NULL) //find all VMs
	{
		string vm_dirname = string(dp_vm->d_name);
		if (vm_dirname.substr(0, 12) != "machine-qemu")
			continue;
		//cout<<vm_dirname<<endl;
		int point_pos = vm_dirname.find_last_of(".");
		//int vm_id = vm_dirname[point_pos-1] - '0';
		if (isdigit(vm_dirname[point_pos - 2]))
		{
			point_pos += 10 * (vm_dirname[point_pos - 2] - '0');
		}
		vm_name.push_back(vm_dirname);
		//cout<<vm_id<<endl; //vm_cpu[vm_id];

		string vcpu_base = base + vm_dirname + "/";
		DIR *dirp_vcpu = opendir(vcpu_base.c_str());
		struct dirent *dp_vcpu;

		while ((dp_vcpu = readdir(dirp_vcpu)) != NULL) //find all [cpu] from vcpu in one VM
		{
			std::smatch sm;
			string vcpu_name = string(dp_vcpu->d_name);

			if (!regex_match(vcpu_name, sm, regex("vcpu([0-9]+)")))
				continue;
			ifstream vcpu_fin(vcpu_base + vcpu_name + "/cpuset.cpus");
			int cpu;
			vcpu_fin >> cpu;
			vm_cpu[vm_cnt].insert(cpu);
		}
		vm_cnt++;
	}

	for (int vm_id = 0; vm_id < (int)vm_cpu.size(); vm_id++)
	{
		for (int socket = 0; socket < (int)socket_topo.size(); socket++)
		{
			set_intersection(vm_cpu[vm_id].begin(), vm_cpu[vm_id].end(), socket_topo[socket].begin(), socket_topo[socket].end(), inserter(vm_socket_cpu[vm_id][socket], vm_socket_cpu[vm_id][socket].begin()));
		}
	}

	for (int vm_id = 0; vm_id < (int)vm_cpu.size(); vm_id++)
	{
		for (int socket = 0; socket < (int)socket_topo.size(); socket++)
		{
			if (vm_socket_cpu[vm_id][socket].size() < 4)
				continue;
			socket_vm[socket].insert(vm_id);
		}
	}
}

int nr_vms;

void init_socket_cpu_vm()
{
	init_topology();
	get_vm_topo();

	nr_sockets = (int)socket_topo.size();
	nr_cpus = (int)topo_cpu.size();
	nr_vms = (int)vm_cpu.size();

	for (int i = 0; i < nr_cpus; i++)
		printf("cpu %d --> %d core, %d socket\n", i, topo_cpu[i][1], topo_cpu[i][0]);
		
	printf("\n");

	for (int i = 0; i < nr_sockets; i++)
	{
		printf("sokcet %d has cpus:", i);
		for (auto cpu : socket_topo[i])
			cout << cpu << ' ';
		printf("\n\n");
	}

	for (int i = 0; i < nr_sockets; i++)
	{
		printf("sokcet %d has vms:", i);
		for (auto vm : socket_vm[i])
			cout << vm << ' ';
		printf("\n\n");
	}

	for (int vm_id = 0; vm_id < nr_vms; vm_id++)
	{
		printf("VM%d includes %d cpus, they are:\n", vm_id, (int)vm_cpu[vm_id].size());
		for (auto cpu : vm_cpu[vm_id])
			cout << cpu << ' ';
		printf("\n\n");
	}

	for (int vm_id = 0; vm_id < nr_vms; vm_id++)
	{
		for (int socket = 0; socket < nr_sockets; socket++)
		{
			printf("VM_%d_socket_%d includes %d cpus, they are:\n", vm_id, socket, (int)vm_socket_cpu[vm_id][socket].size());
			for (auto cpu : vm_socket_cpu[vm_id][socket])
				cout << cpu << ' ';
			printf("\n");
		}
	}
}

// int main(){

// 	cout<<nr_cpus<<nr_sockets<<nr_vms<<endl;
// 	init_topology();
// 	get_vm_topo();

// 	nr_sockets = (int)socket_topo.size();
// 	nr_cpus = (int)topo_cpu.size();
// 	nr_vms = (int)vm_cpu.size();

// 	for (int i=0;i<nr_cpus;i++)
// 		printf("cpu %d --> %d core, %d socket\n",i,topo_cpu[i][1],topo_cpu[i][0]);
// 	printf("\n");
// 	for (int i=0;i<nr_sockets;i++){
// 		printf("sokcet %d has cpus:",i);
// 		for (auto cpu:socket_topo[i])
// 			cout<<cpu<<' ';
// 		printf("\n\n");
// 	}
// 	for (int vm_id=0;vm_id<nr_vms;vm_id++){
// 		printf("VM%d includes %d cpus, they are:\n",vm_id,(int)vm_cpu[vm_id].size());
// 		for (auto cpu:vm_cpu[vm_id])
// 			cout<<cpu<<' ';
// 		printf("\n\n");
// 	}
// 	for (int vm_id = 0; vm_id < nr_vms; vm_id++){
// 		for (int socket = 0; socket < nr_sockets; socket++){
// 			printf("VM_%d_socket_%d includes %d cpus, they are:\n", vm_id, socket, (int)vm_socket_cpu[vm_id][socket].size());
// 			for (auto cpu:vm_socket_cpu[vm_id][socket])
// 				cout<<cpu<<' ';
// 			printf("\n");
// 		}
// 	}
// 	return 0;
// }

# endif
