/* Calf DSP Library
 * Toplevel window that hosts the plugin GUI.
 * Copyright (C) 2007-2011 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <calf/gui_config.h>
#include <calf/gui_controls.h>
#include <calf/preset.h>
#include <calf/preset_gui.h>
#include <gdk/gdk.h>

#include <iostream>
#include <cstring>

using namespace calf_plugins;
using namespace std;

// ---------------------------------------------------------------------------
// Debug layout helper — prints widget tree with min/natural sizes and colors.
// Activated via calfjackhost --debug-layout.
// ---------------------------------------------------------------------------
static void debug_print_widget_tree(GtkWidget *widget, int depth)
{
    if (!widget || depth > 8) return;

    // Indentation
    char indent[80];
    int n = depth * 2 < 78 ? depth * 2 : 78;
    memset(indent, ' ', n);
    indent[n] = '\0';

    // Min / natural sizes
    int min_w = 0, nat_w = 0, min_h = 0, nat_h = 0;
    gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, &min_w, &nat_w, NULL, NULL);
    gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL,   -1, &min_h, &nat_h, NULL, NULL);

    // Computed CSS color
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
    GdkRGBA color = {0.0, 0.0, 0.0, 1.0};
    gtk_style_context_get_color(ctx, &color);
    G_GNUC_END_IGNORE_DEPRECATIONS

    const char *type  = G_OBJECT_TYPE_NAME(widget);
    const char *wname = gtk_widget_get_name(widget);

    g_print("[debug-layout] %s%s \"%s\"  min=%dx%d nat=%dx%d  color=#%02x%02x%02x\n",
            indent, type, wname ? wname : "",
            min_w, min_h, nat_w, nat_h,
            (int)(color.red   * 255 + 0.5),
            (int)(color.green * 255 + 0.5),
            (int)(color.blue  * 255 + 0.5));

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child; child = gtk_widget_get_next_sibling(child))
        debug_print_widget_tree(child, depth + 1);
}

/***************************** GUI widget ********************************************/

plugin_gui_widget::plugin_gui_widget(gui_environment_iface *_env, main_window_iface *_main)
{
    gui = NULL;
    toplevel = NULL;
    environment = _env;
    main = _main;
    assert(environment);
    prefix = "strips";
}

void plugin_gui_widget::on_window_destroyed(GtkWidget *window, gpointer data)
{
    plugin_gui_widget *self = (plugin_gui_widget *)data;
    self->gui->destroy_child_widgets(self->toplevel);
    delete self;
}

gboolean plugin_gui_widget::on_idle(void *data)
{
    plugin_gui_widget *self = (plugin_gui_widget *)data;
    //if (!self->refresh_controller.check_redraw(GTK_WIDGET(self->toplevel)))
        //return TRUE;
    self->gui->on_idle();
    return TRUE;
}

void plugin_gui_widget::create_gui(plugin_ctl_iface *_jh)
{
    gui = new plugin_gui(this);
    const char *xml = _jh->get_metadata_iface()->get_gui_xml(prefix.c_str());
    if (!xml) {
        xml = "<hbox />";
    }
    container = gui->create_from_xml(_jh, xml);
    source_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 1000/30, on_idle, this, NULL); // 30 fps should be enough for everybody
    gui->plugin->send_configures(gui);
}

GtkWidget *plugin_gui_widget::create(plugin_ctl_iface *_jh)
{
    create_gui(_jh);
    gtk_widget_set_name(container, "Calf-Plugin-Strip");
    gtk_widget_add_css_class(container, "Calf-Plugin-Strip");
    gtk_widget_set_visible(container, TRUE);
    toplevel = container;
    g_signal_connect (GTK_WIDGET(toplevel), "destroy", G_CALLBACK (on_window_destroyed), (plugin_gui_widget *)this);
    return container;
}

void plugin_gui_widget::refresh()
{
    if (gui)
        gui->refresh();
}

void plugin_gui_widget::cleanup()
{
    if (source_id)
        g_source_remove(source_id);
    source_id = 0;
}

plugin_gui_widget::~plugin_gui_widget()
{
    cleanup();
    delete gui;
    gui = NULL;
}

/******************************* dialog helper **************************************************/

