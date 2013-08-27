// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void n1() { while (I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void n2() { while (!!(I < N)) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionGeBoundNotMin: 1
void n3() { while (!(I < N)) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionGeBoundNotMin: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void n4() { while (!(I < N)) { I++; } }
