# REQUIRES: x86
## A TLS variable's alignment is relative to the start of the TLS block. When a
## SECTIONS command lays out the TLS sections, the first one still has to be
## aligned to the maximum alignment of all of them, or a variable in a later,
## more strictly aligned TLS section gets a block-relative offset that does not
## satisfy its alignment. GNU ld does this in _bfd_elf_tls_setup().

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=x86_64 a.s -o a.o
# RUN: ld.lld -T a.lds a.o -o a
# RUN: llvm-readelf -S -l a | FileCheck %s

## .tdata only needs an alignment of 1, but .tbss needs 256, so .tdata is
## aligned to 256 and .tbss lands at a block-relative offset of 0x100.
# CHECK:      Name   Type     Address          Off    Size   ES Flg Lk Inf Al
# CHECK:      .tdata PROGBITS 0000000000000100 001100 000001 00 WAT 0    0 256
# CHECK-NEXT: .tbss  NOBITS   0000000000000200 001101 000008 00 WAT 0    0 256

#--- a.s
.section .tdata,"awT",@progbits
.byte 0

.section .tbss,"awT",@nobits
.p2align 8
a:
.quad 0

#--- a.lds
SECTIONS {
  . = 0x100;
  .tdata : { *(.tdata) }
  .tbss : { *(.tbss) }
  .text : { *(.text) }
}
