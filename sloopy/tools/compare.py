#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os.path
import sys
from collections import defaultdict
from datetime import datetime

PRINT_OFFSET = True

if sys.argv[1] == 'cBench':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/cBenchPreprocessed/'
    SLOOPY_FILE = 'classifications_cBench.txt'
    LLVM_ABS_START = '/files/sinn/cBenchPreprocessed/'
    LLVM_FILE = '01072013_preprocessed_summary.txt'
elif sys.argv[1] == 'consumer_jpeg':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/cBenchPreprocessed/'
    SLOOPY_FILE = 'classifications_cBench.txt'
    LLVM_ABS_START = '/files/sinn/cBenchPreprocessed/'
    LLVM_FILE = '01072013_preprocessed_summary.txt'
elif sys.argv[1] == 'wcet':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/wcet/'
    SLOOPY_FILE = 'classifications_wcet.txt'
    LLVM_ABS_START = '/files/sinn/workspace/WCETBench/compiledDebug/'
    LLVM_FILE = 'summaryWCET14062013.txt'
else:
    sys.exit(1)

sf = open(SLOOPY_FILE)
lf = open(LLVM_FILE)

# sloopy_classifications[func][line]
sloopy_classifications = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))
sloopy_classificationsb = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

# class results (count)
results = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))
# distribution results (list of ints)
branch = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))

UNCLASS = 'NotSingleExitSimple'
SIMPLE = 'SingleExitSimple'
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

def parse_sloopy():
    for line in sf:
        filepath, _, func, _, line, _, linesbe, classes = line.split(' ', 7)
        line = int(line)
        linesbe = list(set([ int(linebe) for linebe in linesbe.split(',')[:-1] ]))
        assert(line > 0)
        assert(not 0 in linesbe)

        filepath = './'+os.path.relpath(filepath, SLOOPY_ABS_START)
        classes = classes.split()

        if sloopy_classifications[filepath][func].has_key(line):
            # print >> sys.stderr, "!c", filepath, func, line
            del sloopy_classifications[filepath][func][line]
        else:
            sloopy_classifications[filepath][func][line] = (classes, linesbe)
        for linebe in linesbe:
            sloopy_classificationsb[filepath][func][linebe] = (classes, line)

def classify(sc, term, bound):
    # process classes
    for cls in sc:
        if 'Branch' in cls:
            # Branch-Depth-3-Nodes-6
            c, _, depth, _, nodes = cls.split('-')
            depth, nodes = (int(depth), int(nodes))
            branch[c.split('@')[1]+'depth'][term][bound].append(depth)
            branch[c.split('@')[1]+'nodes'][term][bound].append(nodes)
        elif 'ControlVars' in cls or 'IncrSetSize' in cls or 'Counters' in cls:
            # ControlVars-3
            c, num = cls.split('-')
            num = int(num)
            branch[c.split('@')[1]][term][bound].append(num)
        elif '@' in cls:
            results[term][bound][cls.split('@')[1].split('-')[0]] += 1
            if simplify(sc) == UNCLASS and 'SingleExit' == cls.split('@')[1]:
                results[term][bound]['SingleExitNonSimple'] += 1
    results[term][bound][simplify(sc)] += 1

HEADER_RANGE = 1
BACKEDGE_RANGE = 8