static void dialog_response_cb(GtkDialog *dialog, gint response, gpointer data)
{
    int *result = (int *)data;
    *result = response;
    g_main_loop_quit((GMainLoop *)g_object_get_data(G_OBJECT(dialog), "run-loop"));
}

static int run_dialog_sync(GtkDialog *dialog)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    int result = GTK_RESPONSE_DELETE_EVENT;
    g_object_set_data(G_OBJECT(dialog), "run-loop", loop);
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response_cb), &result);
    gtk_widget_set_visible(GTK_WIDGET(dialog), TRUE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return result;
}

/******************************* Actions **************************************************/

void plugin_gui_window::store_preset_action(GSimpleAction *action, GVariant *parameter, gpointer gui_win)
{
    plugin_gui_window *self = (plugin_gui_window *)gui_win;
    if (self->gui->preset_access)
        self->gui->preset_access->store_preset();
}

namespace {

struct activate_preset_params
{
    preset_access_iface *preset_access;
    int preset;
    bool builtin;

    activate_preset_params(preset_access_iface *_pai, int _preset, bool _builtin)
    : preset_access(_pai), preset(_preset), builtin(_builtin)
    {
    }
    static void action_destroy_notify(gpointer data)
    {
        delete (activate_preset_params *)data;
    }
};

void activate_preset(GSimpleAction *action, GVariant *parameter, gpointer data)
{
    activate_preset_params *params = (activate_preset_params *)data;
    params->preset_access->activate_preset(params->preset, params->builtin);
}

}

void plugin_gui_window::help_action(GSimpleAction *action, GVariant *parameter, gpointer gui_win)
{
    plugin_gui_window *self = (plugin_gui_window *)gui_win;
    string uri = "file://" PKGDOCDIR "/" + string(self->gui->plugin->get_metadata_iface()->get_label()) + ".html";
    GtkUriLauncher *launcher = gtk_uri_launcher_new(uri.c_str());
    gtk_uri_launcher_launch(launcher, GTK_WINDOW(self->toplevel), NULL, NULL, NULL);
    g_object_unref(launcher);
}

void plugin_gui_window::about_action(GSimpleAction *action, GVariant *parameter, gpointer gui_win)
{
    plugin_gui_window *self = (plugin_gui_window *)gui_win;
    GtkAboutDialog *dlg = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
    if (!dlg)
        return;

    static const char *artists[] = {
        "Markus Schmidt (GUI, icons)",
        "Thorsten Wilms (previous icon)",
        NULL
    };

    static const char *authors[] = {
        "Krzysztof Foltman <wdev@foltman.com>",
        "Hermann Meyer <brummer-@web.de>",
        "Thor Harald Johansen <thj@thj.no>",
        "Thorsten Wilms <t_w_@freenet.de>",
        "Hans Baier <hansfbaier@googlemail.com>",
        "Torben Hohn <torbenh@gmx.de>",
        "Markus Schmidt <schmidt@boomshop.net>",
        "Christian Holschuh <chrisch.holli@gmx.de>",
        "Tom Szilagyi <tomszilagyi@gmail.com>",
        "Damien Zammit <damien@zamaudio.com>",
        "David T\xC3\xA4ht <d@teklibre.com>",
        "Dave Robillard <dave@drobilla.net>",
        NULL
    };

    static const char translators[] =
        "Russian: Alexandre Prokoudine <alexandre.prokoudine@gmail.com>\n"
    ;

    string label = self->gui->plugin->get_metadata_iface()->get_label();
    gtk_about_dialog_set_program_name(dlg, ("Calf " + label).c_str());
    gtk_about_dialog_set_version(dlg, PACKAGE_VERSION);
    gtk_about_dialog_set_website(dlg, "http://calf.sourceforge.net/");
    gtk_about_dialog_set_copyright(dlg, "Copyright \xC2\xA9 2001-2011 Krzysztof Foltman, Thor Harald Johansen, Markus Schmidt and others.\nSee AUTHORS file for the full list.");
    gtk_about_dialog_set_logo_icon_name(dlg, "calf");
    gtk_about_dialog_set_artists(dlg, artists);
    gtk_about_dialog_set_authors(dlg, authors);
    gtk_about_dialog_set_translator_credits(dlg, translators);
    run_dialog_sync(GTK_DIALOG(dlg));
    gtk_window_destroy(GTK_WINDOW(dlg));
}

