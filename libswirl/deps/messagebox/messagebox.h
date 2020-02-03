#ifndef MESSAGEBOX_X11_MESSAGEBOX_H
#define MESSAGEBOX_X11_MESSAGEBOX_H

#include <X11/Xlib.h>

typedef struct Button{
    const wchar_t *label;
    int result;
}Button;

int Messagebox(const char* title, const wchar_t* text, const Button* buttons, int numButtons);

#endif //MESSAGEBOX_X11_MESSAGEBOX_H
