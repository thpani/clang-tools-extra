#!/usr/bin/env python

import os.path
import sys
from collections import defaultdict

if sys.argv[1] == 'cBench':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/cBench15032013/'
    SLOOPY_FILE = 'classifications_cBench.txt'
    LLVM_ABS_START = '/files/sinn/cBench/'
    LLVM_FILE = '24062013_summary.txt'
elif sys.argv[1] == 'wcet':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/wcet/'
    SLOOPY_FILE = 'classifications_wcet.txt'
    LLVM_ABS_START = '/files/sinn/workspace/WCETBench/compiledDebug/'
    LLVM_FILE = 'summaryWCET14062013.txt'
else:
    sys.exit(1)

sf = open(SLOOPY_FILE)
lf = open(LLVM_FILE)

sloopy_classifications = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

for line in sf:
    filepath, _, func, _, line, classes = line.split(' ', 5)
    line = int(line)

    filepath = './'+os.path.relpath(filepath, SLOOPY_ABS_START)
    classes = classes.split()

    sloopy_classifications[filepath][func][line] = classes

    # print line, func, filepath, classes

results = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))
branch = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))

UNCLASS = 'unclas'
SIMPLE = 'simple'
SIMPLE_DETAILS = ('IntegerIter', 'DataIter', 'AArrayIter', 'PArrayIter')

def isSimple(cls):
    for simpleClass in SIMPLE_DETAILS:
        if '@'+simpleClass in cls:
            return True
    return False
def simplify(clsses):
    for cls in clsses:
        if isSimple(cls):
            return SIMPLE
    return UNCLASS

for line in lf:
    term, bound, line, linebe, func, filepath = line.split()
    line = int(line)

    if LLVM_ABS_START:
        filepath = './'+os.path.relpath(filepath, LLVM_ABS_START)
    if line == 0:
        continue

    fileName, fileExtension = os.path.splitext(filepath)
    lookup_filename = fileName + '.c'

    ok = False
    for d in range(line):
        sc = sloopy_classifications[lookup_filename][func][line-d]
        if sc:
            # print '-'+str(d), filepath, func, line, term, bound, sc
            for cls in sc:
                if 'Branch' in cls:
                    # Branch-Depth-3-Nodes-6
                    c, _, depth, _, nodes = cls.split('-')
                    depth, nodes = (int(depth), int(nodes))
                    if depth > 500:
                        continue
                    branch[c.split('@')[1]+'depth'][term][bound].append(depth)
                    branch[c.split('@')[1]+'nodes'][term][bound].append(nodes)
                elif 'ControlVars' in cls or 'MultiExitIntegerIterIncrSetSize' in cls:
                    # ControlVars-3
                    c, num = cls.split('-')
                    num = int(num)
                    if num > 200:
                        continue
                    branch[c.split('@')[1]][term][bound].append(num)
                elif '@' in cls:
                    results[term][bound][cls.split('@')[1]] += 1
            results[term][bound][simplify(sc)] += 1
            ok = True
            break
    if not ok:
        print >> sys.stderr, '!!', filepath, func, line

print "==================================="
print sys.argv[1]
print "==================================="
print

def sumall(c):
    return sum([ results[y[0]][y[1]][c] for y in ('YY', 'YN', 'NN') ])

def percent(c):
    return 1. * results['Y']['Y'][c] / sumall(c)

def average(s):
    return sum(s) * 1.0 / len(s)

def stdev(s):
    avg = average(s)
    variance = map(lambda x: (x - avg)**2, s)
    import math
    standard_deviation = math.sqrt(average(variance))
    return standard_deviation

def printresult(desc, key, underline='='):
    print desc
    print underline * len(desc)
    print "YY\t|YN\t|NN\t|"
    for y in ('YY', 'YN', 'NN'):
        print "%d\t|" % results[y[0]][y[1]][key],
    print
    print
    print "Percentage of these loops Loopus can bound: %.2f" % percent(key)
    print

