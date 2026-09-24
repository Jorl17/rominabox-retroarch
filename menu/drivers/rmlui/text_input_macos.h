#pragma once
#include <stdbool.h>
#ifdef __OBJC__
@class NSEvent;
#ifdef __cplusplus
extern "C" {
#endif
/* Call before raw gameplay keyboard dispatch. Composition is in AppKit. */
bool rib_cocoa_text_event(NSEvent *event);
#ifdef __cplusplus
}
#endif
#endif
