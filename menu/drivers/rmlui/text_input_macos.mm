/* AppKit composition for RmlUi, with no key table and no second editor. */
#import <AppKit/AppKit.h>
#include "text_input_platform.hpp"
#include "text_input_macos.h"
#include "document.hpp"
#include "../rmlui_bridge.h"
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/TextInputContext.h>
#include <algorithm>

namespace {
NSString *plain_text(id text)
{
   return [text isKindOfClass:[NSAttributedString class]] ? [text string] : text;
}
// AppKit ranges count UTF-16 units; RmlUi ranges count Unicode codepoints.
int character_offset(NSString *text, NSUInteger offset)
{
   offset = std::min(offset, [text length]);
   if (offset && offset < [text length] &&
       CFStringIsSurrogateHighCharacter([text characterAtIndex:offset - 1]) &&
       CFStringIsSurrogateLowCharacter([text characterAtIndex:offset])) --offset;
   return (int)([[text substringToIndex:offset] lengthOfBytesUsingEncoding:NSUTF32LittleEndianStringEncoding] / 4);
}
NSUInteger utf16_offset(NSString *text, int characters)
{
   const std::string utf8 = [text UTF8String] ?: "";
   const int bytes = Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(utf8, std::max(0, characters));
   NSString *prefix = [[NSString alloc] initWithBytes:utf8.data() length:bytes encoding:NSUTF8StringEncoding];
   const NSUInteger result = [prefix length];
   [prefix release];
   return result;
}
}

@interface RIBTextInputClient : NSObject<NSTextInputClient> {
@public
   rib::Document *document;
   Rml::TextInputContext *input;
   NSTextInputContext *ime;
   NSWindow *window;
   NSRange marked;
   Rml::Vector2f caret_position;
   float line_height;
   BOOL command_handled;
}
- (NSString *)value;
- (void)detach;
@end

