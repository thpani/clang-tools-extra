void a();
void b();
int main() {
    int i = 0;
loop:
    {
        i++;
        if (i<42) goto loop;
    }
    for (int a=0; a<2; a++) {
    }
    while (i<100) {
        i++;
    }
    do {
        if(i) i++;
        else b();
        i--;
    } while (i>0);
}

