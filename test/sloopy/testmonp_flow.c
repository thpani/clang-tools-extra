// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c1() {
    while (I < N) {
        if (I == 2) {
            I++;
        } else {
            I+=1;
        }
    }
}
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c2() {
    while (I < N) {
        if (I == 2) {
            I++;
            I++;
        } else {
            I+=2;
        }
    }
}
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c3() {
    while (I < N) {
        if (I == 2) {
            if (F()) {
                I+=2;
            } else {
                I+=2;
            }
        } else {
            I+=2;
        }
    }
}
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c4() {
    while (I < N) {
        if (I == 2) {
            if (F()) {
                I+=1;
            } else {
                I+=1;
            }
            I+=1;
        } else {
            I+=2;
        }
    }
}
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c5() {
    while (I < N) {
        if (I == 2) {
            I+=3;
            I--;
        } else {
            I+=2;
        }
    }
}
// CHECK: Proved: 1
// CHECK: ProvedWithoutAssumptions: 1
void c7() {
    while (I < N) {
        if (I == 2) {
            if (F()) {
                I+=2;
            } else {
                I+=3;
                I--;
            }
            I+=-1;
        } else {
            I+=1;
        }
    }
}

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void c10() {
    while (1) {
        I++;
        if (I > N)
            break;
    }
}

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void c10_2() {
    while (1) {
        I++;
        if (!I) break;
    }
}

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void c10_21() {
    while (1) {
        I++;
        if (!(3*I)) break;
    }
}

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionWrapvOrRunsInto: 1
void c10_22() {
    while (1) {
        I++;
        if (I == 42) break;
    }
}

// CHECK: Proved: 1
// CHECK: ProvedWithAssumptionLeBoundNotMax: 1
void c11() {
label:
    I++;
    if (I > N)
        return;
    goto label;
}

// CHECK: Proved: 1
void Xj() { while (I < N) { label: I++; if (I) break; goto label; } }
