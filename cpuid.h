#ifndef CPUID_H
#define CPUID_H

#ifdef _WIN32
#include <limits.h>
#include <intrin.h>
typedef unsigned __int32  uint32_t;

#else
#include <stdint.h>
#endif


class CPUID {

public:
  // explicit CPUID(unsigned level) {
  //   asm volatile
  //     ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
  //      : "0" (level));
  // }
  static uint32_t cpuid(unsigned level, int idx)
  {
    uint32_t regs[4];
    asm volatile
      ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
       : "0" (level));
    return regs[idx];
  }

  static uint32_t cpuid_count(unsigned level, unsigned count, int idx) {
    uint32_t regs[4];
    asm volatile
      ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
       : "0" (level), "2" (count));
    return regs[idx];
  }
};


// class CPUCount {
//   uint32_t regs[4];

// public:
//   explicit CPUCount(unsigned level, unsigned count) {
//     asm volatile
//       ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
//        : "0" (level), "2" (count));
//   }

//   const uint32_t &EAX() const {return regs[0];}
//   const uint32_t &EBX() const {return regs[1];}
//   const uint32_t &ECX() const {return regs[2];}
//   const uint32_t &EDX() const {return regs[3];}
// };
#endif // CPUID_H