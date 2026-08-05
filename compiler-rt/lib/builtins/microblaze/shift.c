// MicroBlaze shift helpers.
// The MicroBlaze base ISA has no barrel shifter; these provide the compiler-rt
// libcall implementations registered by the LLVM backend for SHL/SRL/SRA.
#include "../int_lib.h"

si_int __ashlsi3(si_int a, int b) { return (su_int)a << b; }
su_int __lshrsi3(su_int a, int b) { return a >> b; }
si_int __ashrsi3(si_int a, int b) { return a >> b; }
