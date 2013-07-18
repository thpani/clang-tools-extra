#!/usr/bin/env python
# -*- coding: utf-8 -*-

import json
import os.path
import sys
from collections import defaultdict
from datetime import datetime

INCR_DETAILS = ('IntegerIter', 'DataIter', 'AArrayIter', 'PArrayIter')

sloopy_classifications = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))
sloopy_classificationsb = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

if sys.argv[1] == 'cBench':
    SLOOPY_ABS_START = '/Users/thomas/Documents/uni/da/llvm-build/bench/cBench_preprocessed_2013_07_01_Merged/'
    SLOOPY_FILE = 'classifications_cBench.txt'
    LLVM_ABS_START = '/files/sinn/cBenchPreprocessed/'
    LLVM_FILE = 'bench/cBench_preprocessed_2013_07_01_Merged_summary.txt'
else:
    sys.exit(1)

def parse_sloopy():
    loops = json.load(open(SLOOPY_FILE))
    for loop in loops:
        filepath, _, func, _, line, _, linesbe = loop['Location'].split(' ', 6)
        line = int(line)
        linesbe = list(set([ int(linebe) for linebe in linesbe.split(',')[:-1] ]))
        assert(line > 0)
        assert(not 0 in linesbe)

        filepath = './'+os.path.relpath(filepath, SLOOPY_ABS_START)

        if sloopy_classifications[filepath][func].has_key(line):
            # print >> sys.stderr, "!c", filepath, func, line
            del sloopy_classifications[filepath][func][line]
        else:
            sloopy_classifications[filepath][func][line] = (loop, linesbe)
        for linebe in linesbe:
            sloopy_classificationsb[filepath][func][linebe] = (loop, line)

loops = list()
def classify(sc, term, bound):
    sc['LP'] = term+bound
    loops.append(sc)

HEADER_RANGE = 0
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

        # ???!?!?
        ('./office_ghostscript/src/gsutil.c', 'string_match', 625, 603):
        ('./office_ghostscript/src/gsutil.c', 'string_match', 598, 603),
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
    unmatched = 0

    for l in open(LLVM_FILE):
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

        unmatched += 1

    return unmatched

parse_sloopy()
unmatched = parse_loopus()

SELECTORS = {
    # predicates
    'ANY': lambda l: True,
    'SingleExit': lambda l: l['Exits'] == 1,
    'Not(SingleExit)': lambda l: l['Exits'] != 1,

    'FOR': lambda l: l['Stmt'] == 'FOR',
    'WHILE': lambda l: l['Stmt'] == 'WHILE',
    'DO': lambda l: l['Stmt'] == 'DO',
    'GOTO': lambda l: l['Stmt'] == 'GOTO',

    # selectors
    'Exits': lambda l, r=None: r[0] <= l['Exits'] < r[1] if r else l['Exits']
}

LOOP_STMT = ('FOR', 'WHILE', 'DO', 'GOTO')
for exit in ('SingleExit', 'StrongSingleExit'):
    SELECTORS[exit+'.Simple'] = lambda l, exit=exit: l['Exits'] == 1 and l[exit]['Simple']
    SELECTORS[exit+'.Not(Simple)'] = lambda l, exit=exit: l['Exits'] == 1 and not l[exit]['Simple']
    for loop_stmt in LOOP_STMT:
        SELECTORS[exit+'.Simple'+' '+loop_stmt] = lambda l, exit=exit, loop_stmt=loop_stmt: l['Exits'] == 1 and l[exit]['Simple'] and l['Stmt'] == loop_stmt
        SELECTORS[exit+'.Not(Simple)'+' '+loop_stmt] = lambda l, exit=exit, loop_stmt=loop_stmt: l['Exits'] == 1 and not l[exit]['Simple'] and l['Stmt'] == loop_stmt
    for incr_detail in INCR_DETAILS:
        SELECTORS[exit+'.'+incr_detail] = lambda l, exit=exit, incr_detail=incr_detail: l['Exits'] == 1 and not l[exit][incr_detail].startswith('!')
