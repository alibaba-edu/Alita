#include "cpuid.h"

#include <iostream>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {

  cout << "====test cpuid====" << endl;
  cout << "EAX " << CPUID::cpuid(0x0, 0) << endl;
  cout << "EBX " << CPUID::cpuid(0x0, 1) << endl;
  cout << "ECX " << CPUID::cpuid(0x0, 2) << endl;
  cout << "EDX " << CPUID::cpuid(0x0, 3) << endl;

  cout << endl;
  cout << "====test cpuid====" << endl;
  cout << "EAX " << CPUID::cpuid_count(0x7, 0x0, 0) << endl;
  cout << "EBX " << CPUID::cpuid_count(0x7, 0x0, 1) << endl;
  cout << "ECX " << CPUID::cpuid_count(0x7, 0x0, 2) << endl;
  cout << "EDX " << CPUID::cpuid_count(0x7, 0x0, 3) << endl;
  return 0;
}