BENCH=$1
BENCH_DIR="$(pwd)/bench"
TIMESTAMP=$(date "+%Y-%m-%d_%H:%M")

if [[ ${BENCH} == "setup" ]] ; then
    rm -rf ${BENCH_DIR}
    mkdir -p ${BENCH_DIR}

    cd ${BENCH_DIR}

    wget http://ftp.gnu.org/gnu/coreutils/coreutils-8.21.tar.xz
    tar xf coreutils-8.21.tar.xz
    cd coreutils-8.21
    ./configure
    cd ..
    echo -n "Please copy cBench15032013.tar.bz2 (from Moritz' email) to ${BENCH_DIR} [enter]"
    read
    tar xf cBench15032013.tar.bz2

    # dependencies

    wget http://www.tux.org/~ricdude/esound-0.2.8.tar.gz    # consumer_mad
    tar xf esound-0.2.8.tar.gz
    wget http://audiofile.68k.org/audiofile-0.3.6.tar.gz    # consumer_mad
    tar xf audiofile-0.3.6.tar.gz
    mkdir sys
    wget http://people.freebsd.org/~ariff/lowlatency/soundcard.h # consumer_mad
    mv soundcard.h sys

    # office_ghostscript
    cp /usr/include/stdio.h .
    echo -n "\"extern int dprintf (int __fd, __const char *__restrict __fmt, ...) ...\" auskommentieren"
    read
    $EDITOR stdio.h

    # office_ispell
    echo -n "\"ssize_t getline(char ** __restrict, size_t * __restrict, FILE * __restrict) ...\" auskommentieren"
    read
    $EDITOR stdio.h

    # network_patricia
    wget ftp://ftp.irisa.fr/pub/OpenBSD/src/sys/sys/endian.h

    # security_pgp_d
    cp /usr/include/sys/time.h sys
    echo -n "\"int gettimeofday(struct timeval * __restrict, void * __restrict) ...\" auskommentieren"
    read
    $EDITOR sys/time.h

    cd ..

    exit
fi

if [[ ${BENCH} == "both" ]] ; then
    "$0" coreutils $2
    "$0" cBench $2
    exit
fi

if [[ $2 == "run" ]] ; then
    if [[ ${BENCH} == "coreutils" ]] ; then
        FILES=$(find ${BENCH_DIR}/coreutils-8.21/src/ -not -name tac-pipe.c -and -name '*.c')
        INCLUDES=-I${BENCH_DIR}/coreutils-8.21/lib/ 
        DEFINES="-DHASH_ALGO_MD5"
    elif [[ ${BENCH} == "cBench" ]] ; then
        FILES=$(find ${BENCH_DIR}/cBench15032013/ -not -name memmove.c -and -name '*.c')
        INCLUDES=$(find ${BENCH_DIR}/cBench15032013/ -name '*.c' | xargs -L1 dirname | sort | uniq | sed 's/^/-I/' | tr '\n' ' ')
        # consumer_mad
        INCLUDES="$INCLUDES -I${BENCH_DIR}/esound-0.2.8 -I${BENCH_DIR}/audiofile-0.3.6/libaudiofile/"
        # consumer_mad & office_ghostscript
        INCLUDES="$INCLUDES -I${BENCH_DIR}"
        # automotive_{bitcount,qsort1}
        DEFINES="-DEXIT_FAILURE=1"
        # consumer_mad
        DEFINES="$DEFINES -DFPM_DEFAULT -DHAVE_CONFIG_H"
        # security_pgp_d
        DEFINES="$DEFINES -DUNIX"
    fi
    echo "BENCH\t$BENCH">$BENCH.stats

    ninja || exit 1
    echo bin/loop-classify $FILES -loop-stats -debug -- -w $INCLUDES $DEFINES
    bin/loop-classify $FILES -loop-stats -debug -- -w $INCLUDES $DEFINES 2>&1 >$BENCH.stats | egrep -v '^Args:'
fi

