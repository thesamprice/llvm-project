//===--- RTEMS.cpp - RTEMS ToolChain Implementations ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RTEMS.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

RTEMS::RTEMS(const Driver &D, const llvm::Triple &Triple, const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  // The GCC installation supplies the multilib directory, the crt objects and
  // the libstdc++ headers.  RTEMS ships several multilibs per architecture and
  // picking the wrong one links a libc built for a different floating point
  // ABI, so this is not merely a convenience.
  GCCInstallation.init(Triple, Args);

  if (GCCInstallation.isValid()) {
    const Multilib &M = GCCInstallation.getMultilib();
    addPathIfExists(D, GCCInstallation.getInstallPath() + M.gccSuffix(),
                    getFilePaths());
    addPathIfExists(D,
                    GCCInstallation.getParentLibPath() + "/../" +
                        GCCInstallation.getTriple().str() + "/lib" +
                        M.osSuffix(),
                    getFilePaths());
  }

  if (!D.SysRoot.empty()) {
    SmallString<128> P(D.SysRoot);
    llvm::sys::path::append(P, "lib");
    addPathIfExists(D, P, getFilePaths());
  }

  // Prefer the cross binutils over whatever "ld" happens to be on PATH.
  // Without this the driver hands an RTEMS object to the host linker, which is
  // the failure this ToolChain exists to stop.  Binutils installs an unprefixed
  // ld under <sysroot>/bin, so that directory is searched first; the prefixed
  // copies next to GCC are the fallback.
  if (!D.SysRoot.empty()) {
    SmallString<128> B(D.SysRoot);
    llvm::sys::path::append(B, "bin");
    addPathIfExists(D, B, getProgramPaths());
  }
  if (GCCInstallation.isValid()) {
    SmallString<128> B(GCCInstallation.getParentLibPath());
    llvm::sys::path::append(B, "..", getTriple().str(), "bin");
    addPathIfExists(D, B, getProgramPaths());
  }
}

void RTEMS::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                      ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc))
    addSystemInclude(DriverArgs, CC1Args, getDriver().ResourceDir + "/include");

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // RTEMS tools are never self hosted, so the host include directories are
  // deliberately not added; the sysroot is the only source of system headers.
  if (!getDriver().SysRoot.empty()) {
    SmallString<128> P(getDriver().SysRoot);
    llvm::sys::path::append(P, "include");
    addExternCSystemInclude(DriverArgs, CC1Args, P);
  }
}

Tool *RTEMS::buildLinker() const { return new tools::rtems::Linker(*this); }

void rtems::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const auto &TC = static_cast<const toolchains::RTEMS &>(getToolChain());
  const Driver &D = TC.getDriver();
  ArgStringList CmdArgs;

  Args.ClaimAllArgs(options::OPT_g_Group);
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  Args.ClaimAllArgs(options::OPT_w);
  Args.ClaimAllArgs(options::OPT_static);
  Args.ClaimAllArgs(options::OPT_pie);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  // RTEMS is always a static link into a single address space.
  CmdArgs.push_back("-Bstatic");

  if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  }

  const bool NoStdLib = Args.hasArg(options::OPT_nostdlib, options::OPT_r);
  const bool NoStartFiles =
      NoStdLib || Args.hasArg(options::OPT_nostartfiles);

  // The BSP supplies the linker command file.  GCC's -qrtems spec adds this;
  // without it the link silently succeeds with lld's default layout and
  // produces an unbootable image, which is a much worse failure than an error.
  if (!NoStdLib && !Args.hasArg(options::OPT_T))
    CmdArgs.push_back("-Tlinkcmds");

  if (!NoStartFiles) {
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crti.o")));
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crtbegin.o")));
  }

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  TC.AddFilePathLibArgs(Args, CmdArgs);
  Args.addAllArgs(CmdArgs, {options::OPT_T_Group, options::OPT_s,
                            options::OPT_t, options::OPT_r});

  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!NoStdLib) {
    // The RTEMS libraries are mutually dependent and a linker which makes a
    // single pass over each archive cannot resolve them in any fixed order, so
    // they have to be a group.  This is the load bearing half of -qrtems.
    CmdArgs.push_back("--start-group");
    if (C.getDriver().CCCIsCXX())
      CmdArgs.push_back("-lstdc++");
    CmdArgs.push_back("-lrtemsbsp");
    CmdArgs.push_back("-lrtemscpu");
    CmdArgs.push_back("-latomic");
    CmdArgs.push_back("-lc");
    CmdArgs.push_back("-lgcc");
    CmdArgs.push_back("--end-group");
  }

  if (!NoStartFiles) {
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crtend.o")));
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crtn.o")));
  }

  const char *Exec = Args.MakeArgString(TC.GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
