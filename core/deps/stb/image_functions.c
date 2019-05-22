
#include <stdint.h>
#include <stdlib.h>
#include "deps/zlib/zlib.h"
#include "image_functions.h"

// Use zlib to compress PNGs, better, faster and already available.
uint8_t * default_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality) {
	uLongf outsz = 1024 + data_len*1.1f;
	uint8_t *outbuf = (uint8_t*)malloc(outsz);
	if (Z_OK != compress2((Bytef*)outbuf, &outsz, (Bytef*)data, data_len, quality)) {
		free(outbuf);
		return NULL;
	}
	*out_len = outsz;
	return outbuf;
}

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBIW_ZLIB_COMPRESS default_zlib_compress
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

// Writes an RGBA png image to disk.
void writePngImageRGBA(char const *filename, int w, int h, const void *data) {
	stbi_write_png(filename, w, h, 4, data, w*4);
}

void *loadRGBAImageFromFile(char const *filename, int *w, int *h) {
	int n;
	return stbi_load(filename, w, h, &n, 4);
}

