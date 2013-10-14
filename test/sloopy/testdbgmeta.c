// RUN: %clang %s -g -S -emit-llvm -o - | FileCheck %s

void a();
void b();

// CHECK: define void @test_for
// CHECK: for.inc:
// CHECK: load
// CHECK: add nsw
// CHECK: load
// CHECK: add nsw
// CHECK: br label %{{.+}}, !dbg ![[DEBUGFOR:[0-9]+]]
void test_for() {
    for (int i = 1,
             j = 2,
             k = i++;
         i != j &&
         j != k;
         i++,
         j--)
    {
        // this
        // is
        // a
        // comment
        a();
        b();
        // this
        // is
        // another
        // comment
    }
}

// CHECK: define void @test_while
// CHECK: while.body:
// CHECK: load
// CHECK: add nsw
// CHECK: load
// CHECK: add nsw
// CHECK: br label %{{.+}}, !dbg ![[DEBUGWHILE:[0-9]+]]
void test_while() {
    int i = 1,
        j = 2,
        k = i++;
    while (i != j &&
            j != k)
    {
        // this
        // is
        // a
        // comment
        a();
        b();
        i++;
        j--;
        // this
        // is
        // another
        // comment
    }
}

// CHECK: define void @test_do
// CHECK: land.end:
// CHECK: br {{.+}}, label {{.+}}, label {{.+}}, !dbg ![[DEBUGDO:[0-9]+]]
void test_do() {
    int i = 1,
        j = 2,
        k = i++;
    do
    {
        // this
        // is
        // a
        // comment
        a();
        b();
        i++;
        j--;
        // this
        // is
        // another
        // comment
    }
    while (i != j &&
            j != k);
}

// CHECK: define void @test_goto
// CHECK: if.then:
// CHECK: br label %{{.+}}, !dbg ![[DEBUGGOTO:[0-9]+]]
void test_goto() {
    int i = 1,
        j = 2,
        k = i++;
label:
    a();
    b();
    i++;
    j--;
    if (i != j &&
        j != k)
    {
        goto label;
    }
}

// CHECK: ![[DEBUGFOR]] = metadata !{i32 14,
// CHECK: ![[DEBUGWHILE]] = metadata !{i32 46,
// CHECK: ![[DEBUGDO]] = metadata !{i32 71,
// CHECK: ![[DEBUGGOTO]] = metadata !{i32 105,