export LC_ALL=en_US
CLASSES=$(cat "${BENCH}.stats" | egrep '^(DO|FOR|WHILE)\t' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($3), $1; } END { print ""; }' | sed 's/,$//')
EMPTYBODY=$(cat "${BENCH}.stats" | egrep '^ANY-EMPTYBODY' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($3), $1; SUM=SUM+(stripl($3)) } END { printf "%.1f/REST\n", 100-SUM; }' | sed 's/,$//')
FOR_COND=$(cat "${BENCH}.stats" | egrep '^FOR-Cond' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; SUM=SUM+(stripl($4)) } END { printf "%.1f/REST\n", 100-SUM; }' | sed 's/,$//' | sed 's/_/\\_/g')
FOR_ADA=$(cat "${BENCH}.stats" | egrep '^FOR-ADA' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; SUM=SUM+(stripl($4)) } END { printf "%.1f/REST\n", 100-SUM; }' | sed 's/,$//' | sed 's/_/\\_/g')
FOR_ADA_DETAIL=$(cat "${BENCH}.stats" | egrep '^FOR-!?ADA' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; }' | sed 's/,$//' | sed 's/_/\\_/g')
WHILE_COND=$(cat "${BENCH}.stats" | egrep '^WHILE-Cond' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; SUM=SUM+(stripl($4)) } END { printf "%.1f/REST\n", 100-SUM; }' | sed 's/,$//' | sed 's/_/\\_/g')
WHILE_ADA=$(cat "${BENCH}.stats" | egrep '^WHILE-ADA' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; SUM=SUM+(stripl($4)) } END { printf "%.1f/REST\n", 100-SUM; }' | sed 's/,$//' | sed 's/_/\\_/g')
WHILE_ADA_DETAIL=$(cat "${BENCH}.stats" | egrep '^WHILE-!?ADA' | awk 'function stripl(s) { return substr(s, 0, length(s)-1) } { printf "%s/%s,", stripl($4), $1; }' | sed 's/,$//' | sed 's/_/\\_/g')

cat <<EOF >"${BENCH}.tex"
\documentclass{article}

\usepackage[a4paper,landscape]{geometry}
\usepackage{pgf-pie}
\usetikzlibrary{shadows}

\title{$BENCH}

\begin{document}
\maketitle

\section{Plain}
\begin{verbatim}
$(cat "${BENCH}.stats")
\end{verbatim}

\section{Loop Statements}
\begin{tikzpicture}
\pie[radius=6]{$CLASSES}
\end{tikzpicture}

\section{Empty Body}
\begin{tikzpicture}
\pie[radius=6]{$EMPTYBODY}
\end{tikzpicture}

\section{FOR Loops by Condition}
\begin{tikzpicture}
\pie[radius=6]{$FOR_COND}
\end{tikzpicture}

\section{Ada-style FOR Loops}
\begin{tikzpicture}
\pie[radius=6]{$FOR_ADA}
\end{tikzpicture}

\section{Ada-style FOR Loops (exclusion details)}
\begin{tikzpicture}
\pie[text=pin,radius=6]{$FOR_ADA_DETAIL}
\end{tikzpicture}

\section{WHILE Loops by Condition}
\begin{tikzpicture}
\pie[radius=6]{$WHILE_COND}
\end{tikzpicture}

\section{Ada-style WHILE Loops}
\begin{tikzpicture}
\pie[radius=6]{$WHILE_ADA}
\end{tikzpicture}

\section{Ada-style WHILE Loops (exclusion details)}
\begin{tikzpicture}
\pie[text=pin,radius=6]{$WHILE_ADA_DETAIL}
\end{tikzpicture}

\end{document}
EOF
rm ${BENCH}.pdf
pdflatex -interaction=nonstopmode "${BENCH}.tex" >/dev/null
mv ${BENCH}.pdf ${BENCH}_${TIMESTAMP}.pdf
ln -sf ${BENCH}_${TIMESTAMP}.pdf ${BENCH}.pdf
open "${BENCH}.pdf" #convert -density 600x600 texput.pdf -quality 90 -resize 800x600 pic.png && open pic.png
