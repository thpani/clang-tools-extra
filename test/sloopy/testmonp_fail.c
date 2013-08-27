// RUN: sloopy -dump-classes %s -- 2>&1 | FileCheck %s
int I, J, N, M;
unsigned UI, UJ, UN;
int F();
int G();
int H(int);

// CHECK: Proved: 0
void Xa() { while (I*I < N) { I++; } }
// CHECK: Proved: 0
void Xb() { while (I < N) { } }
// CHECK: Proved: 0
void Xd() { while (I < N) { if(I) { I++; } } }
// CHECK: Proved: 0
void Xe() { while (I < N) { if(I) { I++; } else { I--; } } }
// CHECK: Proved: 0
void Xe2() { while (I < N) { if(I) { I+=1; } else { I+=-1; } } }
// CHECK: Proved: 0
void Xf() { while (I+J < N) { I++; J++; } }
// CHECK: Proved: 0
void Xi() { while (I < N) { label: I++; if (I) goto label; } }
// CHECK: Proved: 0
void Xk() { while (I < H(I)) { I++; } }
// CHECK: Proved: 0
void Xl() { while (I-I < N) { I++; } }
// CHECK: Proved: 0
void Xg() { while (I*I/I < N) { I++; } }
// CHECK: Proved: 0
void Xc10() {
    while (1) {
        I++;
        if (N) break;
    }
}
// CHECK: Proved: 0
void Xc10_2() {
    while (1) {
        I+=2;
        if (I) break;
    }
}
// CHECK: Proved: 0
void c10_22() {
    while (1) {
        I++;
        if (2*I == 42) break;
    }
}

// CHECK: Proved: 0
void Xm() { while (I < N) { I+=J; } }
