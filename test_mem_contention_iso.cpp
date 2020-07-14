#include "mem_contention_iso.h"

int main()
{
    cout << "=== LLC & Memory Bandwidth Isolation: Start==" <<endl;
    init();
    MemContentionConfig config;

    config.llc_on = true;
    config.llc_thrtl_bc = 0.95;
    config.llc_thrtl_pct = 0.1;
    config.llc_thrtl_membw = 3;
    config.llc_thrtl_period = 60;
    config.llc_relax_guard = 0.05;
    config.llc_relax_period = 30;

    config.mba_on = true;
    config.mba_threshold = 50;
    config.mba_relax_guard = 5;
    config.mba_rate_min = 20;

    config.interval = 5;
    
    MemoryContention mem_contention_iso(config);
    // vector<int> record;
    // split_lock_iso.get_split_locks(record);
    // cout << accumulate(record.begin(), record.end(), 0) << endl;

    // cout << "===split_lock_scheme: analysis==" <<endl;
    // FixedQueue<double, FIXLEN> rates;
    // rates.push_back(1);
    // rates.push_back(3);
    // rates.push_back(5);
    // rates.push_back(4);

    // split_lock_iso.analysis(rates, 500);
    // for (int i = 0; i < rates.size(); i++)
    //     cout << rates[i] << " ";
    // cout << endl;
    
    while (true)
    {
        if(mem_contention_iso.detect())
        {
            mem_contention_iso.control(); 
        }
    }


    return 0;
}