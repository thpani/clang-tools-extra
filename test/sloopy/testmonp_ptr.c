// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, K;
int *P, *Q;
int *N, *M;
int *F();
int G();
int S(const char *);

const char *s = "nudelsieb";

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void a() { while (P < N) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void aE() { while (P <= N) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void d() { while (3+P < N) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void e() { while (P+3 < N) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void g() { while (P-3 < N) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void k() { while (P-Q < K) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void l() { while (P-F() < G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void m() { while (P-F() + G() < G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void n() { while (P-Q < K) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void p() { while (P < N) { F(); P--; G(); } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void x() { while (P-Q < N-M) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void y() { while (P-Q-42 < N-M) { P--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void z() { while (42 * (P-Q) * (Q-N) < (N-M)) { P--; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void AddrOfA() { while (P < &I) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void AddrOfB() { while (&I+3 < Q) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void AddrOfC() { while (3*((long)&I) < J) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void AddrOfCW() { while (3*((long)&I) < J) { I--; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionMNeq0: 1
// CHECK: ProvedWithAssumptionWrapv: 1
void AddrOfCM() { while (K*((long)&I) < J) { I++; } }

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void DerefA() { while (I < *P) { I++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefB() { while (*P < G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefC() { while (*P != G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefC2() { while (*P+2 != G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefC3() { while (*P*2 != G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefC4() { while (*P+I != G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefC5() { while (*P*I != G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefD() { while (*P == G()) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefE() { while (*P) { P++; } }
// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefE2() { while (!*P) { P++; } }

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionRightArrayContent: 1
void DerefStr() { while (*s != 'l') { s++; } }

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void StringLit() { while (I != S("teststring")) { I++; } }
