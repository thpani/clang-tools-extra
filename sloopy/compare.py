#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os.path
import sys
from collections import defaultdict
from datetime import datetime

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

UNCLASS = 'nonsimple'
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
                elif 'ControlVars' in cls or 'IncrSetSize' in cls:
                    # ControlVars-3
                    c, num = cls.split('-')
                    num = int(num)
                    if num > 200:
                        continue
                    branch[c.split('@')[1]][term][bound].append(num)
                elif '@' in cls:
                    results[term][bound][cls.split('@')[1].split('-')[0]] += 1
            results[term][bound][simplify(sc)] += 1
            ok = True
            break
    if not ok:
        print >> sys.stderr, '!!', filepath, func, line

print "==================================="
print sys.argv[1]
print "==================================="
print
print "generated", datetime.now()
print

def countif(type, r, y):
    return sum([1 if r[0] <= depth and depth < r[1] else 0 for depth in branch[type][y[0]][y[1]]])

def countall(type, r):
    return sum([countif(type, r, y) for y in ('YY', 'YN', 'NN')])

def sumall(c):
    return sum([ results[y[0]][y[1]][c] for y in ('YY', 'YN', 'NN') ])

def percent(c):
    return 100. * results['Y']['Y'][c] / sumall(c)

def average(s):
    return sum(s) * 1.0 / len(s)

def stdev(s):
    avg = average(s)
    variance = map(lambda x: (x - avg)**2, s)
    import math
    standard_deviation = math.sqrt(average(variance))
    return standard_deviation

def printh(desc, expl=None):
    print
    print desc
    print "=" * len(desc)
    print
    if expl:
        print expl

def printresult(desc, key, crosssum=True):
    print desc
    print '-' * len(desc)

    if not hasattr(key, '__iter__'):
        key = (key,)

    longest_key = max([len(x) for x in key])

    print ''.ljust(longest_key), "\t| YY\t| YN\t| NN\t║ LP\t| Σ"
    for x in key:
        print x.ljust(longest_key), "\t|",
        for y in ('YY', 'YN', 'NN'):
            print "%2d\t%s" % (results[y[0]][y[1]][x], '║' if y == 'NN' else '|'),
        print "%.1f\t|" % percent(x),
        if crosssum or x == key[0]:
            print sumall(x)
        else:
            print sumall(x), "/", sumall(key[0]), "(= %.1f%%)" % (100.*sumall(x)/sumall(key[0]))
    if crosssum:
        if len(key) > 1:
            print ''.ljust(longest_key), "\t\t\t\t\t ", sum([sumall(x) for x in key]), "(TOTAL)"
    print

CLASS_LIST = [(0, 1), (1, 2), (2, 3), (3, 10), (10, sys.maxint)]

def distribution(desc, type, subtype=('',)):
    print desc
    print "-" * len (desc)
    for x in subtype:
        print "\t\t| YY\t| YN\t| NN\t║ LP\t| Σ"
        print "avg(%s)\t|" % (x if x else 'count'),
        for y in ('YY', 'YN', 'NN'):
            l = branch[type+x][y[0]][y[1]]
            print "%.2f\t%s" % (average(l), '║' if y == 'NN' else '|'),
        print
        print "stdev(%s)\t|" % (x if x else 'count'),
        for y in ('YY', 'YN', 'NN'):
            l = branch[type+x][y[0]][y[1]]
            print "%.2f\t%s" % (stdev(l), '║' if y == 'NN' else '|'),
        print
        for r in CLASS_LIST:
            if (r[1] - r[0]) == 1:
                print "%s = %d \t|" % (x if x else 'cnt', r[0]),
            else:
                print "%s [%d, %s)\t|" % (x if x else 'cnt', r[0], str(r[1]) if r[1] != sys.maxint else "inf"),
            for y in ('YY', 'YN', 'NN'):
                s = countif(type+x, r, y)
                print "%d\t%s" % (s, '║' if y == 'NN' else '|'),
            print "%.1f\t|" % (100.*countif(type+x, r, 'YY')/countall(type+x, r)),
            print countall(type+x, r)
        print "\t\t\t\t\t\t ", sum([countall(type+x, r) for r in CLASS_LIST]), "(TOTAL)"
        print


printh("(Single Exit) Simple Loops", "Single exit that takes the simple form.\n")

printresult("Single Exit vs. Multi Exit", ('SingleExit', 'MultiExit'))
printresult("Simple Loops ⊆ Single Exit", ('SingleExit', SIMPLE), crosssum=False)
printresult("Simple Loop vs. Non-Simple (= non-simple single exit, or multi exit) (overview)", (UNCLASS, SIMPLE))
printresult("Simple Loop vs. Non-Simple (= non-simple single exit, or multi exit) (class details)", (UNCLASS,) + SIMPLE_DETAILS)
print "LP ... Loopus Performance, % of loops in the resp. class for which Loopus computes a bound.\n"


printh("(Multi Exit) Simple Loops", "Multiple exits, where ALL take the simple form.\n")

print "Still TODO...\n"


printh("Semi-simple Loops", "Multiple exits, where SOME take the simple form.\n")

printresult("Multi-exit Loops", ('MultiExitIntegerIter', 'MultiExitPArrayIter'))
print "MultiExitAArrayIter, MultiExitDataIter are still TODO...\n"
distribution("MultiExitIntegerIter Increment Variables", 'MultiExitIntegerIterIncrSetSize')
distribution("MultiExitPArrayIter Increment Variables", 'MultiExitPArrayIterIncrSetSize')


printh("Influencing/-ed loops")

printresult("Outer loop influenced by inner (inner remains in slice)", 'InfluencedByInner')
printresult("Inner loop influences outer (inner remains in slice)", 'InfluencesOuter')


printh("Amortized Loops (for multi-exit IntegerIter loops ONLY)")

printresult("Amortized A1 (inner loop counter incremented)", ('AmortA1', 'AmortA1InnerEqOuter'), crosssum=False)
printresult("Amortized A2 (inner loop counter never defined)", ('AmortA2', 'AmortA2InnerEqOuter'), crosssum=False)
printresult("Amortized B (inner bound is increment-delta of outer)", 'AmortB')


printh("Control Flow")

distribution("Branching (all loop slice)", 'AllLoopsBranch', ('depth', 'nodes'))
distribution("Branching (outer loop slice)", 'OuterLoopBranch', ('depth', 'nodes'))

distribution("Control Variables (all loop slice)", 'AllLoopsControlVars')
distribution("Control Variables (outer loop slice)", 'OuterLoopControlVars')
