#import <Cocoa/Cocoa.h>
#include "rc_client.h"
#include "rc_protocol.h"
#include "input_macos.h"
#include "volume_macos.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Defined in client_net.c */
extern int rc_client_load_config(rc_client_config_t *cfg, const char *env_path);
extern int rc_client_login(rc_client_t *client, const char *email, const char *password);
extern int rc_client_register(rc_client_t *client, const char *email, const char *password);
extern int rc_client_connect(rc_client_t *client);
extern void rc_client_disconnect(rc_client_t *client);
extern int rc_client_logout(rc_client_t *client);

static rc_client_t g_client;

/* Called by recv thread */
void rc_handle_event(const rc_event_t *event) {
    assert(event != NULL);
    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        rc_input_move_cursor(event->data.cursor_move.direction,
                             event->data.cursor_move.speed);
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

/* ─── Login Window ───────────────────────────────────────────────── */

@interface RCLoginController : NSObject
@property (strong) NSWindow *window;
@property (strong) NSTextField *emailField;
@property (strong) NSSecureTextField *passwordField;
@property (strong) NSTextField *statusLabel;
@property int action;
@property (copy) NSString *email;
@property (copy) NSString *password;
@end

@implementation RCLoginController

- (void)showLoginWindow {
    NSRect frame = NSMakeRect(0, 0, 380, 240);
    self.window = [[NSWindow alloc]
        initWithContentRect:frame
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
        backing:NSBackingStoreBuffered
        defer:NO];
    [self.window setTitle:@"RemoteControl - Login"];
    [self.window center];

    NSView *content = [self.window contentView];

    /* Title */
    NSTextField *title = [NSTextField labelWithString:@"RemoteControl"];
    title.font = [NSFont boldSystemFontOfSize:18];
    title.textColor = [NSColor systemBlueColor];
    title.frame = NSMakeRect(20, 190, 340, 30);
    [content addSubview:title];

    /* Email */
    NSTextField *emailLabel = [NSTextField labelWithString:@"Email"];
    emailLabel.frame = NSMakeRect(20, 160, 340, 20);
    [content addSubview:emailLabel];

    self.emailField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 135, 340, 24)];
    self.emailField.placeholderString = @"user@example.com";
    [content addSubview:self.emailField];

    /* Password */
    NSTextField *passLabel = [NSTextField labelWithString:@"Password"];
    passLabel.frame = NSMakeRect(20, 108, 340, 20);
    [content addSubview:passLabel];

    self.passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(20, 83, 340, 24)];
    self.passwordField.placeholderString = @"Min 8 characters";
    [content addSubview:self.passwordField];

    /* Login button */
    NSButton *loginBtn = [[NSButton alloc] initWithFrame:NSMakeRect(20, 35, 160, 36)];
    loginBtn.title = @"Login";
    loginBtn.bezelStyle = NSBezelStyleRounded;
    loginBtn.target = self;
    loginBtn.action = @selector(onLogin:);
    [content addSubview:loginBtn];

    /* Register button */
    NSButton *registerBtn = [[NSButton alloc] initWithFrame:NSMakeRect(200, 35, 160, 36)];
    registerBtn.title = @"Register";
    registerBtn.bezelStyle = NSBezelStyleRounded;
    registerBtn.target = self;
    registerBtn.action = @selector(onRegister:);
    [content addSubview:registerBtn];

    /* Status */
    self.statusLabel = [NSTextField labelWithString:@""];
    self.statusLabel.frame = NSMakeRect(20, 8, 340, 20);
    self.statusLabel.textColor = [NSColor systemRedColor];
    [content addSubview:self.statusLabel];

    [self.window makeKeyAndOrderFront:nil];
    [NSApp runModalForWindow:self.window];
}

- (void)onLogin:(id)sender {
    (void)sender;
    self.email = self.emailField.stringValue;
    self.password = self.passwordField.stringValue;
    self.action = 1;
    [NSApp stopModal];
    [self.window close];
}

- (void)onRegister:(id)sender {
    (void)sender;
    self.email = self.emailField.stringValue;
    self.password = self.passwordField.stringValue;
    self.action = 2;
    [NSApp stopModal];
    [self.window close];
}

@end

/* ─── Status Bar (Menu Bar) ──────────────────────────────────────── */

@interface RCAppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSStatusItem *statusItem;
@end

@implementation RCAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;

    self.statusItem = [[NSStatusBar systemStatusBar]
        statusItemWithLength:NSVariableStatusItemLength];
    self.statusItem.button.title = @"RC";

    NSMenu *menu = [[NSMenu alloc] init];
    NSMenuItem *statusMenuItem = [[NSMenuItem alloc]
        initWithTitle:@"Status: Connected" action:nil keyEquivalent:@""];
    statusMenuItem.enabled = NO;
    [menu addItem:statusMenuItem];
    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *reconnect = [[NSMenuItem alloc]
        initWithTitle:@"Reconnect" action:@selector(onReconnect:) keyEquivalent:@"r"];
    reconnect.target = self;
    [menu addItem:reconnect];

    NSMenuItem *quit = [[NSMenuItem alloc]
        initWithTitle:@"Quit" action:@selector(onQuit:) keyEquivalent:@"q"];
    quit.target = self;
    [menu addItem:quit];

    self.statusItem.menu = menu;
}

- (void)onReconnect:(id)sender {
    (void)sender;
    if (g_client.connected) rc_client_disconnect(&g_client);
    if (g_client.auth.authenticated) rc_client_connect(&g_client);
}

- (void)onQuit:(id)sender {
    (void)sender;
    rc_client_disconnect(&g_client);
    rc_client_logout(&g_client);
    [NSApp terminate:nil];
}

@end

/* ─── main ───────────────────────────────────────────────────────── */

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        curl_global_init(CURL_GLOBAL_ALL);

        memset(&g_client, 0, sizeof(g_client));
        g_client.udp_fd = -1;

        rc_client_load_config(&g_client.config, ".env");
        rc_input_init();
        rc_volume_init();

        /* Login window */
        RCLoginController *login = [[RCLoginController alloc] init];
        [login showLoginWindow];

        if (login.action == 0) {
            rc_input_shutdown();
            rc_volume_shutdown();
            return 0;
        }

        const char *email = [login.email UTF8String];
        const char *password = [login.password UTF8String];

        if (login.action == 2) {
            rc_client_register(&g_client, email, password);
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = @"Check your email for activation link.";
            [alert runModal];
            return 0;
        }

        if (rc_client_login(&g_client, email, password) != 0) {
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = @"Login failed.";
            alert.alertStyle = NSAlertStyleCritical;
            [alert runModal];
            return 1;
        }

        if (rc_client_connect(&g_client) != 0) {
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = @"Connection failed.";
            alert.alertStyle = NSAlertStyleCritical;
            [alert runModal];
            rc_client_logout(&g_client);
            return 1;
        }

        /* Run as menu bar app */
        RCAppDelegate *delegate = [[RCAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];

        rc_input_shutdown();
        rc_volume_shutdown();
        curl_global_cleanup();
    }

    return 0;
}
