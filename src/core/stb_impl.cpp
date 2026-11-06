// The one translation unit that compiles the stb implementations.
// Everywhere else: just #include <stb_image.h> (no #define).
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
