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
    "$0" coreutils $2 || exit $?
    "$0" cBench $2 || exit $?
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

    sed -i '' -e 's/\(.*FLAGS.*loop-classify.*\)-fno-exceptions\(.*\)/\1\2/' build.ninja
    ninja || exit 1
    echo bin/loop-classify $FILES -loop-stats -debug -- -w $INCLUDES $DEFINES
    LOOP_CLASSIFY_ARGS="$LOOP_CLASSIFY_ARGS -loop-stats"
    LOOP_CLASSIFY_ARGS="$LOOP_CLASSIFY_ARGS -per-loop-stats"
    t0=`date +%s`
    bin/loop-classify $FILES ${LOOP_CLASSIFY_ARGS} -debug -- -w $INCLUDES $DEFINES 2>&1 >$BENCH.stats | egrep -v '^Args:'
    t1=`date +%s`
    echo Elapsed $[$t1-$t0] >> $BENCH.stats
    # mv loops.sql ${BENCH}.sql
    # echo | sqlite3 -init ${BENCH}.sql ${BENCH}.db
fi

cat_bench() {
    if [[ $1 == "*" ]] ; then
        cat ${BENCH}.stats | sed 's/ADA/IntegerIter/g'
    elif [[ $PREFIX == "" ]] ; then
        cat_bench '*' | egrep -v '@'
    else
        cat_bench '*' | egrep "^$PREFIX" | sed "s/^$PREFIX//g"
    fi
}
strip_trailing_comma() {
    sed 's/,*[[:space:]]*$//'
}
print_fields() {
    a='function symbcoord(s) { x = substr(s, 1, 1); i = (x=="?" || x=="!") ? 2 : 1; return tolower(substr(s, i, 2)) }'
    b='function stripr(s) { return substr(s, 0, length(s)-1) }' 
    awk "$a $b $1 END { $2 print \"\"; }"
}
join_lines() {
    tr '\n' ' '
}
escape_latex() {
    sed 's/_/\\_/g'
}
supergrep() {
    cat_bench | egrep $1 | print_fields "$2" "$3" | strip_trailing_comma | escape_latex
}
supergrep_class() {
    supergrep "^$1" '{ printf "%s/%s,", stripr($4), $1; SUM=SUM+(stripr($4)) }' 'printf "%.1f/REST", 100-SUM;'
}
supergrep_class_detail() {
    supergrep "^[\?!]?$1" '{ printf "%s/%s,", stripr($4), $1; }'
}

