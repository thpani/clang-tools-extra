void f(int, int);
int main() {
    int j, p;
    int *ppt;
    for ( j = 0, p += 4; j < 4*6; j++, p++, ppt++ )
        f(p, *ppt);
}
