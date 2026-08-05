# RUN: llvm-mca -mtriple=microblazeel -mcpu=generic %s | FileCheck %s --check-prefix=SPEED
# RUN: llvm-mca -mtriple=microblazeel -mcpu=generic -mattr=+area-optimized %s | FileCheck %s --check-prefix=AREA

# Multiply pipelining differs by C_AREA_OPTIMIZED (UG984 pipeline section): in the
# 5-stage performance pipeline the multiplier is pipelined (one issue per cycle),
# while in the 3-stage area pipeline multiply is multi-cycle and holds the unit,
# so two independent multiplies serialize.  Block reciprocal throughput triples.

mul r3, r5, r7
mul r6, r8, r9

# SPEED: Dispatch Width:    1
# SPEED: Block RThroughput: 2.0

# AREA:  Dispatch Width:    1
# AREA:  Block RThroughput: 6.0
