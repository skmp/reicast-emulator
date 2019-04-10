#ifndef UTILS_H_
#define UTILS_H_

void setAPK (const wchar_t* apkPath);
//Filename will be looked up in the apk (should start with assets/ or res/
GLuint loadTextureFromPNG (const wchar_t* filename, int &width, int &height);

#endif /* UTILS_H_ */