def distribution(type):
    print type
    print "==========================================="
    print "\t\t|YY\t|YN\t|NN\t|"
    print "avg(count)\t|",
    for y in ('YY', 'YN', 'NN'):
        l = branch[type][y[0]][y[1]]
        print "%.2f\t|" % average(l),
    print
    print "stdev(count)\t|",
    for y in ('YY', 'YN', 'NN'):
        l = branch[type][y[0]][y[1]]
        print "%.2f\t|" % stdev(l),
    print
    for r in [(0, 1), (1, 2), (2, 3), (3, 10), (10, sys.maxint)]:
        print "cnt [%d, %s)\t|" % (r[0], str(r[1]) if r[1] != sys.maxint else "inf"),
        for y in ('YY', 'YN', 'NN'):
            s = sum(1 if r[0] <= depth and depth < r[1] else 0 for depth in branch[type][y[0]][y[1]])
            print "%d\t|" % s,
        print
    print

def printb(type):
    print "Branching (sliced %s)" % type
    print "============================"
    for x in ('depth', 'nodes'):
        print "\t\t|YY\t|YN\t|NN\t|"
        print "avg(%s)\t|" % x,
        for y in ('YY', 'YN', 'NN'):
            l = branch[type+'Branch'+x][y[0]][y[1]]
            print "%.2f\t|" % average(l),
        print
        print "stdev(%s)\t|" % x,
        for y in ('YY', 'YN', 'NN'):
            l = branch[type+'Branch'+x][y[0]][y[1]]
            print "%.2f\t|" % stdev(l),
        print
        for r in [(0, 1), (1, 2), (2, 3), (3, 10), (10, sys.maxint)]:
            print x+" [%d, %s)\t|" % (r[0], str(r[1]) if r[1] != sys.maxint else "inf"),
            for y in ('YY', 'YN', 'NN'):
                s = sum(1 if r[0] <= depth and depth < r[1] else 0 for depth in branch[type+'Branch'+x][y[0]][y[1]])
                print "%d\t|" % s,
            print
    print

printresult("Simple Control Flow (EXIT block has 1 pred)", 'SimpleCF')

print "Class distribution (SIMPLE overview)"
print "===================================="
print "\t|YY\t|YN\t|NN\t|"
for x in (UNCLASS, SIMPLE):
    print x, "\t|",
    for y in ('YY', 'YN', 'NN'):
        print results[y[0]][y[1]][x], "\t|",
    print
print "u/(s+1)\t|",
for y in ('YY', 'YN', 'NN'):
    print "%.2f\t|" % (results[y[0]][y[1]][UNCLASS] / (results[y[0]][y[1]][SIMPLE]+1.)),
print
print

print "Percentage of simple loops Loopus can bound: %.2f" % percent(SIMPLE)
print "Percentage of unclassified loops Loopus can bound: %.2f" % percent(UNCLASS)
print

print "Class distribution (SIMPLE details)"
print "==================================="
print "\t\t|YY\t|YN\t|NN\t|"
for x in (UNCLASS,) + SIMPLE_DETAILS:
    print "%-8s\t|" % x,
    for y in ('YY', 'YN', 'NN'):
        print results[y[0]][y[1]][x], "\t|",
    print
print

printresult("Multi-exit IntegerIter", 'MultiExitIntegerIter')
distribution('MultiExitIntegerIterIncrSetSize')
printb('AllLoops')
printb('OuterLoop')

distribution('AllLoopsControlVars')
distribution('OuterLoopControlVars')

printresult("Outer loop influenced by inner (inner remains in slice)", 'InfluencedByInner')

print "Amortized Loops"
print "==============="
print
print "Inner loop is IntegerIter loop w/ the exception that"
print "  - it may have >1 arcs leaving the loop, and"
print "  - of the resp. conditions >= 1 takes the IntegerIter form."
print

printresult("Amortized A1 (inner loop counter incremented)", 'AmortA1', '-')
printresult("Amortized A1 I_inner.VD = I_outer.VD", 'AmortA1InnerEqOuter', '-')
printresult("Amortized A2 (inner loop counter never defined)", 'AmortA2', '-')
printresult("Amortized A2 I_inner.VD = I_outer.VD", 'AmortA2InnerEqOuter', '-')
printresult("Amortized B (inner bound is increment-delta of outer)", 'AmortB', '-')
