#define MAX_NFACTS 5
struct factors
{
  unsigned     plarge[2]; /* Can have a single large factor */
  unsigned     p[MAX_NFACTS];
  unsigned char e[MAX_NFACTS];
  unsigned char nfactors;
};

int main() {
  int n;
  for (n = 4; n != 0; n--);

  struct factors factors;
  for (unsigned int j = 0; j < factors.nfactors; j++)
    for (unsigned int k = 0; k < factors.e[j]; k++)
      gmp_printf (" %Zd", factors.p[j]);

  for (unsigned int j = 0; j < factors.nfactors; j++)
    for (unsigned int k = 0; k < factors.e[j]; k++)
      {
        char buf[42];
      }
}

static void
translate_buffer (char *buf, unsigned nread)
{
  char *cp;
  unsigned i;

  for (i = nread; i; i--, cp++)
    nread = 5;
}
void y() {
    int n;
  for (int i = 10; --i != 0; ) {
      n = 4;
  }
}
void x() {
    int written, len;
    for (written = 0; written < len;)
      {
        int to_write;

        if (to_write == 0)
          {
          }
        else
          {
            written += to_write;
          }
      }
}
