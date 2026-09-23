/* The core-profile header from Apple. We would otherwise include glad in
 * this backend, but glad does not compile in a program that has already
 * included gl.h, and the GL2 backend must include it for the legacy context.
 * So the two backends are separate translation units, and here we link the
 * 3.3 entry points that are already in the OpenGL framework. */
#include <OpenGL/gl3.h>