for exit in ('MultiExit', 'StrongMultiExit'):
    SELECTORS[exit+'.Counters'] = lambda l, r=None, exit=exit: r[0] <= l[exit]['Counters'] < r[1] if r else l[exit]['Counters']
    SELECTORS[exit+'.Simple'] = lambda l, exit=exit: l[exit]['Simple']
    SELECTORS[exit+'.Not(Simple)'] = lambda l, exit=exit: not l[exit]['Simple']
    for loop_stmt in LOOP_STMT:
        SELECTORS[exit+'.Simple'+' '+loop_stmt] = lambda l, exit=exit, loop_stmt=loop_stmt: l[exit]['Simple'] and l['Stmt'] == loop_stmt
        SELECTORS[exit+'.Not(Simple)'+' '+loop_stmt] = lambda l, exit=exit, loop_stmt=loop_stmt: not l[exit]['Simple'] and l['Stmt'] == loop_stmt
    for incr_detail in INCR_DETAILS:
        SELECTORS[exit+'.'+incr_detail] = lambda l, exit=exit, incr_detail=incr_detail: not l[exit][incr_detail].startswith('!')
for prop in ('InfluencedByInner', 'StronglyInfluencedByInner', 'InfluencesOuter', 'StronglyInfluencesOuter', 'AmortA1', 'AmortA1InnerEqOuter', 'WeakAmortA1', 'WeakAmortA1InnerEqOuter', 'AmortA2', 'AmortA2InnerEqOuter', 'AmortB'):
    SELECTORS[prop] = lambda l, prop=prop: l.has_key(prop) and l[prop]
for key in ('AllLoops', 'OuterLoop'):
    for subkey in ('BranchDepth', 'BranchNodes', 'ControlVars'):
        SELECTORS[key+'.'+subkey] = lambda l, r=None, key=key, subkey=subkey: r[0] <= l[key][subkey] < r[1] if r else l[key][subkey]

def select2(p, r=None):
    if isinstance(p, basestring):
        selector = SELECTORS[p]
    else:
        selector = p
    if r:
        return (l for l in loops if selector(l,r))
    return (l for l in loops if selector(l))

def select(p, r=None):
    yy = [ l for l in select2(p, r) if l['LP'] == 'YY' ]
    yn = [ l for l in select2(p, r) if l['LP'] == 'YN' ]
    nn = [ l for l in select2(p, r) if l['LP'] == 'NN' ]
    return yy, yn, nn, yy+yn+nn

def div0(a, b, default=0):
    try:
        return 1.*a/b
    except ZeroDivisionError:
        return default

def select_count(p, of=None, range=None):
    yy, yn, nn, all = ( len(l) for l in select(p, range) )
    if of:
        count_of = select_count(of)
        return yy, yn, nn, div0(100.*yy, all), div0(100.*(yy+yn), all), yy+yn+nn, count_of[5], div0(100.*all,count_of[5])
    else:
        return yy, yn, nn, div0(100.*yy, all), div0(100.*(yy+yn), all), yy+yn+nn

def avg(s):
    return div0(sum(s), len(s))

def stdev(s):
    a = avg(s)
    variance = map(lambda x: (x - a)**2, s)
    import math
    standard_deviation = math.sqrt(avg(variance))
    return standard_deviation

def select_f(p, f):
    return ( f([SELECTORS[p](l) for l in list]) for list in select(p) )

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

    total = 0
    print ''.ljust(longest_key), "\t| YY\t| YN\t| NN\t║ LPB\t| LPT\t| Σ\t|"
    for x in key:
        print x.ljust(longest_key), "\t|",
        result = select_count(x, key[0] if not crosssum else None)
        for i, r in enumerate(result[:5]):
            fmt_str = "%2d\t%s" if i <= 2 else "%.1f\t%s"
            print fmt_str % (r, '║' if i == 2 else '|'),
        if crosssum or x == key[0]:
            print result[5]
        else:
            print result[5], "/", result[6], "(= %.1f%%)" % result[7]
        total += result[5]

    if crosssum:
        if len(key) > 1:
            print ''.ljust(longest_key), "\t\t\t\t\t\t ", total, "(TOTAL)"
    print

CLASS_LIST = [(0, 1), (1, 2), (2, 3), (3, 10), (10, sys.maxint)]
CLASS_LIST2 = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, sys.maxint)]

