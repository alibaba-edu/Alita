#include "lib/utility.h"
int main()
{
	//unsigned long long val = readmsr(2, 240);
	//writemsr(2, 240, 0);
    init_topology();
    cout << "\n====socket_vote_core====" << endl;
	for (auto it = socket_vote_core.begin(); it != socket_vote_core.end();it++)
	{
		std::cout << it->first << ": ";

		for (auto itset=it->second.begin(); itset != it->second.end(); ++itset) 
        	cout << ' ' << *itset; 
		cout << endl;
	}

	cout << "\n====core_vote_cpu====" << endl;
	for (auto it = core_vote_cpu.begin(); it != core_vote_cpu.end();it++)
	{
		std::cout << " (" << it->first[0] << "," <<  it->first[1] << "): " << it->second << std::endl;
	}
	
	cout << "\n====socket_vote_cpu====" << endl;
	for (auto it = socket_vote_cpu.begin(); it != socket_vote_cpu.end();it++)
	{
		std::cout << it->first << ": " << it->second << std::endl;
	}
	cout << "\n====topo_cpu====" << endl;
	for (auto it = topo_cpu.begin(); it != topo_cpu.end();it++)
	{
		std::cout << it->first << ": (" << it->second[0] << "," <<  it->second[1] << ")"<< std::endl;
	}
	cout << "\n====socket_topo====" << endl;
	for (auto it = socket_topo.begin(); it != socket_topo.end();it++)
	{
		std::cout << it->first << ": ";

		for (auto itset=it->second.begin(); itset != it->second.end(); ++itset) 
        	cout << ' ' << *itset; 
		cout << endl;
	}
    
    nr_sockets = socket_topo.size();
    nr_cpus = topo_cpu.size();
    cout << "nr_cpus:" << nr_cpus << endl;
    cout << "nr_sockets:" << nr_sockets << endl;

	int  llc_size = get_llcinfo(); 
	cout << "llc size: " << llc_size << endl;

	return 0;
}