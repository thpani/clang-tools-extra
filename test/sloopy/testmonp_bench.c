// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s

// /Users/thomas/Documents/uni/da/llvm-build/bench/wcet/adpcm.c -func my_sin -line 292 -linebe 293,
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
#define PI 3141
int my_sin(int rad)
{
  /* MAX dependent on rad's value, say 50 */
  while (rad > 2*PI)
      rad -= 2*PI;
}
