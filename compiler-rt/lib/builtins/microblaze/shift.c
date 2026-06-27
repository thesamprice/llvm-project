//===-- shift.c - 32-bit shift builtins for MicroBlaze --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Built with +barrel-shift (see builtin-config-ix.cmake), so these lower to
// bsll/bsrl/bsra instructions rather than recursing into the same builtins.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

si_int __ashlsi3(si_int a, int b) { return (su_int)a << b; }
su_int __lshrsi3(su_int a, int b) { return a >> b; }
si_int __ashrsi3(si_int a, int b) { return a >> b; }