def distribution(desc, type, subtypes=('',), class_list=CLASS_LIST):
    print desc
    print "-" * len (desc)
    for subtype in subtypes:
        t = type + '.' + subtype if subtype else type
        longest_key = len(subtype if subtype else 'count')+7
        print ''.ljust(longest_key), "\t| YY\t| YN\t| NN\t║ LPB\t| LPT\t| Σ\t|"
        print "avg(%s)\t|" % (subtype if subtype else 'count'),
        for i, a in enumerate(select_f(t, avg)):
            print "%.2f\t%s" % (a, '║ n/a\t| n/a\t|' if i == 2 else '|'),
        print
        print "stdev(%s)\t|" % (subtype if subtype else 'count'),
        for i, a in enumerate(select_f(t, stdev)):
            print "%.2f\t%s" % (a, '║ n/a\t| n/a\t|' if i == 2 else '|'),
        print
        for r in class_list:
            if (r[1] - r[0]) == 1:
                print "%s = %d \t|" % (subtype if subtype else 'cnt', r[0]),
            else:
                print "%s [%d, %s)\t|" % (subtype if subtype else 'cnt', r[0], str(r[1]) if r[1] != sys.maxint else "inf"),
            result = select_count(t, range=r)
            for i, r in enumerate(result):
                fmt_str = "%2d\t%s" if i <= 2 or i == 5 else "%.1f\t%s"
                print fmt_str % (r, '║' if i == 2 else '|'),
            print
        total = sum ((select_count(t, range=r))[5] for r in class_list)
        print "\t\t\t\t\t\t\t ", total, "(TOTAL)"
        print

if len(sys.argv) == 3:
    # for loop in select2(lambda l: l['Exits'] == 1 and l['Stmt'] == 'GOTO' and not l['SingleExit']['Simple']):
    for loop in select2(lambda l: eval(sys.argv[2])):
        print loop['Location']
    sys.exit(0)

print "==================================="
print sys.argv[1]
print "==================================="
print
print "generated", datetime.now()
print "loops matched: %d, unmatched: %d" % (len(loops), unmatched)
print
print "LPT ... Loopus Performance Termination [ (YY+YN)/(YY+YN+NN) ]"
print "LPB ... Loopus Performance Bound [ YY/(YY+YN+NN) ]"
print

printresult("Single Exit vs. Multi Exit", ('ANY', 'SingleExit', 'Not(SingleExit)'), crosssum=False)
distribution("Exits", 'Exits')
printresult("Statements", ('ANY',) + LOOP_STMT, crosssum=False)

printh("(Single Exit) Simple Loops", "Single exit that takes the simple form.\n")

printresult("Simple Loops ⊆ Single Exit", ('SingleExit', 'SingleExit.Not(Simple)', 'SingleExit.Simple'), crosssum=False)
printresult("Simple Loops (class details)", ('Not(SingleExit)', 'SingleExit.Not(Simple)') + tuple(('SingleExit.'+x for x in INCR_DETAILS)))
printresult("Statements", ('SingleExit.Simple', 'SingleExit.Simple FOR', 'SingleExit.Simple WHILE', 'SingleExit.Simple DO', 'SingleExit.Simple GOTO'), crosssum=False)
printresult("Statements", ('SingleExit.Not(Simple)', 'SingleExit.Not(Simple) FOR', 'SingleExit.Not(Simple) WHILE', 'SingleExit.Not(Simple) DO', 'SingleExit.Not(Simple) GOTO'), crosssum=False)

printh("Strong (Single Exit) Simple Loops", "Single exit that takes the simple form AND exactly 1 increment on each path.\n")

printresult("Strong Simple Loops ⊆ Single Exit", ('SingleExit', 'StrongSingleExit.Not(Simple)', 'StrongSingleExit.Simple'), crosssum=False)
printresult("Strong Simple Loops ⊆ Simple Loops", ('SingleExit.Simple', 'StrongSingleExit.Simple'), crosssum=False)
printresult("Strong Simple Loops (class details)", ('Not(SingleExit)', 'StrongSingleExit.Not(Simple)') + tuple(('StrongSingleExit.'+x for x in INCR_DETAILS)))
printresult("Statements", ('StrongSingleExit.Simple', 'StrongSingleExit.Simple FOR', 'StrongSingleExit.Simple WHILE', 'StrongSingleExit.Simple DO', 'StrongSingleExit.Simple GOTO'), crosssum=False)
printresult("Statements", ('StrongSingleExit.Not(Simple)', 'StrongSingleExit.Not(Simple) FOR', 'StrongSingleExit.Not(Simple) WHILE', 'StrongSingleExit.Not(Simple) DO', 'StrongSingleExit.Not(Simple) GOTO'), crosssum=False)

