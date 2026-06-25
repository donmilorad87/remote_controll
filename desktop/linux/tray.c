#include "tray.h"

#include <assert.h>
#include <stdio.h>
#include <gtk/gtk.h>

static GtkStatusIcon    *g_status_icon = NULL;
static GtkWidget        *g_menu = NULL;
static GtkWidget        *g_status_item = NULL;
static rc_tray_quit_cb   g_quit_cb = NULL;

static void on_quit_activate(GtkMenuItem *item, gpointer data)
{
    (void)item; (void)data;
    if (g_quit_cb != NULL) g_quit_cb();
    gtk_main_quit();
}

static void on_reconnect_activate(GtkMenuItem *item, gpointer data)
{
    rc_tray_reconnect_cb cb = (rc_tray_reconnect_cb)data;
    (void)item;
    if (cb != NULL) cb();
}

static void on_popup_menu(GtkStatusIcon *icon, guint button, guint time, gpointer data)
{
    (void)icon; (void)data;
    gtk_menu_popup(GTK_MENU(g_menu), NULL, NULL, gtk_status_icon_position_menu,
                   g_status_icon, button, time);
}

int rc_tray_init(rc_tray_quit_cb on_quit, rc_tray_reconnect_cb on_reconnect)
{
    assert(on_quit != NULL);
    g_quit_cb = on_quit;

    g_status_icon = gtk_status_icon_new_from_icon_name("network-transmit");
    gtk_status_icon_set_tooltip_text(g_status_icon, "RemoteControl");
    gtk_status_icon_set_visible(g_status_icon, TRUE);

    g_menu = gtk_menu_new();

    g_status_item = gtk_menu_item_new_with_label("Status: Connected");
    gtk_widget_set_sensitive(g_status_item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_status_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());

    GtkWidget *reconnect = gtk_menu_item_new_with_label("New Pairing");
    g_signal_connect(reconnect, "activate", G_CALLBACK(on_reconnect_activate), (gpointer)on_reconnect);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), reconnect);

    GtkWidget *quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit, "activate", G_CALLBACK(on_quit_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), quit);

    gtk_widget_show_all(g_menu);

    g_signal_connect(g_status_icon, "popup-menu", G_CALLBACK(on_popup_menu), NULL);

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
    if (g_status_icon != NULL && status != NULL) {
        gtk_status_icon_set_tooltip_text(g_status_icon, status);
    }
}

void rc_tray_run(void) { gtk_main(); }

void rc_tray_shutdown(void)
{
    if (g_status_icon != NULL) {
        g_object_unref(g_status_icon);
        g_status_icon = NULL;
    }
}
