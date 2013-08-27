int main() {
    int u, v;
    do {
        int t = u % v;
        u = v;
        v = t;
    } while (v);
}
