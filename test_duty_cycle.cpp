#include "duty_cycle.h"
#include "init.h" 
int main()
{
    init();
    uint64_t duty;
    duty = read_duty_cycle(1);
    //cout << duty << endl;

    write_duty_cycle(1, 10);
    duty = read_duty_cycle(1);
    assert(duty == 10);

        vector<int> tmp = vector<int>(nr_cpus, 0);

    DutyCycleScheme split_lock_scheme;
    cout << "===split_lock_scheme==" <<endl;
    split_lock_scheme.update_duty(0, 1, true);
    vector<double> freq_rate =  split_lock_scheme.get_freq_rate();
    for (int i = 0; i < nr_cpus; i++)
    {
        cout << freq_rate[i] << " ";
    }

}