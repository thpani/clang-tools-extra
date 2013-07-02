for f (**/Makefile.llvm) {
    cp $f $f.preproc
    pushd "$(dirname $f)" >/dev/null
    for cf (*.c) {
        TMPF=$(mktemp -t asdf)
        gsed -i '/-c/ { s!\(.*\)-c\(.*\)\*\.c!&\n\1-E\2 '"$cf"' | gindent 2>/dev/null >'"$TMPF"'\n\tmv '"$TMPF"' '"$cf"'! }' Makefile.llvm.preproc
    }
    gsed -i '/-c/d; /\*\.o/d;' Makefile.llvm.preproc
    bench=$(basename $(dirname $(dirname "$f")))
    echo "make $bench"
    if [[ $bench == "consumer_mad" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/ -I/Users/thomas/Documents/uni/da/llvm-build/bench/esound-0.2.8/ -I/Users/thomas/Documents/uni/da/llvm-build/bench/audiofile-0.3.6/libaudiofile/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "office_ghostscript" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "office_ispell" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "security_pgp_d" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "security_pgp_e" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "network_patricia" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    elif [[ $bench == "office_rsynth" ]] ; then
        CCC_OPTS="-I/Users/thomas/Documents/uni/da/llvm-build/bench/include/" make -s -f Makefile.llvm.preproc
    else
        make -s -f Makefile.llvm.preproc
    fi
    popd >/dev/null
}
