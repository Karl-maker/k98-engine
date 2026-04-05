// Single translation unit that compiles miniaudio (large); all other .cpp files include
// <miniaudio.h> without MINIAUDIO_IMPLEMENTATION.

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
