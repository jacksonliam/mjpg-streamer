#define HUFF_EXTEND(x,s)  ((x) < (1<<((s)-1)) ? (x) + (((-1)<<(s)) + 1) : (x))

// haven't done restart markers, but these don't seem to appear
double getFrameSharpnessValue(unsigned char *data, int len);
