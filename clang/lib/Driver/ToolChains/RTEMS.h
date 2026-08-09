//===--- RTEMS.h - RTEMS ToolChain Implementations --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RTEMS_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RTEMS_H

#include "Gnu.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {
namespace rtems {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("rtems::Linker", "linker", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace rtems
} // end namespace tools

namespace toolchains {

/// RTEMS -- a statically linked, single address space RTOS.
///
/// The kernel is an ordinary ELF static link against libraries the RTEMS build
/// installs, so this derives from Generic_ELF to reuse the GCC installation
/// detector.  That is what supplies the multilib selection, the crt objects and
/// the libstdc++ include paths; without it every one of those has to be passed
/// by hand on the command line.
class LLVM_LIBRARY_VISIBILITY RTEMS : public Generic_ELF {
public:
  RTEMS(const Driver &D, const llvm::Triple &Triple,
        const llvm::opt::ArgList &Args);

  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }
  bool IsIntegratedAssemblerDefault() const override { return true; }

  /// RTEMS is not self hosted, so the host /usr/include must never be used.
  bool IsMathErrnoDefault() const override { return false; }

  /// Runtime libraries are installed under an "rtems" directory so that
  /// compiler-rt and the driver agree on where to look.
  StringRef getOSLibName() const override { return "rtems"; }

  /// RTEMS unwinds with libgcc; compiler-rt ships no unwinder.
  UnwindLibType GetDefaultUnwindLibType() const override {
    return ToolChain::UNW_Libgcc;
  }

  /// RTEMS links with GNU binutils; the cross ld is found via the program
  /// paths seeded from the GCC installation.
  const char *getDefaultLinker() const override { return "ld"; }

  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildLinker() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RTEMS_H
