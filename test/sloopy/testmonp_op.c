// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b1() { while (I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b2() { while (N > I) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b3() { while (I > N) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b4() { while (N < I) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void b1E() { while (I <= N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void b2E() { while (N >= I) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionGeBoundNotMin: 1
void b3E() { while (I >= N) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionGeBoundNotMin: 1
void b4E() { while (N <= I) { I--; } }
