#include "lib/split_lock_iso.h"
#include "lib/init.h"
#include "lib/utility.h"

int main()
{
    init();

    //SplitLockIso(bool switchon, double interval, int quota)
    cout << "===Split Lock Isolation: Start==" <<endl;
    SplitLockIso split_lock_iso(true, 0.1, 70000);

    
    while (true)
    {
        if(split_lock_iso.detect())
        {
            split_lock_iso.control(); 
        }
    }


    return 0;
}