INLINING_MATCHER = {
        ('./security_pgp_d/src/fileio.c', 'ck_dup_output', 1938, 2809) :
        ('./security_pgp_d/src/fileio.c', 'ck_dup_output', 2762, 2809),

        ('./office_ghostscript/src/zchar1.c', 'bbox_finish', 3056, 3260) :
        ('./office_ghostscript/src/zchar1.c', 'bbox_finish', 3244, 3260),

        ('./office_ghostscript/src/zchar1.c', 'type1addpath_continue', 3059, 3299) :
        ('./office_ghostscript/src/zchar1.c', 'type1addpath_continue', 3272, 3299),

        ('./office_ghostscript/src/gp_unifs.c', 'gp_enumerate_files_close', 1215, 1460) :
        ('./office_ghostscript/src/gp_unifs.c', 'gp_enumerate_files_close', 1458, 1460),

        ('./office_ghostscript/src/gdevmem.c', 'gdev_mem_max_height', 1945, 1995):
        ('./office_ghostscript/src/gdevmem.c', 'gdev_mem_max_height', 1994, 1995),

        ('./network_dijkstra/src/dijkstra_large.c', 'dijkstra', 940, 976):
        ('./network_dijkstra/src/dijkstra_large.c', 'dijkstra', 960, 976),

        ('./consumer_mad/src/latin1.c', 'id3_latin1_decode', 819, 836):
        ('./consumer_mad/src/latin1.c', 'id3_latin1_decode', 835, 836),

        ('./consumer_lame/src/mainmpglib.c', 'lame_decode_initfile', 2244, 2266):
        ('./consumer_lame/src/mainmpglib.c', 'lame_decode_initfile', 2261, 2266),

        ('./consumer_jpeg_c/src/rdgif.c', 'start_input_gif', 1670, 0):
        ('./consumer_jpeg_c/src/rdgif.c', 'start_input_gif', 1923, 1964),

        ('./bzip2d/src/bzip2.c', 'compressStream', 3049, 3088):
        ('./bzip2d/src/bzip2.c', 'compressStream', 3077, 3088),

        ('./bzip2d/src/bzip2.c', 'uncompressStream', 3049, 3257):
        ('./bzip2d/src/bzip2.c', 'uncompressStream', 3246, 3257),

        #11

        # BE inlined

        ('./office_ghostscript/src/stream.c', 'spputc', 1396, 1562):
        ('./office_ghostscript/src/stream.c', 'spputc', 1396, 1404),

        ('./office_ghostscript/src/stream.c', 'spgetcc', 1371, 1551):
        ('./office_ghostscript/src/stream.c', 'spgetcc', 1371, 1374),

        ('./office_ghostscript/src/stream.c', 's_std_read_flush', 1227, 1551):
        ('./office_ghostscript/src/stream.c', 's_std_read_flush', 1227, 1231),

        ('./office_ghostscript/src/isave.c', 'combine_space', 2088, 2094):
        ('./office_ghostscript/src/isave.c', 'combine_space', 2088, 2108),

        #4

        # linebreak fix
        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3422, 3619):
        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3422, 3629),

        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3199, 3270):
        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3199, 3280),

        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3110, 3182):
        ('./office_ghostscript/src/gximage2.c', 'image_render_mono', 3110, 3192),

        ('./office_ghostscript/src/gximage0.c', 'gx_default_image_data', 2261, 2274):
        ('./office_ghostscript/src/gximage0.c', 'gx_default_image_data', 2261, 2286),

        ('./office_ghostscript/src/gscie.c', 'gs_cie_render_complete', 3765, 3775):
        ('./office_ghostscript/src/gscie.c', 'gs_cie_render_complete', 3765, 3788),

        ('./office_ghostscript/src/siscale.c', 's_IScale_process', 2094, 2168):
        ('./office_ghostscript/src/siscale.c', 's_IScale_process', 2094, 2180),

        #6
    }

def search_match(lookup_filename, func, line, linebe, term, bound):
    t = (lookup_filename, func, line, linebe)
    if INLINING_MATCHER.has_key(t):
        lookup_filename, func, line, linebe = INLINING_MATCHER[t]

    for d in range(line):
        if not sloopy_classifications[lookup_filename][func].has_key(line-d):
            continue

        sc, slinesbe = sloopy_classifications[lookup_filename][func][line-d]

        # seems we found a match, find nearest backedge
        if linebe and linebe - line > 4:
            try:
                min_value = min(slinebe-linebe for slinebe in slinesbe if slinebe-linebe >= 0)

                # check if the found backedge is plausible
                if min_value > BACKEDGE_RANGE:
                    continue
            except ValueError:
                # no backedge match
                continue

        # we found a match, so stop looking
        classify(sc, term, bound)
        return True

    return False

def search_backedge_match(lookup_filename, func, line, linebe, term, bound):
    t = (lookup_filename, func, line, linebe)
    if INLINING_MATCHER.has_key(t):
        lookup_filename, func, line, linebe = INLINING_MATCHER[t]

    for d in range(BACKEDGE_RANGE+1):
        if not sloopy_classificationsb[lookup_filename][func].has_key(linebe+d):
            continue

        sc, sline = sloopy_classificationsb[lookup_filename][func][linebe+d]

        # check if the found header is plausible
        if line-sline > HEADER_RANGE:
            continue

        # we found a match, so stop looking
        classify(sc, term, bound)
        return True

    return False

def parse_loopus():
    for l in lf:
        term, bound, line, linebe, func, filepath = l.split()
        line = int(line)
        linebe = int(linebe)

        if LLVM_ABS_START:
            filepath = './'+os.path.relpath(filepath, LLVM_ABS_START)
        assert(line > 0)

        fileName, _ = os.path.splitext(filepath)
        lookup_filename = fileName + '.c'

        if search_match(lookup_filename, func, line, linebe, term, bound):
            continue
        if search_backedge_match(lookup_filename, func, line, linebe, term, bound):
            continue

        print >> sys.stderr, '!0', lookup_filename, func, line, linebe

