#include "tray.h"

#include <assert.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libappindicator/app-indicator.h>

static AppIndicator    *g_indicator = NULL;
static GtkWidget       *g_menu = NULL;
static GtkWidget       *g_status_item = NULL;
static rc_tray_quit_cb  g_quit_cb = NULL;

static void on_quit_activate(GtkMenuItem *item, gpointer data)
{
    (void)item;
    (void)data;
    if (g_quit_cb != NULL) {
        g_quit_cb();
    }
    gtk_main_quit();
}

static void on_reconnect_activate(GtkMenuItem *item, gpointer data)
{
    rc_tray_reconnect_cb reconnect_cb = (rc_tray_reconnect_cb)data;
    (void)item;
    if (reconnect_cb != NULL) {
        reconnect_cb();
    }
}

int rc_tray_init(rc_tray_quit_cb on_quit, rc_tray_reconnect_cb on_reconnect)
{
    assert(on_quit != NULL);

    g_quit_cb = on_quit;

    g_indicator = app_indicator_new(
        "remotecontrol-desktop",
        "network-transmit",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    if (g_indicator == NULL) {
        fprintf(stderr, "[tray] Failed to create AppIndicator\n");
        return -1;
    }

    app_indicator_set_status(g_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(g_indicator, "RemoteControl");

    /* Build menu */
    g_menu = gtk_menu_new();

    /* Status item (non-clickable) */
    g_status_item = gtk_menu_item_new_with_label("Status: Connecting...");
    gtk_widget_set_sensitive(g_status_item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_status_item);

    /* Separator */
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu),
                          gtk_separator_menu_item_new());

    /* Reconnect */
    GtkWidget *reconnect_item = gtk_menu_item_new_with_label("Reconnect");
    g_signal_connect(reconnect_item, "activate",
                     G_CALLBACK(on_reconnect_activate), (gpointer)on_reconnect);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), reconnect_item);

    /* Quit */
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), quit_item);

    gtk_widget_show_all(g_menu);
    app_indicator_set_menu(g_indicator, GTK_MENU(g_menu));

    fprintf(stdout, "[tray] System tray initialized\n");
    return 0;
}

void rc_tray_set_status(const char *status)
{
    if (g_status_item != NULL && status != NULL) {
        char label[128];
        snprintf(label, sizeof(label), "Status: %s", status);
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_status_item), label);
    }
}

void rc_tray_run(void)
{
    gtk_main();
}

void rc_tray_shutdown(void)
{
    if (g_indicator != NULL) {
        g_object_unref(g_indicator);
        g_indicator = NULL;
    }
}
