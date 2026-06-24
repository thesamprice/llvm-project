# REQUIRES: microblaze
# RUN: llvm-mc -filetype=obj -triple=microblazeel %s -o %t.o
# RUN: printf '.global _start\n.text\n_start:\nnop\n' | \
# RUN:   llvm-mc -filetype=obj -triple=microblazeel -o %t2.o -
# RUN: ld.lld -o %t %t.o %t2.o --image-base=0x1000 \
# RUN:   --Tdata=0x2000 --Ttext=0x3000
# RUN: llvm-objdump -s %t | FileCheck %s

## R_MICROBLAZE_32: absolute 32-bit symbol address in a data word.
## _start lands at 0x3000 (--Ttext=0x3000).
## LE encoding of 0x3000 = 00 30 00 00.

.data
.long _start   ## R_MICROBLAZE_32

# CHECK: Contents of section .data:
# CHECK: 2000 00300000