printh("Semi-simple Loops", "Multiple exits, where SOME take the simple form.\n")

printresult("Semi-simple Loops ⊆ Multi Exit", ('ANY', 'MultiExit.Not(Simple)', 'MultiExit.Simple'), crosssum=False)
printresult("Semi-simple Loops (class details)", ('MultiExit.Not(Simple)',) +  tuple(('MultiExit.'+x for x in INCR_DETAILS)))
distribution("Semi-simple Loop Counter Variables", 'MultiExit.Counters', class_list=CLASS_LIST2)
printresult("Statements", ('MultiExit.Simple', 'MultiExit.Simple FOR', 'MultiExit.Simple WHILE', 'MultiExit.Simple DO', 'MultiExit.Simple GOTO'), crosssum=False)
printresult("Statements", ('MultiExit.Not(Simple)', 'MultiExit.Not(Simple) FOR', 'MultiExit.Not(Simple) WHILE', 'MultiExit.Not(Simple) DO', 'MultiExit.Not(Simple) GOTO'), crosssum=False)

printh("Strong Semi-simple Loops", "Multiple exits, where SOME take the simple form AND exactly 1 increment on each path.\n")

printresult("Strong Semi-simple Loops ⊆ Multi Exit", ('ANY', 'StrongMultiExit.Not(Simple)', 'StrongMultiExit.Simple'), crosssum=False)
printresult("Strong Semi-simple Loops ⊆ Semi-simple Loops", ('MultiExit.Simple', 'StrongMultiExit.Simple'), crosssum=False)
printresult("Strong Semi-simple Loops (class details)", ('StrongMultiExit.Not(Simple)',) +  tuple(('StrongMultiExit.'+x for x in INCR_DETAILS)))
distribution("Strong Semi-simple Loop Counter Variables", 'StrongMultiExit.Counters', class_list=CLASS_LIST2)
printresult("Statements", ('StrongMultiExit.Simple', 'StrongMultiExit.Simple FOR', 'StrongMultiExit.Simple WHILE', 'StrongMultiExit.Simple DO', 'StrongMultiExit.Simple GOTO'), crosssum=False)
printresult("Statements", ('StrongMultiExit.Not(Simple)', 'StrongMultiExit.Not(Simple) FOR', 'StrongMultiExit.Not(Simple) WHILE', 'StrongMultiExit.Not(Simple) DO', 'StrongMultiExit.Not(Simple) GOTO'), crosssum=False)

printh("Influencing/-ed loops")

printresult("Outer loop influenced by inner (inner remains in slice)", ('InfluencedByInner', 'StronglyInfluencedByInner'), crosssum=False)
printresult("Inner loop influences outer (inner remains in slice)", ('InfluencesOuter', 'StronglyInfluencesOuter'), crosssum=False)


printh("Amortized Loops")

printresult("Amortized A1 (inner loop counter incremented)", ('AmortA1', 'AmortA1InnerEqOuter'), crosssum=False)
printresult("Weak Amortized A1 (inner loop counter incremented)", ('WeakAmortA1', 'WeakAmortA1InnerEqOuter'), crosssum=False)
printresult("Amortized A2 (inner loop counter never defined)", ('AmortA2', 'AmortA2InnerEqOuter'), crosssum=False)
printresult("Amortized B (inner bound is increment-delta of outer)", 'AmortB')


printh("Control Flow")

distribution("Branching (all loop slice)", 'AllLoops', ('BranchDepth', 'BranchNodes'))
distribution("Branching (outer loop slice)", 'OuterLoop', ('BranchDepth', 'BranchNodes'))

distribution("Control Variables (all loop slice)", 'AllLoops.ControlVars')
distribution("Control Variables (outer loop slice)", 'OuterLoop.ControlVars')
