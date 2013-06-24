typedef char byte;
typedef unsigned int uint;
int main() {
    for (int i=0; (i -= 42) > 8;) ;

    int i, indices;
    byte *dest;
    byte *colors;
    for ( ; (i -= 2) >= 0; indices >>= 8 );
				  *--dest =
				    (byte)colors[(uint)indices & 0xf] +
				    ((byte)colors[((uint)indices >> 4) & 0xf]
				     << 4);
}
