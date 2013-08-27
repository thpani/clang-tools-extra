// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a() { while (I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void aE() { while (I <= N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void b() { while (-I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void bu() { while (-UI < UN) { UI++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c() { while (+I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void d() { while (3+I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void e() { while (I+3 < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void f() { while (3-I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void g() { while (I-3 < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void h() { while (3*I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void i() { while (I*3 < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void k() { while (I+I < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void kMin() { while (I-J < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void l() { while (I+F() < G()) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void m() { while (4*I*F() + G() + 2 < 3 * G()) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void n() { while (I+J < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void o() { while (I*J < N) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void p() { while (I < N) { F(); I--; G(); } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void q() { while (I+J < N+M) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void r() { while (I+J+42 < N+M-3+2*M) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void s() { while (42 * (I+J) * (J/2+2) < (N+M-3+2*M/32)) { I++; } }

// CHECK: Proved: 1
void c1() {
    while (I < N) {
        if (I == 2) {
            I+=2;
        } else {
            I+=1;
        }
    }
}