struct info_dlg_ctx { GMainLoop *loop; bool closed; };
static void info_dlg_btn_cb(GtkButton *, gpointer data) {
    info_dlg_ctx *c = (info_dlg_ctx *)data;
    g_main_loop_quit(c->loop);
}
static void info_dlg_destroy_cb(GtkWidget *, gpointer data) {
    info_dlg_ctx *c = (info_dlg_ctx *)data;
    c->closed = true;
    if (g_main_loop_is_running(c->loop))
        g_main_loop_quit(c->loop);
}
static void show_info_dialog(GtkWindow *parent, const char *title, const char *text)
{
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12); gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);  gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(win), vbox);
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
    gtk_box_append(GTK_BOX(vbox), label);
    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btnbox, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btnbox, 4);
    gtk_box_append(GTK_BOX(vbox), btnbox);
    info_dlg_ctx ctx = { g_main_loop_new(NULL, FALSE), false };
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(info_dlg_btn_cb), &ctx);
    gtk_box_append(GTK_BOX(btnbox), ok_btn);
    g_signal_connect(win, "destroy", G_CALLBACK(info_dlg_destroy_cb), &ctx);
    gtk_widget_set_visible(win, TRUE);
    g_main_loop_run(ctx.loop);
    if (!ctx.closed)
        gtk_window_destroy(GTK_WINDOW(win));
    g_main_loop_unref(ctx.loop);
}

static void tips_tricks_action(GSimpleAction *action, GVariant *parameter, gpointer gui_win)
{
    plugin_gui_window *self = (plugin_gui_window *)gui_win;
    static const char tips_and_tricks[] =
    "1. Knob and Fader Control\n"
    "\n"
    "* Use SHIFT-dragging for increased precision\n"
    "* Mouse wheel is also supported\n"
    "* Middle click opens a text entry\n"
    "* Right click a knob to assign a MIDI controller\n"
    "\n"
    "2. Rack Ears\n"
    "\n"
    "If you consider those a waste of screen space, you can turn them off in Preferences dialog in Calf JACK host. The setting affects all versions of the GUI (LV2 GTK+, LV2 External, JACK host).\n"
    "\n"
    ;
    show_info_dialog(GTK_WINDOW(self->toplevel), "Tips and Tricks", tips_and_tricks);
}

void calf_plugins::activate_command(GSimpleAction *action, GVariant *parameter, gpointer data)
{
    activate_command_params *params = (activate_command_params *)data;
    plugin_gui *gui = params->gui;
    gui->plugin->execute(params->function_idx);
    gui->refresh();
}

/***************************** GUI window ********************************************/

plugin_gui_window::plugin_gui_window(gui_environment_iface *_env, main_window_iface *_main)
: plugin_gui_widget(_env, _main)
{
    menu_model = NULL;
    win_actions = NULL;
    notifier = NULL;
}

void plugin_gui_window::fill_gui_presets(bool builtin, char &ch)
{
    preset_access_iface *pai = gui->preset_access;
    preset_vector &pvec = (builtin ? get_builtin_presets() : get_user_presets()).presets;

    // Append to the Preset submenu (index 0 in menu_model)
    GMenuModel *preset_menu_model = g_menu_model_get_item_link(G_MENU_MODEL(menu_model), 0, G_MENU_LINK_SUBMENU);
    if (!preset_menu_model)
        return;
    GMenu *preset_menu = G_MENU(preset_menu_model);

    for (unsigned int i = 0; i < pvec.size(); i++)
    {
        if (pvec[i].plugin != gui->effect_name)
            continue;

        if (ch != ' ' && ++ch == ':')
            ch = 'A';
        if (ch > 'Z')
            ch = ' ';

        string prefix = ch == ' ' ? string() : string("_") + ch + " ";
        string name = prefix + pvec[i].name;
        string action_name = string(builtin ? "builtin_preset" : "user_preset") + to_string(i);
        string full_action = "win." + action_name;

        GMenuItem *menu_item = g_menu_item_new(name.c_str(), full_action.c_str());
        g_menu_append_item(preset_menu, menu_item);
        g_object_unref(menu_item);

        GSimpleAction *act = g_simple_action_new(action_name.c_str(), NULL);
        activate_preset_params *params = new activate_preset_params(pai, i, builtin);
        g_signal_connect_data(act, "activate", G_CALLBACK(activate_preset),
                              params, (GClosureNotify)activate_preset_params::action_destroy_notify, (GConnectFlags)0);
        g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act));
        g_object_unref(act);
    }
    g_object_unref(preset_menu_model);
}

