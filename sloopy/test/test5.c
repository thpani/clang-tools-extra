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
        if(i) a();
        else b();
        i--;
    } while (i>0);

    int level,old_level;
    while (level-old_level) {
        if (level > old_level) {
            old_level -= 4;
        } else {
            old_level += 4;
        }
    }
}

