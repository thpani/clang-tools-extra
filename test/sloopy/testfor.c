int main() {
    for (int i=0;i<32;i++) {
        a();
        if (i) continue;
        b();
    }
}