void plugin_gui_window::create(plugin_ctl_iface *_jh, const char *title, const char *effect)
{
    prefix = "gui";
    GtkWidget *win = gtk_window_new();
    gtk_window_set_icon_name(GTK_WINDOW(win), "calf_plugin");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkRequisition req, req2;
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_child(GTK_WINDOW(win), GTK_WIDGET(vbox));

    create_gui(_jh);

    gui->effect_name = effect;
    gtk_widget_set_name(GTK_WIDGET(vbox), "Calf-Plugin");
    gtk_widget_add_css_class(GTK_WIDGET(vbox), "Calf-Plugin");
    GtkWidget *decoTable = decorate(container);

    // Replace GtkEventBox with a plain GtkBox
    GtkWidget *eventbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(GTK_WIDGET(eventbox), "Calf-Plugin");
    gtk_widget_add_css_class(GTK_WIDGET(eventbox), "Calf-Plugin");
    gtk_box_append(GTK_BOX(eventbox), decoTable);

    gtk_widget_set_visible(eventbox, TRUE);

    // Build menu with GMenu + GSimpleActionGroup
    win_actions = g_simple_action_group_new();

    // Store-preset action
    GSimpleAction *act_store = g_simple_action_new("store-preset", NULL);
    g_signal_connect(act_store, "activate", G_CALLBACK(store_preset_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_store));
    g_object_unref(act_store);

    // About action
    GSimpleAction *act_about = g_simple_action_new("about", NULL);
    g_signal_connect(act_about, "activate", G_CALLBACK(about_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_about));
    g_object_unref(act_about);

    // Help action
    GSimpleAction *act_help = g_simple_action_new("help", NULL);
    g_signal_connect(act_help, "activate", G_CALLBACK(help_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_help));
    g_object_unref(act_help);

    // Tips & Tricks action
    GSimpleAction *act_tips = g_simple_action_new("tips-tricks", NULL);
    g_signal_connect(act_tips, "activate", G_CALLBACK(tips_tricks_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_tips));
    g_object_unref(act_tips);

    // Add command actions
    plugin_command_info *ci = _jh->get_metadata_iface()->get_commands();
    if (ci)
    {
        for (int i = 0; ci->name; i++, ci++)
        {
            GSimpleAction *act_cmd = g_simple_action_new(ci->label, NULL);
            g_signal_connect_data(act_cmd, "activate", G_CALLBACK(activate_command),
                                  (gpointer)new activate_command_params(gui, i),
                                  (GClosureNotify)[](gpointer d, GClosure*){ delete (activate_command_params*)d; },
                                  (GConnectFlags)0);
            g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_cmd));
            g_object_unref(act_cmd);
        }
    }

    gtk_widget_insert_action_group(win, "win", G_ACTION_GROUP(win_actions));

    // Build GMenu model
    menu_model = g_menu_new();

    GMenu *preset_submenu = g_menu_new();
    g_menu_append(preset_submenu, "Store preset", "win.store-preset");

    char ch = '0';
    // We need preset_submenu attached before fill_gui_presets can find it
    GMenuItem *preset_item = g_menu_item_new_submenu("_Preset", G_MENU_MODEL(preset_submenu));
    g_menu_append_item(menu_model, preset_item);
    g_object_unref(preset_item);
    g_object_unref(preset_submenu);

    fill_gui_presets(true, ch);
    fill_gui_presets(false, ch);

    // Commands submenu
    ci = _jh->get_metadata_iface()->get_commands();
    if (ci && ci->name)
    {
        GMenu *cmd_menu = g_menu_new();
        plugin_command_info *ci2 = _jh->get_metadata_iface()->get_commands();
        for (; ci2->name; ci2++)
        {
            string action_str = string("win.") + ci2->label;
            g_menu_append(cmd_menu, ci2->name, action_str.c_str());
        }
        GMenuItem *cmd_item = g_menu_item_new_submenu("_Commands", G_MENU_MODEL(cmd_menu));
        g_menu_append_item(menu_model, cmd_item);
        g_object_unref(cmd_item);
        g_object_unref(cmd_menu);
    }

    // Help menu
    GMenu *help_menu = g_menu_new();
    g_menu_append(help_menu, "_Help", "win.help");
    g_menu_append(help_menu, "_Tips and tricks...", "win.tips-tricks");
    g_menu_append(help_menu, "_About...", "win.about");
    GMenuItem *help_item = g_menu_item_new_submenu("_Help", G_MENU_MODEL(help_menu));
    g_menu_append_item(menu_model, help_item);
    g_object_unref(help_item);
    g_object_unref(help_menu);

    GtkWidget *menubar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menu_model));
    gtk_widget_set_name(menubar, "Calf-Menu");
    gtk_widget_add_css_class(menubar, "Calf-Menu");
    gtk_box_append(GTK_BOX(vbox), menubar);

    // Determine size without content
    // Use minimum (not natural) to match GTK2 gtk_widget_size_request() behavior
    gtk_widget_set_visible(GTK_WIDGET(vbox), TRUE);
    gtk_widget_measure(GTK_WIDGET(vbox), GTK_ORIENTATION_HORIZONTAL, -1, &req2.width, NULL, NULL, NULL);
    gtk_widget_measure(GTK_WIDGET(vbox), GTK_ORIENTATION_VERTICAL,   -1, &req2.height, NULL, NULL, NULL);

    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_visible(GTK_WIDGET(sw), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(eventbox));
    gtk_widget_set_name(GTK_WIDGET(sw), "Calf-Container");
    gtk_widget_add_css_class(GTK_WIDGET(sw), "Calf-Container");
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_box_append(GTK_BOX(vbox), sw);

    show_rack_ears(environment->get_config()->rack_ears);

    int nat_w = 0, nat_h = 0;
    gtk_widget_measure(GTK_WIDGET(container), GTK_ORIENTATION_HORIZONTAL, -1, &req.width,  &nat_w, NULL, NULL);
    gtk_widget_measure(GTK_WIDGET(container), GTK_ORIENTATION_VERTICAL,   -1, &req.height, &nat_h, NULL, NULL);
    int wx = max(req.width + 10, req2.width);
    int wy = req.height + req2.height + 10;
    gtk_window_set_default_size(GTK_WINDOW(win), wx, wy);

    if (environment->check_condition("debug-layout")) {
        g_print("[debug-layout] ========== plugin: %s ==========\n", title);
        g_print("[debug-layout]   window     default=%dx%d\n", wx, wy);
        g_print("[debug-layout]   menubar    min=%dx%d\n",
                req2.width, req2.height);
        g_print("[debug-layout]   container  min=%dx%d  natural=%dx%d\n",
                req.width, req.height, nat_w, nat_h);
        g_print("[debug-layout]   --- widget tree (depth <=4, min/nat sizes, CSS color) ---\n");
        debug_print_widget_tree(container, 0);
        fflush(stdout);
    }
    g_signal_connect (GTK_WIDGET(win), "destroy", G_CALLBACK (on_window_destroyed), (plugin_gui_widget *)this);
    if (main)
        main->set_window(gui->plugin, this);

    toplevel = win;
    notifier = environment->get_config_db()->add_listener(this);
}

