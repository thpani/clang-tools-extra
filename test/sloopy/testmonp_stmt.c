// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void s1() { while (I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void s2() { do { I++; } while (I < N); }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void s3() { label: I++; if (I < N) goto label; }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void s4() { for (int i=0; i < N; i++) { F(); i++; G(); } }