gen_latex() {
    export LC_ALL=en_US
    export PREFIX=""
    [[ $1 != "" ]] && export PREFIX="$1@"
    HEADER_PREFIX="ALL Statements"
    CLASSES="(IntegerIter|DataIter|ArrayIter|MacroPseudoBlock|UNCLASSIFIED)"
    STMT=$(supergrep '^(DO|FOR|WHILE)\t' '{ printf "%s/%s,", stripr($4), $1; }')
    CLASSES_LABELS=$(supergrep "^${CLASSES}" '{ printf "%s,", $1; }')
    CLASSES_X_COORDS=$(supergrep "^${CLASSES}" '{ printf "%s,", symbcoord($1); }')
    CLASSES_COORDS=$(supergrep "^${CLASSES}" '{ printf "(%s,%s) ", symbcoord($1), stripr($4); }')
    CLASSES_UNK_COORDS=$(supergrep "^${CLASSES}|\?${CLASSES}" '{ unk[symbcoord($1)] += stripr($4); }' 'for (i in unk) { printf "(%s,%s) ", i, unk[i]; i++ }')
    CLASSES_UPPER_COORDS=$(supergrep "^[\?!]${CLASSES}-NoLoopVarCandidate" '{ printf "(%s,%s) ", symbcoord($1), 100-stripr($4); }')
    EMPTYBODY=$(supergrep "^EMPTYBODY" '{ printf "%s/%s,", stripr($4), $1; SUM=SUM+(stripr($4)) }' 'printf "%.1f/REST", 100-SUM;')
    COND=$(supergrep "^Cond" '{ printf "%s/%s,", stripr($4), $1; }')
    BRANCH_DEPTH=$(supergrep "^Branch" '{ split($1, a, "-"); depth=a[3]; node_count=a[5]; count=$2; depths[depth]+=count; nodes[node_count]+=count; }' 'for (i in depths) { print "(", depths[i], ",", i, ")" }')
    BRANCH_NODES=$(supergrep "^Branch" '{ split($1, a, "-"); depth=a[3]; node_count=a[5]; count=$2; depths[depth]+=count; nodes[node_count]+=count; }' 'for (i in nodes) { print "(", nodes[i], ",", i, ") " }')
    cat <<EOF
        \section{${2}}
EOF
    [[ "$1" == "" ]] && cat <<EOF
        \subsection{${2} -- Loop Statements}
        \begin{tikzpicture}
        \pie[radius=6]{$STMT}
        \end{tikzpicture}
EOF
    cat <<EOF
        \subsection{${2} -- Loop Classes}
        \begin{tikzpicture}
        \begin{axis}[
        ybar,
        bar shift=0pt,
        ymin=0,
        symbolic x coords={${CLASSES_X_COORDS}},
        xtick=data,
        xticklabel style={text height=1.5ex}, % To make sure the text labels are nicely aligned
        xticklabels={${CLASSES_LABELS}},
        height=0.95\textheight,
        every node near coord/.style={xshift=24},
        nodes near coords={\pgfmathprintnumber{\pgfplotspointmeta}\%},
        nodes near coords align=center,
        ]
        \addplot[red!80!black, fill=red!30] coordinates { $CLASSES_UPPER_COORDS $( [[ $PREFIX == "DO@" ]] && echo "(ma,0)" ) $( [[ $PREFIX == "" ]] && echo "(ma,0) (un,0)" ) };
        \addplot[orange!80!black, fill=orange!30] coordinates { $CLASSES_UNK_COORDS };
        \addplot[green!80!black, fill=green!30] coordinates { $CLASSES_COORDS };
        \end{axis}
        \end{tikzpicture}

        \subsection{${2} -- Branching: Depth}
        \begin{tikzpicture}
        \begin{axis}[
        height=\textheight,
        xbar,
        width=20cm,
        ytick=data,
        xmin=0,
        nodes near coords, nodes near coords align={horizontal}
        ]
        \addplot+[xbar] coordinates
        { ${BRANCH_DEPTH} };
        \end{axis}
        \end{tikzpicture}

        \subsection{${2} -- Branching: Nodes}
        \begin{tikzpicture}
        \begin{axis}[
        height=\textheight,
        xbar,
        width=20cm,
        ytick=data,
        xmin=0,
        nodes near coords, nodes near coords align={horizontal}
        ]
        \addplot+[xbar] coordinates
        { ${BRANCH_NODES} };
        \end{axis}
        \end{tikzpicture}

        \subsection{${2} -- Empty Body}
        \begin{tikzpicture}
        \pie[radius=6]{$EMPTYBODY}
        \end{tikzpicture}

        \subsection{${2} -- Loops by Condition}
        \begin{tikzpicture}
        \pie[radius=6]{$COND}
        \end{tikzpicture}
EOF

    for class in "IntegerIter" "ArrayIter" "DataIter" ; do
        # \subsection{${2} -- $class Loops}
        # \begin{tikzpicture}
        # \pie[radius=6]{$(supergrep_class $class)}
        # \end{tikzpicture}

        cat <<EOF
        \subsection{${2} -- $class Loops (exclusion details)}
        \begin{tikzpicture}
        \pie[text=pin,radius=6]{$(supergrep_class_detail $class)}
        \end{tikzpicture}
EOF
    done
}

cat <<EOF >"${BENCH}.tex"
\documentclass{article}

\usepackage[a4paper,landscape]{geometry}
\usepackage{datetime}
\usepackage{titlesec}
\newcommand{\subsectionbreak}{\clearpage}
\usepackage{pgf-pie}
\usepackage{pgfplots}
\usetikzlibrary{shadows}

\title{$BENCH}

\begin{document}
\date{Compiled on \today\ at \currenttime}
\maketitle

$(gen_latex '' 'ALL (\texttt{for, while, do-while}) Statements')
$(gen_latex 'FOR' '\texttt{for} Statements')
$(gen_latex 'WHILE' '\texttt{while} Statements')
$(gen_latex 'DO' '\texttt{do} Statements')

\section{Plain}
\begin{verbatim}
$(cat_bench '*')
\end{verbatim}

\end{document}
EOF
rm ${BENCH}.pdf
pdflatex -interaction=nonstopmode "${BENCH}.tex" >/dev/null
mv ${BENCH}.pdf ${BENCH}_${TIMESTAMP}.pdf
ln -f ${BENCH}_${TIMESTAMP}.pdf ${BENCH}.pdf
open "${BENCH}.pdf" #convert -density 600x600 texput.pdf -quality 90 -resize 800x600 pic.png && open pic.png
