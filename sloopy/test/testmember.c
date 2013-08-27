struct a {
    int i;
    int x;
};

int main() {
    struct a a;
    for (int i = 0; i < a.i; i++);
    for (int i = 0; i < a.i; i++) a.x = 4;
    for (int i = 0; i < a.i; i++) a.i = 4;
}
