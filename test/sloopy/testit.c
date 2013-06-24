typedef struct node {
    int val;
    struct node *next;
} node_t;


int main() {
    for (node_t *p=0; p; p = p->next);

    node_t *p, *n;
    while (p) {
        n = p->next;
        free(p);
        p = n;
    }
}
