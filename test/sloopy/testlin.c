// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int i;
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void a() { while (i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void b() { while (-i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void c() { while (+i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void d() { while (3+i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void e() { while (i+3) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void f() { while (3-i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void g() { while (i-3) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void h() { while (3*i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void j() { while (i*3) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void k() { while (i+i) { i++; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a2() { while (!i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void b2() { while (!-i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c2() { while (!+i) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void d2() { while (!(3+i)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void e2() { while (!(i+3)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void f2() { while (!(3-i)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void g2() { while (!(i-3)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void h2() { while (!(3*i)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void j2() { while (!(i*3)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void k2() { while (!(i+i)) { i++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void d2n() { while ((!3)+i) { i++; } }

/* ---------------------------------------- */

// CHECK: Proved: 0
void Xl() { while (i*i) { i++; } }
// CHECK: Proved: 0
void Xm() { while (i) { } }
// CHECK: Proved: 0
void Xn() { while (i) { if(i) { i++; } } }
