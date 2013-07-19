MKCHARTS=$0
BENCH=$1
SUBBENCH=$2
BENCH_DIR="./bench"

function timeit {
    t0=`date +%s`
    $@
    t1=`date +%s`
    echo Elapsed $[$t1-$t0]
}

function runall {
    $MKCHARTS cBench $@ || exit $?
    $MKCHARTS coreutils $@ || exit $?
    $MKCHARTS svcomp $@ || exit $?
    $MKCHARTS wcet $@ || exit $?
}

function runall_svcomp {
    for set in $(find $BENCH_DIR -name '*.set' -and -not -name 'DeviceDrivers64.set' | xargs basename -s .set) ; do
        $0 svcomp $set
    done
}

# all target
if [[ ${BENCH} == "all" ]] ; then
    shift
    timeit runall
    exit
fi

# individual targets
if [[ ${BENCH} == "coreutils" ]] ; then
    # TODO
    FILES=$(find ${BENCH_DIR}/coreutils-8.21/src/ -not -name tac-pipe.c -and -not -name yes.c -and -name '*.c')
    INCLUDES=-I${BENCH_DIR}/coreutils-8.21/lib/ 
    DEFINES="-DHASH_ALGO_MD5"
elif [[ ${BENCH} == "svcomp" ]] ; then
    if [[ -z $SUBBENCH ]] ; then
        timeit runall_svcomp
        exit
    fi
    FILES=$(cat ${BENCH_DIR}/svcomp13/${SUBBENCH}.set | sed '/^\s*$/d' | sed 's!^!'${BENCH_DIR}/svcomp13/'!')
    SLOOPY_ARGS="-allow-infinite-loops"
    CC1_ARGS="-target x86_64"
elif [[ ${BENCH} == "wcet" ]] ; then
    # TODO
    FILES=$(find ${BENCH_DIR}/wcet/ -not -name des.c -and -name '*.c')
elif [[ ${BENCH} == "cBench" ]] ; then
    FILES=$(find ${BENCH_DIR}/cBench_preprocessed_2013_07_01_Merged/${SUBBENCH} -name '*.c')
fi

# ensure sloopy is built
ninja sloopy || exit 1

SLOOPY_ARGS="$SLOOPY_ARGS -loop-stats"
[[ -z $SUBBENCH ]] && BENCH_NAME="${BENCH}" || BENCH_NAME="${BENCH}_${3}"
timeit bin/sloopy $FILES ${SLOOPY_ARGS} -debug -bench-name "$BENCH_NAME" -- -w $INCLUDES $DEFINES $CC1_ARGS 2>&1 | egrep -v '^Args:'