@implementation RIBTextInputClient
- (id)init
{
   if ((self = [super init])) {
      marked = NSMakeRange(NSNotFound, 0);
      ime = [[NSTextInputContext alloc] initWithClient:self];
   }
   return self;
}
- (void)dealloc { [self detach]; [ime release]; [super dealloc]; }
- (NSString *)value
{
   auto *field = document && document->get_context() ?
         dynamic_cast<Rml::ElementFormControlInput*>(document->get_context()->GetFocusElement()) : nullptr;
   return field ? [NSString stringWithUTF8String:field->GetValue().c_str()] : @"";
}
- (void)detach
{
   // Destroy callbacks must not call back into the text widget being destroyed.
   input = nullptr;
   marked = NSMakeRange(NSNotFound, 0);
   [ime discardMarkedText];
   [ime deactivate];
   window = nil;
}
- (NSRange)selectedRange
{
   if (!input) return NSMakeRange(NSNotFound, 0);
   int start, end;
   input->GetSelectionRange(start, end);
   NSString *value = [self value];
   const NSUInteger first = utf16_offset(value, std::min(start, end));
   return NSMakeRange(first, utf16_offset(value, std::max(start, end)) - first);
}
- (NSRange)markedRange { return marked; }
- (BOOL)hasMarkedText { return marked.location != NSNotFound; }
- (NSArray *)validAttributesForMarkedText { return @[]; }
- (void)setMarkedText:(id)text selectedRange:(NSRange)selection replacementRange:(NSRange)replacement
{
   if (!input) return;
   NSString *value = [self value];
   NSString *string = plain_text(text);
   if (replacement.location == NSNotFound) replacement = [self hasMarkedText] ? marked : [self selectedRange];
   if (replacement.location == NSNotFound) return;
   const int start = character_offset(value, replacement.location);
   const int end = character_offset(value, NSMaxRange(replacement));
   input->SetText(std::string([string UTF8String] ?: ""), start, end);
   const int count = character_offset(string, [string length]);
   input->SetCompositionRange(start, start + count);
   input->SetSelectionRange(start + character_offset(string, selection.location),
         start + character_offset(string, NSMaxRange(selection)));
   marked = NSMakeRange(replacement.location, [string length]);
}
- (void)insertText:(id)text replacementRange:(NSRange)replacement
{
   if (!input) return;
   NSString *value = [self value];
   NSString *string = plain_text(text);
   if (replacement.location == NSNotFound) replacement = [self hasMarkedText] ? marked : [self selectedRange];
   if (replacement.location == NSNotFound) return;
   const int start = character_offset(value, replacement.location);
   const int end = character_offset(value, NSMaxRange(replacement));
   // Selection replacement and input restrictions for all commits are in RmlUi.
   input->SetCompositionRange(0, 0);
   input->SetSelectionRange(start, end);
   marked = NSMakeRange(NSNotFound, 0);
   document->get_context()->ProcessTextInput([string UTF8String] ?: "");
}
- (void)unmarkText
{
   if (!input || ![self hasMarkedText]) return;
   NSString *value = [self value];
   const NSRange range = NSIntersectionRange(marked, NSMakeRange(0, [value length]));
   [self insertText:[value substringWithRange:range] replacementRange:range];
}
- (void)doCommandBySelector:(SEL)selector
{
   if (!input) return;
   using namespace Rml::Input;
   if (selector == @selector(cancelOperation:) && [self hasMarkedText]) {
      [self insertText:@"" replacementRange:marked];
      [ime discardMarkedText];
      return;
   }
   // Translate OS editing intents into the editor's existing key operations.
   // Form navigation and cancellation go through the shared input boundary.
   const struct { SEL command; KeyIdentifier key; int modifiers; } commands[] = {
      {@selector(moveLeft:), KI_LEFT, 0}, {@selector(moveRight:), KI_RIGHT, 0},
      {@selector(moveWordLeft:), KI_LEFT, KM_CTRL}, {@selector(moveWordRight:), KI_RIGHT, KM_CTRL},
      {@selector(moveLeftAndModifySelection:), KI_LEFT, KM_SHIFT},
      {@selector(moveRightAndModifySelection:), KI_RIGHT, KM_SHIFT},
      {@selector(moveWordLeftAndModifySelection:), KI_LEFT, KM_CTRL | KM_SHIFT},
      {@selector(moveWordRightAndModifySelection:), KI_RIGHT, KM_CTRL | KM_SHIFT},
      {@selector(moveToBeginningOfLine:), KI_HOME, 0}, {@selector(moveToEndOfLine:), KI_END, 0},
      {@selector(moveToBeginningOfLineAndModifySelection:), KI_HOME, KM_SHIFT},
      {@selector(moveToEndOfLineAndModifySelection:), KI_END, KM_SHIFT},
      {@selector(deleteBackward:), KI_BACK, 0}, {@selector(deleteForward:), KI_DELETE, 0},
      {@selector(deleteWordBackward:), KI_BACK, KM_CTRL}, {@selector(deleteWordForward:), KI_DELETE, KM_CTRL},
      {@selector(selectAll:), KI_A, KM_CTRL}, {@selector(copy:), KI_C, KM_CTRL},
      {@selector(cut:), KI_X, KM_CTRL}, {@selector(paste:), KI_V, KM_CTRL}
   };
   for (const auto& command : commands) if (selector == command.command) {
      document->get_context()->ProcessKeyDown(command.key, command.modifiers);
      document->get_context()->ProcessKeyUp(command.key, command.modifiers);
      return;
   }
   command_handled = NO;
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual
{
   auto *field = document && document->get_context() ?
         dynamic_cast<Rml::ElementFormControlInput*>(document->get_context()->GetFocusElement()) : nullptr;
   // Do not supply surrounding password text to spelling/IME services.
   if (!field || field->GetAttribute<std::string>("type", "") == "password") return nil;
   NSString *value = [self value];
   if (range.location == NSNotFound || range.location > [value length]) return nil;
   range = NSIntersectionRange(range, NSMakeRange(0, [value length]));
   if (actual) *actual = range;
   return [[[NSAttributedString alloc] initWithString:[value substringWithRange:range]] autorelease];
}
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual
{
   if (actual) *actual = range;
   if (!window || !document || !document->get_context()) return NSZeroRect;
   NSView *view = [window contentView];
   const NSRect bounds = [view bounds];
   const auto dimensions = document->get_context()->GetDimensions();
   const CGFloat x = caret_position.x * NSWidth(bounds) / std::max(1, dimensions.x);
   const CGFloat height = line_height * NSHeight(bounds) / std::max(1, dimensions.y);
   CGFloat y = caret_position.y * NSHeight(bounds) / std::max(1, dimensions.y);
   if (![view isFlipped]) y = NSHeight(bounds) - y - height;
   return [window convertRectToScreen:[view convertRect:NSMakeRect(x, y, 1, height) toView:nil]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return NSNotFound; }
- (NSInteger)windowLevel { return window ? [window level] : NSNormalWindowLevel; }
@end

namespace {
RIBTextInputClient *active_client;
class CocoaTextInput final : public rib::TextInputPlatform
{
public:
   explicit CocoaTextInput(rib::Document& document)
   {
      client = [[RIBTextInputClient alloc] init];
      client->document = &document;
   }
   ~CocoaTextInput() override
   {
      if (active_client == client) active_client = nil;
      [client release];
   }
   void OnActivate(Rml::TextInputContext *input) override
   {
      if (active_client && active_client != client) [active_client detach];
      client->input = input;
      active_client = client;
      // The new focus element is known in RmlUi only after this callback. We
      // activate AppKit on its first event, when surrounding-text queries work.
   }
   void OnDeactivate(Rml::TextInputContext *input) override
   {
      if (client->input != input) return;
      [client unmarkText];
      [client detach];
      if (active_client == client) active_client = nil;
   }
   void OnDestroy(Rml::TextInputContext *input) override
   {
      if (client->input != input) return;
      [client detach];
      if (active_client == client) active_client = nil;
   }
   void get_clipboard(Rml::String& text) const override
   {
      NSString *value = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
      text = value ? [value UTF8String] : "";
   }
   void set_clipboard(const Rml::String& text) override
   {
      NSString *value = [NSString stringWithUTF8String:text.c_str()];
      if (!value) return;
      NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
      [pasteboard clearContents];
      [pasteboard setString:value forType:NSPasteboardTypeString];
   }
   void caret(Rml::Vector2f position, float height) override
   {
      client->caret_position = position;
      client->line_height = height;
      [client->ime invalidateCharacterCoordinates];
   }
private:
   RIBTextInputClient *client;
};
}
namespace rib {
std::unique_ptr<TextInputPlatform> make_text_input_platform(Document& document)
{
   return std::make_unique<CocoaTextInput>(document);
}
}
extern "C" bool rib_cocoa_text_event(NSEvent *event)
{
   if (!active_client || !active_client->input ||
       ([event type] != NSEventTypeKeyDown && [event type] != NSEventTypeKeyUp)) return false;
   if (!rib_rmlui_begin_native_text()) return false;
   // Let RetroArch observe releases even when AppKit consumed the down edge.
   if ([event type] == NSEventTypeKeyUp) return false;
   active_client->window = [event window];
   active_client->command_handled = YES;
   [active_client->ime activate];
   const BOOL handled = [active_client->ime handleEvent:event];
   return handled && active_client && active_client->command_handled;
}
