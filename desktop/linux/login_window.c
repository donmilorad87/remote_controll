#include "login_window.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

typedef struct {
    GtkWidget       *window;
    GtkWidget       *email_entry;
    GtkWidget       *password_entry;
    GtkWidget       *status_label;
    rc_login_result_t *result;
    int              done;
} login_ctx_t;

static void on_login_clicked(GtkWidget *button, gpointer data)
{
    (void)button;
    login_ctx_t *ctx = (login_ctx_t *)data;
    assert(ctx != NULL);

    const char *email = gtk_entry_get_text(GTK_ENTRY(ctx->email_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(ctx->password_entry));

    if (strlen(email) < 5 || strlen(password) < 8) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label),
                           "Email and password (min 8 chars) required");
        return;
    }

    strncpy(ctx->result->email, email, sizeof(ctx->result->email) - 1);
    strncpy(ctx->result->password, password, sizeof(ctx->result->password) - 1);
    ctx->result->action = 1; /* login */
    ctx->done = 1;
    gtk_main_quit();
}

static void on_register_clicked(GtkWidget *button, gpointer data)
{
    (void)button;
    login_ctx_t *ctx = (login_ctx_t *)data;
    assert(ctx != NULL);

    const char *email = gtk_entry_get_text(GTK_ENTRY(ctx->email_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(ctx->password_entry));

    if (strlen(email) < 5 || strlen(password) < 8) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label),
                           "Email and password (min 8 chars) required");
        return;
    }

    strncpy(ctx->result->email, email, sizeof(ctx->result->email) - 1);
    strncpy(ctx->result->password, password, sizeof(ctx->result->password) - 1);
    ctx->result->action = 2; /* register */
    ctx->done = 1;
    gtk_main_quit();
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    (void)widget;
    (void)event;
    login_ctx_t *ctx = (login_ctx_t *)data;
    ctx->result->action = 0;
    ctx->done = 1;
    gtk_main_quit();
    return TRUE;
}

int rc_login_window_show(rc_login_result_t *result)
{
    assert(result != NULL);
    memset(result, 0, sizeof(*result));

    int argc = 0;
    gtk_init(&argc, NULL);

    login_ctx_t ctx = { .result = result, .done = 0 };

    /* Window */
    ctx.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx.window), "RemoteControl - Login");
    gtk_window_set_default_size(GTK_WINDOW(ctx.window), 360, 280);
    gtk_window_set_position(GTK_WINDOW(ctx.window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(ctx.window), FALSE);
    g_signal_connect(ctx.window, "delete-event", G_CALLBACK(on_window_delete), &ctx);

    /* Main box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);
    gtk_container_add(GTK_CONTAINER(ctx.window), vbox);

    /* Title */
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='large' weight='bold' color='#3b82f6'>RemoteControl</span>");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    /* Email */
    GtkWidget *email_label = gtk_label_new("Email");
    gtk_widget_set_halign(email_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), email_label, FALSE, FALSE, 0);

    ctx.email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx.email_entry), "user@example.com");
    gtk_box_pack_start(GTK_BOX(vbox), ctx.email_entry, FALSE, FALSE, 0);

    /* Password */
    GtkWidget *pass_label = gtk_label_new("Password");
    gtk_widget_set_halign(pass_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), pass_label, FALSE, FALSE, 0);

    ctx.password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ctx.password_entry), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx.password_entry), "Min 8 characters");
    gtk_box_pack_start(GTK_BOX(vbox), ctx.password_entry, FALSE, FALSE, 0);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 4);

    GtkWidget *login_btn = gtk_button_new_with_label("Login");
    gtk_widget_set_size_request(login_btn, 150, 36);
    g_signal_connect(login_btn, "clicked", G_CALLBACK(on_login_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), login_btn, TRUE, TRUE, 0);

    GtkWidget *register_btn = gtk_button_new_with_label("Register");
    gtk_widget_set_size_request(register_btn, 150, 36);
    g_signal_connect(register_btn, "clicked", G_CALLBACK(on_register_clicked), &ctx);
    gtk_box_pack_start(GTK_BOX(btn_box), register_btn, TRUE, TRUE, 0);

    /* Status label */
    ctx.status_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), ctx.status_label, FALSE, FALSE, 0);

    gtk_widget_show_all(ctx.window);
    gtk_main();

    gtk_widget_destroy(ctx.window);

    /* Process remaining GTK events */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    return (result->action != 0) ? 0 : -1;
}