void plugin_gui_window::on_config_change()
{
    environment->get_config()->load(environment->get_config_db());
    show_rack_ears(environment->get_config()->rack_ears);
}

void plugin_gui_window::close()
{
    gtk_window_destroy(GTK_WINDOW(toplevel));
}

static GtkWidget *image_from_pixbuf(GdkPixbuf *pb)
{
    if (!pb)
        return gtk_image_new();
    int w      = gdk_pixbuf_get_width(pb);
    int h      = gdk_pixbuf_get_height(pb);
    int nc     = gdk_pixbuf_get_n_channels(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    GdkMemoryFormat fmt = (nc == 4) ? GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8;
    GBytes *bytes = g_bytes_new(gdk_pixbuf_get_pixels(pb), (gsize)h * stride);
    GdkTexture *tex = gdk_memory_texture_new(w, h, fmt, bytes, stride);
    g_bytes_unref(bytes);
    GtkWidget *img = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
    g_object_unref(tex);
    return img;
}

GtkWidget *plugin_gui_window::decorate(GtkWidget *widget) {
    GtkWidget *decoTable = gtk_grid_new();

    // images for left side
    GtkWidget *nwImg     = image_from_pixbuf(environment->get_image_factory()->get("side_nw"));
    GtkWidget *swImg     = image_from_pixbuf(environment->get_image_factory()->get("side_sw"));

    // images for right side
    GtkWidget *neImg     = image_from_pixbuf(environment->get_image_factory()->get("side_ne"));
    GtkWidget *seImg     = image_from_pixbuf(environment->get_image_factory()->get("side_se"));

    // pack left box — replace GtkEventBox with GtkBox
    leftBG = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(leftBG), leftBox);
    gtk_box_append(GTK_BOX(leftBox), GTK_WIDGET(nwImg));
    // prepend swImg so it goes to the "end"
    GtkWidget *leftBoxSpacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(leftBoxSpacer, TRUE);
    gtk_box_append(GTK_BOX(leftBox), leftBoxSpacer);
    gtk_box_append(GTK_BOX(leftBox), GTK_WIDGET(swImg));
    gtk_widget_set_name(leftBG, "CalfPluginLeft");
    gtk_widget_add_css_class(leftBG, "CalfPluginLeft");

    // pack right box — replace GtkEventBox with GtkBox
    rightBG = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *rightBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(rightBG), rightBox);
    gtk_box_append(GTK_BOX(rightBox), GTK_WIDGET(neImg));
    GtkWidget *rightBoxSpacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(rightBoxSpacer, TRUE);
    gtk_box_append(GTK_BOX(rightBox), rightBoxSpacer);
    gtk_box_append(GTK_BOX(rightBox), GTK_WIDGET(seImg));
    gtk_widget_set_name(rightBG, "CalfPluginRight");
    gtk_widget_add_css_class(rightBG, "CalfPluginRight");

    // gtk_table_attach equivalents using gtk_grid_attach(grid, child, col, row, w, h)
    // left column: col=0, row=0, span 1x1
    gtk_widget_set_vexpand(leftBG, TRUE);
    gtk_grid_attach(GTK_GRID(decoTable), GTK_WIDGET(leftBG),  0, 0, 1, 1);
    // right column: col=2, row=0
    gtk_widget_set_vexpand(rightBG, TRUE);
    gtk_grid_attach(GTK_GRID(decoTable), GTK_WIDGET(rightBG), 2, 0, 1, 1);
    // content: col=1, row=0, with margins
    gtk_widget_set_hexpand(widget, TRUE);
    gtk_widget_set_vexpand(widget, TRUE);
    gtk_widget_set_margin_start(widget, 15);
    gtk_widget_set_margin_end(widget, 15);
    gtk_widget_set_margin_top(widget, 5);
    gtk_widget_set_margin_bottom(widget, 5);
    gtk_grid_attach(GTK_GRID(decoTable), widget,              1, 0, 1, 1);

    gtk_widget_set_visible(decoTable, TRUE);
    return GTK_WIDGET(decoTable);
}


void plugin_gui_window::show_rack_ears(bool show)
{
    // if hidden, add a no-show-all attribute so that LV2 host doesn't accidentally override
    // the setting by doing a show_all on the outermost container
    if (show)
    {
        gtk_widget_set_visible(leftBG, TRUE);
        gtk_widget_set_visible(rightBG, TRUE);
    }
    else
    {
        gtk_widget_set_visible(leftBG, FALSE);
        gtk_widget_set_visible(rightBG, FALSE);
    }
}
plugin_gui_window::~plugin_gui_window()
{
    if (notifier)
    {
        delete notifier;
        notifier = NULL;
    }
    if (win_actions)
    {
        g_object_unref(win_actions);
        win_actions = NULL;
    }
    if (menu_model)
    {
        g_object_unref(menu_model);
        menu_model = NULL;
    }
    if (main)
        main->set_window(gui->plugin, NULL);
}
