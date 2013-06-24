#!/usr/bin/env python

import os.path
import sys
import operator
from collections import defaultdict

if sys.argv[1] == 'cBench':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/cBench15032013/'
    SLOOPY_FILE = 'classifications_cBench.txt'
    LLVM_ABS_START = None
    LLVM_FILE = 'summary_14062013.txt'
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
cvars = defaultdict(lambda: defaultdict(list))

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
    term, bound, line, func, filepath = line.split()
    line = int(line)

    if LLVM_ABS_START:
        filepath = './'+os.path.relpath(filepath, LLVM_ABS_START)
    if line == 0:
        continue

    fileName, fileExtension = os.path.splitext(filepath)
    lookup_filename = fileName + '.c'

    ok = False
    for d in range(line):
        def x(op):
            sc = sloopy_classifications[lookup_filename][func][op(line, d)]
            if sc:
                # opname = '+' if op == operator.add else '-'
                # print opname+str(d), filepath, func, line, term, bound, sc
                for cls in sc:
                    if 'Branch' in cls:
                        # Branch-Depth-3-Nodes-6
                        _, _, depth, _, nodes = cls.split('-')
                        depth, nodes = (int(depth), int(nodes))
                        if depth > 500:
                            continue
                        branch[term][bound]['depth'].append(depth)
                        branch[term][bound]['nodes'].append(nodes)
                    elif 'ControlVars' in cls:
                        # ControlVars-3
                        _, num = cls.split('-')
                        num = int(num)
                        if num > 200:
                            continue
                        cvars[term][bound].append(num)
                    elif '@' in cls:
                        results[term][bound][cls.split('@')[1]] += 1
                results[term][bound][simplify(sc)] += 1
                return True
        if x(operator.__sub__):
            ok = True
            break
        # if x(operator.__add__):
        #     ok = True
        #     break
    if not ok:
        print >> sys.stderr, '!!', filepath, func, line

print "==================================="
print sys.argv[1]
print "==================================="
print

print "Class distribution (SIMPLE overview)"
print "==================================="
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

allU = sum([ results[y[0]][y[1]][UNCLASS] for y in ('YY', 'YN', 'NN') ])
allS = sum([ results[y[0]][y[1]][SIMPLE] for y in ('YY', 'YN', 'NN') ])
yyU = sum([ results[y[0]][y[1]][UNCLASS] for y in ('YY',) ])
yyS = sum([ results[y[0]][y[1]][SIMPLE] for y in ('YY',) ])
nU = sum([ results[y[0]][y[1]][UNCLASS] for y in ('YN', 'NN') ])
nS = sum([ results[y[0]][y[1]][SIMPLE] for y in ('YN', 'NN') ])
print "Percentage of simple loops Loopus can bound: %.2f" % (1.*yyS/allS)
print "Percentage of unclassified loops Loopus can bound: %.2f" % (1.*yyU/allU)
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


def average(s):
    return sum(s) * 1.0 / len(s)
def stdev(s):
    avg = average(s)
    variance = map(lambda x: (x - avg)**2, s)
    import math
    standard_deviation = math.sqrt(average(variance))
    return standard_deviation

print "Branching (sliced)"
print "=================="
for x in ('depth', 'nodes'):
    print "\t\t|YY\t|YN\t|NN\t|"
    print "avg(%s)\t|" % x,
    for y in ('YY', 'YN', 'NN'):
        l = branch[y[0]][y[1]][x]
        print "%.2f\t|" % average(l),
    print
    print "stdev(%s)\t|" % x,
    for y in ('YY', 'YN', 'NN'):
        l = branch[y[0]][y[1]][x]
        print "%.2f\t|" % stdev(l),
    print
print


print "Simple Control Flow (EXIT block has 1 pred)"
print "==========================================="
print "YY\t|YN\t|NN\t|"
for y in ('YY', 'YN', 'NN'):
    print "%d\t|" % results[y[0]][y[1]]['SIMPLE'],
print
print

print "Control Vars (sliced)"
print "====================="
print "\t\t|YY\t|YN\t|NN\t|"
print "avg(count)\t|",
for y in ('YY', 'YN', 'NN'):
    l = cvars[y[0]][y[1]]
    print "%.2f\t|" % average(l),
print
print "stdev(count)\t|",
for y in ('YY', 'YN', 'NN'):
    l = cvars[y[0]][y[1]]
    print "%.2f\t|" % stdev(l),
print
print

print "Amortized"
print "========="
print "YY\t|YN\t|NN\t|"
for y in ('YY', 'YN', 'NN'):
    print "%d\t|" % results[y[0]][y[1]]['AmortA'],
print
print
