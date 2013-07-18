BENCH=$1
BENCH_DIR="$(pwd)/bench"
TIMESTAMP=$(date "+%Y-%m-%d_%H:%M")

if [[ ${BENCH} == "all" ]] ; then
    shift
    shift
    "$0" coreutils $@ || exit $?
    "$0" cBench $@ || exit $?
    "$0" wcet $@ || exit $?
    exit
fi

if [[ $2 == "run" ]] ; then
    if [[ ${BENCH} == "coreutils" ]] ; then
        FILES=$(find ${BENCH_DIR}/coreutils-8.21/src/ -not -name tac-pipe.c -and -not -name yes.c -and -name '*.c')
        INCLUDES=-I${BENCH_DIR}/coreutils-8.21/lib/ 
        DEFINES="-DHASH_ALGO_MD5"
    elif [[ ${BENCH} == "svcomp" ]] ; then
        if [[ -z $3 ]] ; then
            for set in $(find bench -name 'DeviceDrivers64.set' | xargs basename -s .set) ; do
                $0 svcomp run $set
            done
            exit
        fi
        FILES=$(cat ${BENCH_DIR}/svcomp13/$3.set | sed '/^\s*$/d' | sed 's!^!'${BENCH_DIR}/svcomp13/'!')
        SLOOPY_ARGS="-ignore-infinite-loops"
        CC1_ARGS="-target x86_64"
    elif [[ ${BENCH} == "wcet" ]] ; then
        FILES=$(find ${BENCH_DIR}/wcet/ -not -name des.c -and -name '*.c')
    elif [[ ${BENCH} == "cBench" ]] ; then
        FILES=$(find ${BENCH_DIR}/cBench_preprocessed_2013_07_01_Merged/$3 -name '*.c')
    fi

    ninja sloopy || exit 1
    SLOOPY_ARGS="$SLOOPY_ARGS -loop-stats"
    t0=`date +%s`
    [[ -z $3 ]] && BENCH_NAME="${BENCH}" || BENCH_NAME="${BENCH}_${3}"
    bin/sloopy $FILES ${SLOOPY_ARGS} -debug -bench-name "$BENCH_NAME" -- -w $INCLUDES $DEFINES $CC1_ARGS 2>&1 | egrep -v '^Args:'
    t1=`date +%s`
    echo Elapsed $[$t1-$t0]
fi
