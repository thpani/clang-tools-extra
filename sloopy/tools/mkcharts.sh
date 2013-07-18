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
    echo "BENCH\t$BENCH">$BENCH.stats

    ninja sloopy || exit 1
    SLOOPY_ARGS="$SLOOPY_ARGS -loop-stats"
    t0=`date +%s`
    [[ -z $3 ]] && BENCH_NAME="${BENCH}" || BENCH_NAME="${BENCH}_${3}"
    bin/sloopy $FILES ${SLOOPY_ARGS} -debug -bench-name "$BENCH_NAME" -- -w $INCLUDES $DEFINES $CC1_ARGS 2>&1 >$BENCH.stats | egrep -v '^Args:'
    t1=`date +%s`
    echo Elapsed $[$t1-$t0]
    echo Elapsed $[$t1-$t0] >> $BENCH.stats
fi

cat_bench() {
    if [[ $1 == "*" ]] ; then
        cat ${BENCH}.stats
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
    CLASSES="(IntegerIter|DataIter|AArrayIter|PArrayIter|MacroPseudoBlock|UNCLASSIFIED)"
    STMT=$(supergrep '^(DO|FOR|WHILE|GOTO)\t' '{ printf "%s/%s,", stripr($4), $1; }')
    CLASSES_LABELS="IntegerIter,DataIter,AArrayIter,PArrayIter,MacroPseudoBlock,UNCLASSIFIED"
    CLASSES_X_COORDS="in,da,aa,pa,ma,un"
    CLASSES_PROG1='BEGIN { n=split("in da ar ma un", unkinv, " "); for (i in unkinv) unk[unkinv[i]] = 0; } { unk[symbcoord($1)] += stripr($4); }'
    CLASSES_PROG2='for (i = 1; i <= n; i++) { printf "(%s,%s) ", unkinv[i], unk[unkinv[i]]; }'
    CLASSES_COORDS=$(supergrep "^${CLASSES}" "$CLASSES_PROG1" "$CLASSES_PROG2")
    CLASSES_UNK_COORDS=$(supergrep "^${CLASSES}|\?${CLASSES}"  "$CLASSES_PROG1" "$CLASSES_PROG2")
    CLASSES_UPPER_COORDS=$(supergrep "^[\?!]${CLASSES}-NoLoopVarCandidate" "$CLASSES_PROG1" 'for (i = 1; i <= n; i++) { printf "(%s,%s) ", unkinv[i], 100-unk[unkinv[i]]; }')
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
        \addplot[red!80!black, fill=red!30] coordinates { $CLASSES_UPPER_COORDS };
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

# cat <<EOF >"${BENCH}.tex"
# \documentclass{article}

# \usepackage[a4paper,landscape]{geometry}
# \usepackage{datetime}
# \usepackage{titlesec}
# \newcommand{\subsectionbreak}{\clearpage}
# \usepackage{pgf-pie}
# \usepackage{pgfplots}
# \usetikzlibrary{shadows}

# \title{$BENCH}

# \begin{document}
# \date{Compiled on \today\ at \currenttime}
# \maketitle

# $(gen_latex '' 'ALL (\texttt{for, while, do-while}) Statements')
# $(gen_latex 'FOR' '\texttt{for} Statements')
# $(gen_latex 'WHILE' '\texttt{while} Statements')
# $(gen_latex 'DO' '\texttt{do} Statements')
# $(gen_latex 'GOTO' '\texttt{goto} Statements')

# \section{Plain}
# \begin{verbatim}
# $(cat_bench '*')
# \end{verbatim}

# \end{document}
# EOF
# rm ${BENCH}.pdf
# pdflatex -interaction=nonstopmode "${BENCH}.tex" >/dev/null
# mv ${BENCH}.pdf ${BENCH}_${TIMESTAMP}.pdf
# ln -f ${BENCH}_${TIMESTAMP}.pdf ${BENCH}.pdf
# open "${BENCH}.pdf" #convert -density 600x600 texput.pdf -quality 90 -resize 800x600 pic.png && open pic.png
