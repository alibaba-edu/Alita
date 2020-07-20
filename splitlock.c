#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <time.h>
#pragma pack(push, 4)

struct CounterType
{
    char buf[59];
    long long c;
};
#pragma pack(pop)

int main(int argc, char *argv[])
{
    int size = sizeof(struct CounterType);


    struct CounterType *_p = (struct CounterType *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    register long long *ptr = &_p->c;
    for (;;)
    {
        __sync_fetch_and_add(ptr, 1);
    }

    return 0;
}