parse_sloopy()
parse_loopus()

sys.exit(0)

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
CLASS_LIST2 = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, sys.maxint)]

def distribution(desc, type, subtype=('',), class_list=CLASS_LIST):
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
        for r in class_list:
            if (r[1] - r[0]) == 1:
                print "%s = %d \t|" % (x if x else 'cnt', r[0]),
            else:
                print "%s [%d, %s)\t|" % (x if x else 'cnt', r[0], str(r[1]) if r[1] != sys.maxint else "inf"),
            for y in ('YY', 'YN', 'NN'):
                s = countif(type+x, r, y)
                print "%d\t%s" % (s, '║' if y == 'NN' else '|'),
            try:
                print "%.1f\t|" % (100.*countif(type+x, r, 'YY')/countall(type+x, r)),
            except ZeroDivisionError:
                print "%.1f\t|" % (100.),
            print countall(type+x, r)
        print "\t\t\t\t\t\t ", sum([countall(type+x, r) for r in class_list]), "(TOTAL)"
        print

printresult("Single Exit vs. Multi Exit", ('MultiExit', 'SingleExit'), crosssum=False)

printh("(Single Exit) Simple Loops", "Single exit that takes the simple form.\n")

printresult("Simple Loops ⊆ Single Exit", ('SingleExit', SIMPLE, 'SingleExitNonSimple'), crosssum=False)
printresult("Simple Loop vs. Non-Simple (= non-simple single exit, or multi exit) (overview)", (UNCLASS, SIMPLE))
printresult("Simple Loop vs. Non-Simple (= non-simple single exit, or multi exit) (class details)", (UNCLASS,) + SIMPLE_DETAILS)
print "LP ... Loopus Performance, % of loops in the resp. class for which Loopus computes a bound.\n"


printh("(Multi Exit) Simple Loops", "Multiple exits, where ALL take the simple form.\n")

print "Still TODO...\n"


printh("Semi-simple Loops", "Multiple exits, where SOME take the simple form.\n")

printresult("Semi-simple Loops ⊆ Multi Exit", ('MultiExit', 'MultiExitSimple'), crosssum=False)
printresult("Semi-simple Loops vs. Non-semi-simple (overview)", ('MultiExitNonSimple', 'MultiExitSimple'))
printresult("Semi-simple Loops vs. Non-semi-simple (class details)", ('MultiExitNonSimple', 'MultiExitIntegerIter', 'MultiExitDataIter', 'MultiExitAArrayIter', 'MultiExitPArrayIter'))
distribution("MultiExit Unique Increments [= triples (counter-var, bound, delta) ]", 'MultiExitIncrSetSize', class_list=CLASS_LIST2)
distribution("MultiExit Counter Variables", 'MultiExitCounters', class_list=CLASS_LIST2)
# distribution("MultiExitIntegerIter Increment Variables", 'MultiExitIntegerIterIncrSetSize')
# distribution("MultiExitDataIter Increment Variables", 'MultiExitDataIterIncrSetSize')
# distribution("MultiExitAArrayIter Increment Variables", 'MultiExitAArrayIterIncrSetSize')
# distribution("MultiExitPArrayIter Increment Variables", 'MultiExitPArrayIterIncrSetSize')


printh("Influencing/-ed loops")

printresult("Outer loop influenced by inner (inner remains in slice)", 'InfluencedByInner')
printresult("Inner loop influences outer (inner remains in slice)", 'InfluencesOuter')


printh("Amortized Loops")

printresult("Amortized A1 (inner loop counter incremented)", ('AmortA1', 'AmortA1InnerEqOuter'), crosssum=False)
printresult("Amortized A2 (inner loop counter never defined)", ('AmortA2', 'AmortA2InnerEqOuter'), crosssum=False)
printresult("Amortized B (inner bound is increment-delta of outer)", 'AmortB')


printh("Control Flow")

distribution("Branching (all loop slice)", 'AllLoopsBranch', ('depth', 'nodes'))
distribution("Branching (outer loop slice)", 'OuterLoopBranch', ('depth', 'nodes'))

distribution("Control Variables (all loop slice)", 'AllLoopsControlVars')
distribution("Control Variables (outer loop slice)", 'OuterLoopControlVars')
