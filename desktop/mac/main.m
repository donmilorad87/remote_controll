#import <Cocoa/Cocoa.h>
#include "rc_desktop.h"
#include "rc_protocol.h"
#include "input_macos.h"
#include "volume_macos.h"
#include <assert.h>
#include <stdio.h>

static rc_desktop_t g_desktop;

void rc_handle_event(const rc_event_t *event) {
    assert(event != NULL);
    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        rc_input_move_cursor(event->data.cursor_move.direction, event->data.cursor_move.speed);
        break;
    case RC_EVT_TOUCHPAD:
        rc_input_move_cursor_relative(event->data.touchpad.dx, event->data.touchpad.dy);
        break;
    case RC_EVT_MOUSE_CLICK:
        rc_input_mouse_click(event->data.mouse_click.button);
        break;
    case RC_EVT_MOUSE_SCROLL:
        rc_input_mouse_scroll(event->data.mouse_scroll.direction);
        break;
    case RC_EVT_KEY_PRESS:
        rc_input_key_press(event->data.key.keycode, event->data.key.modifiers);
        break;
    case RC_EVT_KEY_RELEASE:
        rc_input_key_release(event->data.key.keycode, event->data.key.modifiers);
        break;
    case RC_EVT_VOLUME:
        rc_volume_handle(event->data.volume.action);
        break;
    default: break;
    }
}

@interface RCAppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSStatusItem *statusItem;
@end

@implementation RCAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    (void)n;
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    NSString *title = [NSString stringWithFormat:@"RC: %s", g_desktop.info.name];
    self.statusItem.button.title = title;

    NSMenu *menu = [[NSMenu alloc] init];
    NSString *info = [NSString stringWithFormat:@"%s (%s)", g_desktop.info.name, g_desktop.info.lan_ip];
    [menu addItemWithTitle:info action:nil keyEquivalent:@""].enabled = NO;
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Quit" action:@selector(onQuit:) keyEquivalent:@"q"].target = self;
    self.statusItem.menu = menu;
}
- (void)onQuit:(id)sender { (void)sender; rc_desktop_stop(&g_desktop); [NSApp terminate:nil]; }
@end

int main(int argc, const char *argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        rc_input_init();
        rc_volume_init();

        if (rc_desktop_start(&g_desktop) != 0) {
            NSAlert *a = [[NSAlert alloc] init];
            a.messageText = @"Failed to start network.";
            [a runModal];
            return 1;
        }

        fprintf(stdout, "Ready. Announcing as \"%s\" on %s\n",
                g_desktop.info.name, g_desktop.info.lan_ip);

        RCAppDelegate *delegate = [[RCAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];

        rc_desktop_stop(&g_desktop);
        rc_input_shutdown();
        rc_volume_shutdown();
    }
    return 0;
}
