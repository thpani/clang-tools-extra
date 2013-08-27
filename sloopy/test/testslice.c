int comp(void *, void *);
// bench/cBench15032013/automotive_qsort1/src/qsort.c
int main() {
    int p, lo, width, hi, max;
    for (p = lo+width; p <= hi; p += width)
        if (comp(p, max) > 0)
            max = p;
}
