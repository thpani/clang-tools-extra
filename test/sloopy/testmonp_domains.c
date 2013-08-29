// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s

int nondet();
int I, J;
int *P, *Q;

// CHECK: Proved: 1
void a() {
    while (1) {
        I++;
        P++;
        if (nondet()) {
            if (I == J) break;
        } else {
            if (P == Q) break;
        }
    }
}
