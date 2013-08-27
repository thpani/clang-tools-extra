// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void a() { while (I != N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void b() { while (I != N) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void c() { while (1) { if (I == N) { break; } I--; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a2() { while (I == N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b2() { while (I == N) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c2() { while (1) { if (I != N) { break; } I--; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a3() { while (I == N) { I+=2; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b3() { while (I == N) { I-=14; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c3() { while (1) { if (I != N) { break; } I+=2; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a4() { while (3*I == N) { I+=2; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
void b4() { while (J*I == N) { I-=14; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c4() { while (1) { if (3*(I+2)+F() != N) { break; } I+=2; } }

// CHECK: Proved: 0
void X0() { while (I != N) { } }
// CHECK: Proved: 0
void Xa() { while (I != N) { I+=2; } }
// CHECK: Proved: 0
void Xb() { while (I != N) { I-=2; } }
