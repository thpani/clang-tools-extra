struct stats {};
char audio_mulaw_round(int, struct stats*);
int main() {
    int *left, *right;
    struct stats *stats;

    // cBench/consumer_mad/src/audio.c
    unsigned int len;
    unsigned char *data;
    while (len--) {
        data[0] = audio_mulaw_round(*left++,  stats);
        data[1] = audio_mulaw_round(*right++, stats);

        data += 2;
    }
}
