
#ifdef __cplusplus
extern "C" {
#endif

void writePngImageRGBA(char const *filename, int w, int h, const void *data);
void *loadRGBAImageFromFile(char const *filename, int *w, int *h);

#ifdef __cplusplus
}
#endif

