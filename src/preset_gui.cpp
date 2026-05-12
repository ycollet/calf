/* Calf DSP Library
 * GUI functions for preset management.
 * Copyright (C) 2007 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <calf/gui.h>
#include <calf/preset.h>
#include <calf/preset_gui.h>

using namespace calf_plugins;
using namespace std;

struct activate_preset_params
{
    plugin_gui *gui;
    int preset;
    bool builtin;

    activate_preset_params(plugin_gui *_gui, int _preset, bool _builtin)
    : gui(_gui), preset(_preset), builtin(_builtin)
    {
    }
};

/******************************* dialog helpers **************************************************/

struct dlg_ctx {
    GMainLoop *loop;
    int result;
    bool destroyed;
};

static void dlg_btn_cb(GtkButton *btn, gpointer data)
{
    dlg_ctx *c = (dlg_ctx *)data;
    c->result = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "resp"));
    g_main_loop_quit(c->loop);
}

static void dlg_destroy_cb(GtkWidget *, gpointer data)
{
    dlg_ctx *c = (dlg_ctx *)data;
    c->destroyed = true;
    if (g_main_loop_is_running(c->loop))
        g_main_loop_quit(c->loop);
}

// Runs a GtkWindow synchronously as a modal dialog with OK/Cancel buttons.
// body is packed into a vertical box above the button row.
// Returns GTK_RESPONSE_OK or GTK_RESPONSE_CANCEL.
static int run_modal_dialog(GtkWindow *parent, const char *title, GtkWidget *body)
{
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(win), vbox);
    gtk_box_append(GTK_BOX(vbox), body);

    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btnbox, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btnbox, 4);
    gtk_box_append(GTK_BOX(vbox), btnbox);

    dlg_ctx ctx = { g_main_loop_new(NULL, FALSE), GTK_RESPONSE_CANCEL, false };

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_object_set_data(G_OBJECT(cancel_btn), "resp", GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(dlg_btn_cb), &ctx);
    gtk_box_append(GTK_BOX(btnbox), cancel_btn);

    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    g_object_set_data(G_OBJECT(ok_btn), "resp", GINT_TO_POINTER(GTK_RESPONSE_OK));
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(dlg_btn_cb), &ctx);
    gtk_box_append(GTK_BOX(btnbox), ok_btn);

    g_signal_connect(win, "destroy", G_CALLBACK(dlg_destroy_cb), &ctx);

    gtk_widget_set_visible(win, TRUE);
    g_main_loop_run(ctx.loop);
    if (!ctx.destroyed)
        gtk_window_destroy(GTK_WINDOW(win));
    g_main_loop_unref(ctx.loop);
    return ctx.result;
}

/******************************* store-preset OK helper **************************************************/

struct store_ok_data {
    dlg_ctx   *ctx;
    GtkWidget *entry;
    string    *name;
};

static void on_store_ok_clicked(GtkButton *, gpointer data)
{
    store_ok_data *d = (store_ok_data *)data;
    *d->name = gtk_editable_get_text(GTK_EDITABLE(d->entry));
    d->ctx->result = GTK_RESPONSE_OK;
    g_main_loop_quit(d->ctx->loop);
}

/******************************* gui_preset_access **************************************************/

gui_preset_access::gui_preset_access(plugin_gui *_gui)
{
    gui = _gui;
    store_preset_dlg = NULL;
}

void gui_preset_access::store_preset()
{
    if (store_preset_dlg)
    {
        gtk_window_present(GTK_WINDOW(store_preset_dlg));
        return;
    }

    // Build the dialog programmatically — the old calf-gui.xml uses GTK2-only
    // GtkComboBoxEntry which cannot be loaded by a GTK4 builder.
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Store preset");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    if (gui->window->toplevel)
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(gui->window->toplevel));

    store_preset_dlg = win;
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_dlg_destroy_window), this);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(hbox, 4);
    gtk_box_append(GTK_BOX(vbox), hbox);

    GtkWidget *name_label = gtk_label_new("Preset name:");
    gtk_box_append(GTK_BOX(hbox), name_label);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(hbox), entry);

    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btnbox, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(vbox), btnbox);

    dlg_ctx ctx = { g_main_loop_new(NULL, FALSE), GTK_RESPONSE_CANCEL, false };
    string preset_name;
    store_ok_data ok_d = { &ctx, entry, &preset_name };

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_object_set_data(G_OBJECT(cancel_btn), "resp", GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(dlg_btn_cb), &ctx);
    gtk_box_append(GTK_BOX(btnbox), cancel_btn);

    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_store_ok_clicked), &ok_d);
    gtk_box_append(GTK_BOX(btnbox), ok_btn);

    g_signal_connect(win, "destroy", G_CALLBACK(dlg_destroy_cb), &ctx);

    gtk_widget_set_visible(win, TRUE);
    g_main_loop_run(ctx.loop);
    if (!ctx.destroyed)
        gtk_window_destroy(GTK_WINDOW(win));
    g_main_loop_unref(ctx.loop);

    if (ctx.result != GTK_RESPONSE_OK)
        return;

    plugin_preset sp;
    sp.name = preset_name;
    sp.bank = 0;
    sp.program = 0;
    sp.plugin = gui->effect_name;
    sp.get_from(gui->plugin);

    preset_list tmp;
    try {
        tmp.load(tmp.get_preset_filename(false).c_str(), false);
    }
    catch(...)
    {
        tmp = get_user_presets();
    }

    bool found = false;
    for (preset_vector::const_iterator i = tmp.presets.begin(); i != tmp.presets.end(); ++i)
    {
        if (i->plugin == gui->effect_name && i->name == sp.name)
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "Preset '%s' already exists. Overwrite?", sp.name.c_str());
        GtkWidget *label = gtk_label_new(msg);
        GtkWindow *parent = gui->window->toplevel ? GTK_WINDOW(gui->window->toplevel) : NULL;
        int resp = run_modal_dialog(parent, "Overwrite preset?", label);
        if (resp != GTK_RESPONSE_OK)
            return;
    }

    tmp.add(sp);
    get_user_presets() = tmp;
    get_user_presets().save(tmp.get_preset_filename(false).c_str());
    if (gui->window->get_main_window())
        gui->window->get_main_window()->refresh_all_presets(false);
}

void gui_preset_access::activate_preset(int preset, bool builtin)
{
    plugin_preset &p = (builtin ? get_builtin_presets() : get_user_presets()).presets[preset];
    if (p.plugin != gui->effect_name)
        return;
    if (!gui->plugin->activate_preset(p.bank, p.program))
        p.activate(gui->plugin);
    gui->refresh();
}

void gui_preset_access::on_dlg_destroy_window(GtkWindow *window, gpointer data)
{
    ((gui_preset_access *)data)->store_preset_dlg = NULL;
}
