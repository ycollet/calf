/* Calf DSP Library
 * GUI main window.
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

#include <calf/gtk_main_win.h>

using namespace calf_plugins;
using namespace std;

// Forward declaration (defined later in this file)
static int run_dialog_sync(GtkDialog *dialog);

// Helper: create a GtkWidget image from a GdkPixbuf (gtk_image_new_from_pixbuf deprecated in GTK4)
static GtkWidget *image_from_pixbuf(GdkPixbuf *pb)
{
    if (!pb) return gtk_image_new();
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int nc = gdk_pixbuf_get_n_channels(pb), stride = gdk_pixbuf_get_rowstride(pb);
    GdkMemoryFormat fmt = (nc == 4) ? GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8;
    GBytes *bytes = g_bytes_new(gdk_pixbuf_get_pixels(pb), (gsize)h * stride);
    GdkTexture *tex = gdk_memory_texture_new(w, h, fmt, bytes, stride);
    g_bytes_unref(bytes);
    GtkWidget *img = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
    g_object_unref(tex);
    return img;
}

gtk_main_window::gtk_main_window()
{
    toplevel = NULL;
    owner = NULL;
    notifier = NULL;
    is_closed = true;
    progress_window = NULL;
    images = image_factory();
}

void gtk_main_window::on_open_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    ((gtk_main_window *)data)->open_file();
}

void gtk_main_window::on_save_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    ((gtk_main_window *)data)->save_file();
}

void gtk_main_window::on_save_as_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    ((gtk_main_window *)data)->save_file_as();
}

void gtk_main_window::on_reorder_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    ((gtk_main_window *)data)->owner->reorder_plugins();
}

void gtk_main_window::on_preferences_action(GSimpleAction *action, GVariant *param, gpointer data)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_main_window *main = (gtk_main_window *)data;
    GtkBuilder *prefs_builder = gtk_builder_new();
    GError *error = NULL;
    const gchar *objects[] = { "preferences", NULL };
    if (!gtk_builder_add_objects_from_file(prefs_builder, PKGLIBDIR "/calf-gui.xml", (const char **)objects, &error))
    {
        g_warning("Cannot load preferences dialog: %s", error->message);
        g_error_free(error);
        g_object_unref(G_OBJECT(prefs_builder));
        return;
    }

    // styles selector
    GtkCellRenderer *cell;
    GtkListStore *styles = main->get_styles();
    GtkComboBox *cb = GTK_COMBO_BOX(gtk_builder_get_object(prefs_builder, "rcstyles"));
    gtk_combo_box_set_model(cb, GTK_TREE_MODEL(styles));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cb), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cb), cell, "text", 0, NULL);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(styles), &iter);
    while (valid) {
        GValue path = G_VALUE_INIT;
        gtk_tree_model_get_value(GTK_TREE_MODEL(styles), &iter, 1, &path);
        if (main->get_config()->style.compare(g_value_get_string(&path)) == 0) {
            gtk_combo_box_set_active_iter(cb, &iter);
            break;
        }
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(styles), &iter);
        g_value_unset(&path);
    }

    GtkComboBoxText *rack_float = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(prefs_builder, "rack-float"));
    gtk_combo_box_text_append_text(rack_float, "Rows");
    gtk_combo_box_text_append_text(rack_float, "Columns");
    gtk_combo_box_set_active(GTK_COMBO_BOX(rack_float), main->get_config()->rack_float);
    GtkWidget *preferences_dlg = GTK_WIDGET(gtk_builder_get_object(prefs_builder, "preferences"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "show-rack-ears")), main->get_config()->rack_ears);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "win-to-tray")), main->get_config()->win_to_tray);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "win-start-hidden")), main->get_config()->win_start_hidden);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(prefs_builder, "float-size")), 1, 32);
    gtk_spin_button_set_increments(GTK_SPIN_BUTTON(gtk_builder_get_object(prefs_builder, "float-size")), 1, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(prefs_builder, "float-size")), main->get_config()->float_size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "show-vu-meters")), main->get_config()->vu_meters);
    int response = run_dialog_sync(GTK_DIALOG(preferences_dlg));
    if (response == GTK_RESPONSE_OK)
    {
        GValue path_ = G_VALUE_INIT;
        GtkTreeIter iter;
        gtk_combo_box_get_active_iter(cb, &iter);
        gtk_tree_model_get_value(GTK_TREE_MODEL(styles), &iter, 1, &path_);
        main->get_config()->rack_ears = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "show-rack-ears")));
        main->get_config()->win_to_tray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "win-to-tray")));
        main->get_config()->win_start_hidden = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "win-start-hidden")));
        main->get_config()->rack_float = gtk_combo_box_get_active(GTK_COMBO_BOX(rack_float));
        main->get_config()->float_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(prefs_builder, "float-size")));
        main->get_config()->vu_meters = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(prefs_builder, "show-vu-meters")));
        main->get_config()->style = g_value_get_string(&path_);
        main->get_config()->save(main->get_config_db());
        //main->load_style(g_value_get_string(&path_));
        g_value_unset(&path_);
    }
    gtk_window_destroy(GTK_WINDOW(preferences_dlg));
    g_object_unref(G_OBJECT(prefs_builder));
G_GNUC_END_IGNORE_DEPRECATIONS
}

void gtk_main_window::on_exit_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    gtk_main_window *main = (gtk_main_window *)data;
    gtk_window_destroy(GTK_WINDOW(main->toplevel));
}

void gtk_main_window::add_plugin(jack_host *plugin)
{
    if (toplevel)
    {
        plugin_strip *strip = create_strip(plugin);
        plugins[plugin] = strip;
        update_strip(plugin);
        sort_strips();
    }
    else {
        plugin_queue.push_back(plugin);
        //plugins[plugin] = NULL;
    }
}

void gtk_main_window::del_plugin(plugin_ctl_iface *plugin)
{
    if (!plugins.count(plugin))
        return;
    plugin_strip *strip = plugins[plugin];
    if (strip->gui_win)
        strip->gui_win->close();
    vector<GtkWidget *> to_destroy;
    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        if (i->second == strip)
            to_destroy.push_back(i->second->strip_table);
        else if(i->second->id > strip->id)
            i->second->id--;
    }
    for (unsigned int i = 0; i < to_destroy.size(); i++)
        gtk_grid_remove(GTK_GRID(strips_table), to_destroy[i]);
    plugins.erase(plugin);
    sort_strips();
}

void gtk_main_window::rename_plugin(plugin_ctl_iface *plugin, std::string name)
{
    if (!plugins.count(plugin))
        return;
    plugin_strip *strip = plugins[plugin];
    gtk_label_set_text(GTK_LABEL(strip->name), name.c_str());
    if (strip->gui_win) {
        gtk_window_set_title(GTK_WINDOW(strip->gui_win->toplevel), ("Calf - " + name).c_str());
    }
}

void gtk_main_window::set_window(plugin_ctl_iface *plugin, plugin_gui_window *gui_win)
{
    if (!plugins.count(plugin))
        return;
    plugin_strip *strip = plugins[plugin];
    if (!strip)
        return;
    strip->gui_win = gui_win;
    if (!is_closed)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(strip->button), gui_win != NULL ? TRUE : FALSE);
}

void gtk_main_window::refresh_all_presets(bool builtin_too)
{
    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        if (i->second && i->second->gui_win) {
            char ch = '0';
            i->second->gui_win->fill_gui_presets(true, ch);
            i->second->gui_win->fill_gui_presets(false, ch);
        }
    }
}

static gboolean
gui_button_pressed(GtkToggleButton *button, plugin_strip *strip)
{
    if (gtk_toggle_button_get_active(button) == (strip->gui_win != NULL))
        return FALSE;
    if (strip->gui_win) {
        strip->gui_win->close();
        strip->gui_win = NULL;
    } else {
        strip->main_win->open_gui(strip->plugin);
    }
    return TRUE;
}
static gboolean
connect_button_pressed(GtkWidget *button, plugin_strip *strip)
{
    if (strip->connector) {
        strip->connector->close();
        strip->connector = NULL;
    } else {
        strip->connector = new calf_connector(strip);
    }
    return TRUE;
}
static gboolean
extra_button_pressed(GtkWidget *button, plugin_strip *strip)
{
    if (strip->connector)
        strip->connector->close();
    strip->main_win->owner->remove_plugin(strip->plugin);
    return TRUE;
}

void gtk_main_window::show_rack_ears(bool show)
{
    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        if (show)
        {
            gtk_widget_set_visible(i->second->leftBG, TRUE);
            gtk_widget_set_visible(i->second->rightBG, TRUE);
        }
        else
        {
            gtk_widget_set_visible(i->second->leftBG, FALSE);
            gtk_widget_set_visible(i->second->rightBG, FALSE);
        }
    }
}

void gtk_main_window::show_vu_meters(bool show)
{
    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        if (show)
        {
            if (i->second->inBox)
                gtk_widget_set_visible(i->second->inBox, TRUE);
            if (i->second->outBox)
                gtk_widget_set_visible(i->second->outBox, TRUE);
        }
        else
        {
            if (i->second->inBox)
                gtk_widget_set_visible(i->second->inBox, FALSE);
            if (i->second->outBox)
                gtk_widget_set_visible(i->second->outBox, FALSE);
        }
    }
}

void gtk_main_window::on_edit_title(GtkGestureClick *gesture, int n_press, double x, double y, plugin_strip *strip) {
    gtk_editable_set_text(GTK_EDITABLE(strip->entry), gtk_label_get_text(GTK_LABEL(strip->name)));
    gtk_widget_grab_focus(strip->entry);
    gtk_widget_set_visible(strip->name, FALSE);
    gtk_widget_set_visible(strip->entry, TRUE);
}

void gtk_main_window::on_activate_entry(GtkWidget *entry, plugin_strip *strip) {
    const char *txt = gtk_editable_get_text(GTK_EDITABLE(entry));
    const char *old = gtk_label_get_text(GTK_LABEL(strip->name));
    if (strcmp(old, txt) and strlen(txt))
        strip->main_win->owner->rename_plugin(strip->plugin, txt);
    gtk_widget_set_visible(strip->entry, FALSE);
    gtk_widget_set_visible(strip->name, TRUE);
}

void gtk_main_window::on_blur_entry(plugin_strip *strip) {
    gtk_widget_set_visible(strip->entry, FALSE);
    gtk_widget_set_visible(strip->name, TRUE);
}

GtkWidget *gtk_main_window::create_vu_meter() {
    GtkWidget *vu = calf_vumeter_new();
    calf_vumeter_set_falloff(CALF_VUMETER(vu), 2.5);
    calf_vumeter_set_hold(CALF_VUMETER(vu), 1.5);
    calf_vumeter_set_width(CALF_VUMETER(vu), 100);
    calf_vumeter_set_height(CALF_VUMETER(vu), 12);
    calf_vumeter_set_position(CALF_VUMETER(vu), 2);
    return vu;
}

GtkWidget *gtk_main_window::create_meter_scale() {
    GtkWidget *vu = calf_meter_scale_new();
    CalfMeterScale *ms = CALF_METER_SCALE(vu);
    const unsigned long sz = 7;
    const double cv[sz] = {0., 0.0625, 0.125, 0.25, 0.5, 0.71, 1.};
    const vector<double> ck(cv, &cv[sz]);
    ms->marker   = ck;
    ms->dots     = 1;
    ms->position = 2;
    gtk_widget_set_name(vu, "Calf-MeterScale");
    return vu;
}

void gtk_main_window::on_table_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    GtkWidget *widget = GTK_WIDGET(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));
    gtk_widget_grab_focus(widget);
}

plugin_strip *gtk_main_window::create_strip(jack_host *plugin)
{
    /*    0          1    2             3      4          5           6          7
     *  0 ┌──────────┬────────────────────────────────────────────────┬──────────┐ 0
     *    │          │                   top light                    │          │
     *  1 │          ├────┬─────────────┬──────┬──────────┬───────────┤          │ 1
     *    │          │    │             │      │          │           │          │
     *    │          │ X  │    label    │ MIDI │  inputs  │  outputs  │          │
     *    │          │    │             │      │          │           │          │
     *  2 │   left   ├────┼─────────────┼──────┼──────────┼───────────┤   right  │ 2
     *    │   box    │    ╎             ╎      ╎          ╎           │    box   │
     *    │          │   buttons and params    ╎          ╎           │          │
     *    │          │    ╎             ╎      ╎          ╎           │          │
     *  3 │          ├────┴─────────────┴──────┴──────────┴───────────┤          │ 3
     *    │          │                  bottom light                  │          │
     *  4 └──────────┴────────────────────────────────────────────────┴──────────┘ 4
     *    0          1    2             3      4          5           6          7
     */

    plugin_strip *strip = new plugin_strip;
    strip->main_win = this;
    strip->plugin = plugin;
    strip->gui_win = NULL;
    strip->connector = NULL;
    strip->id = plugins.size();

    const plugin_metadata_iface *metadata = plugin->get_metadata_iface();

    strip->strip_table = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(strip->strip_table), 0);
    gtk_grid_set_row_spacing(GTK_GRID(strip->strip_table), 0);

    // images for left side
    GtkWidget *nwImg     = image_from_pixbuf(images.get("side_d_nw"));
    GtkWidget *swImg     = image_from_pixbuf(images.get("side_d_sw"));
    GtkWidget *wImg      = image_from_pixbuf(images.get("side_d_w"));
    gtk_widget_set_size_request(GTK_WIDGET(wImg), 56, 1);

    // images for right side
    GtkWidget *neImg     = image_from_pixbuf(images.get("side_d_ne"));
    GtkWidget *seImg     = image_from_pixbuf(images.get("side_d_se"));
    GtkWidget *eImg      = image_from_pixbuf(images.get("side_d_e"));
    gtk_widget_set_size_request(GTK_WIDGET(eImg), 56, 1);

    // pack left box @ column 0, rows 0-3
    GtkWidget *leftBG  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(leftBG), leftBox);
    gtk_widget_set_name(leftBG, "CalfMainLeft");
    gtk_box_append(GTK_BOX(leftBox), GTK_WIDGET(nwImg));
    gtk_box_prepend(GTK_BOX(leftBox), GTK_WIDGET(swImg));
    gtk_widget_set_visible(GTK_WIDGET(leftBG), TRUE);
    if (!get_config()->rack_ears)
        gtk_widget_set_visible(GTK_WIDGET(leftBG), FALSE);
    gtk_grid_attach(GTK_GRID(strip->strip_table), leftBG, 0, 0, 1, 4);
    gtk_widget_set_vexpand(leftBG, TRUE);

    // pack right box @ column 6, rows 0-3
    GtkWidget *rightBG = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *rightBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(rightBG), rightBox);
    gtk_widget_set_name(rightBG, "CalfMainRight");
    gtk_box_append(GTK_BOX(rightBox), GTK_WIDGET(neImg));
    gtk_box_prepend(GTK_BOX(rightBox), GTK_WIDGET(seImg));
    gtk_widget_set_visible(GTK_WIDGET(rightBG), TRUE);
    if (!get_config()->rack_ears)
        gtk_widget_set_visible(GTK_WIDGET(rightBG), FALSE);
    gtk_grid_attach(GTK_GRID(strip->strip_table), rightBG, 6, 0, 1, 4);
    gtk_widget_set_vexpand(rightBG, TRUE);

    strip->leftBG = leftBG;
    strip->rightBG = rightBG;

    // top light @ columns 1-5, row 0
    GtkWidget *topImg     = image_from_pixbuf(images.get("light_top"));
    gtk_widget_set_size_request(GTK_WIDGET(topImg), 1, 1);
    gtk_grid_attach(GTK_GRID(strip->strip_table), topImg, 1, 0, 5, 1);
    gtk_widget_set_hexpand(topImg, TRUE);
    gtk_widget_set_visible(topImg, TRUE);

    // remove button @ column 1, row 1
    strip->extra = calf_button_new("×");
    g_signal_connect(G_OBJECT(strip->extra), "clicked", G_CALLBACK(extra_button_pressed),
        (plugin_ctl_iface *)strip);
    gtk_widget_set_visible(strip->extra, TRUE);
    gtk_widget_set_margin_start(strip->extra, 5);
    gtk_widget_set_margin_end(strip->extra, 5);
    gtk_widget_set_margin_top(strip->extra, 5);
    gtk_widget_set_margin_bottom(strip->extra, 5);
    gtk_grid_attach(GTK_GRID(strip->strip_table), GTK_WIDGET(strip->extra), 1, 1, 1, 1);

    // title @ column 2, row 1
    strip->name = gtk_label_new(NULL);
    gtk_widget_set_name(GTK_WIDGET(strip->name), "Calf-Rack-Title");
    gtk_label_set_markup(GTK_LABEL(strip->name), plugin->instance_name.c_str());
    gtk_label_set_justify(GTK_LABEL(strip->name), GTK_JUSTIFY_RIGHT);

    GtkWidget *ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(ebox, 180, -1);
    gtk_widget_set_halign(strip->name, GTK_ALIGN_START);
    gtk_widget_set_valign(strip->name, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(ebox), strip->name);
    GtkGesture *title_gesture = gtk_gesture_click_new();
    g_signal_connect(title_gesture, "pressed", G_CALLBACK(on_edit_title), strip);
    gtk_widget_add_controller(ebox, GTK_EVENT_CONTROLLER(title_gesture));
    gtk_widget_set_margin_start(ebox, 10);
    gtk_widget_set_margin_end(ebox, 10);
    gtk_grid_attach(GTK_GRID(strip->strip_table), ebox, 2, 1, 1, 1);
    gtk_widget_set_vexpand(ebox, TRUE);
    gtk_widget_set_visible(ebox, TRUE);
    gtk_widget_set_visible(strip->name, TRUE);

    strip->entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(strip->entry), "Calf-Rack-Entry");
    gtk_widget_set_name(strip->entry, "Calf-Rack-Entry");
    gtk_widget_set_size_request(strip->entry, 180, -1);
    gtk_widget_set_margin_start(strip->entry, 10);
    gtk_widget_set_margin_end(strip->entry, 10);
    gtk_grid_attach(GTK_GRID(strip->strip_table), strip->entry, 2, 1, 1, 1);
    gtk_widget_set_vexpand(strip->entry, TRUE);
    gtk_widget_set_visible(strip->entry, TRUE);
    g_signal_connect(G_OBJECT(strip->entry), "activate", G_CALLBACK(on_activate_entry), strip);
    {
        GtkEventController *focus_ctrl = gtk_event_controller_focus_new();
        g_signal_connect_swapped(focus_ctrl, "leave", G_CALLBACK(on_blur_entry), strip);
        gtk_widget_add_controller(strip->entry, focus_ctrl);
    }
    gtk_widget_set_visible(strip->entry, FALSE);

    // open button
    strip->button = calf_toggle_button_new("Open");
    g_signal_connect(G_OBJECT(strip->button), "toggled", G_CALLBACK(gui_button_pressed),
        (plugin_ctl_iface *)strip);
    gtk_widget_set_visible(strip->button, TRUE);

    // connect button
    strip->con = calf_toggle_button_new("Connect");
    g_signal_connect(G_OBJECT(strip->con), "toggled", G_CALLBACK(connect_button_pressed),
        (plugin_ctl_iface *)strip);
    gtk_widget_set_visible(strip->con, TRUE);

    // button box @ columns 1-2, row 2
    GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(buttonBox, GTK_ALIGN_START);
    gtk_widget_set_valign(buttonBox, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(buttonBox), GTK_WIDGET(strip->button));
    gtk_box_append(GTK_BOX(buttonBox), GTK_WIDGET(strip->con));
    gtk_widget_set_margin_start(buttonBox, 5);
    gtk_widget_set_margin_end(buttonBox, 5);
    gtk_widget_set_margin_top(buttonBox, 5);
    gtk_widget_set_margin_bottom(buttonBox, 5);
    gtk_grid_attach(GTK_GRID(strip->strip_table), buttonBox, 1, 2, 2, 1);
    gtk_widget_set_hexpand(buttonBox, TRUE);
    gtk_widget_set_vexpand(buttonBox, TRUE);
    gtk_widget_set_visible(buttonBox, TRUE);

    // param box @ columns 3-5, row 2
    plugin_gui_widget *widget = new plugin_gui_widget(this, this);
    strip->gui_widget = widget;
    GtkWidget *paramBox = widget->create(plugin);
    gtk_widget_set_halign(paramBox, GTK_ALIGN_END);
    gtk_widget_set_valign(paramBox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(paramBox, 5);
    gtk_widget_set_margin_end(paramBox, 5);
    gtk_widget_set_margin_top(paramBox, 5);
    gtk_widget_set_margin_bottom(paramBox, 5);
    gtk_grid_attach(GTK_GRID(strip->strip_table), paramBox, 3, 2, 3, 1);
    gtk_widget_set_hexpand(paramBox, TRUE);
    gtk_widget_set_visible(paramBox, TRUE);

    // midi box @ column 3, row 1
    if (metadata->get_midi()) {
        GtkWidget *led = calf_led_new();
        GtkWidget *midiBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        GtkWidget *midiLabel = gtk_label_new("MIDI");
        GtkWidget *midiSpacer = gtk_label_new("");
        gtk_box_append(GTK_BOX(midiBox), midiLabel);
        gtk_box_append(GTK_BOX(midiBox), GTK_WIDGET(led));
        gtk_box_append(GTK_BOX(midiBox), midiSpacer);
        gtk_widget_set_vexpand(midiSpacer, TRUE);
        gtk_widget_set_margin_start(midiBox, 5);
        gtk_widget_set_margin_end(midiBox, 5);
        gtk_widget_set_margin_top(midiBox, 3);
        gtk_widget_set_margin_bottom(midiBox, 3);
        gtk_grid_attach(GTK_GRID(strip->strip_table), midiBox, 3, 1, 1, 1);
        gtk_widget_set_size_request(GTK_WIDGET(led), 25, 25);
        strip->midi_in = led;
        gtk_widget_set_visible(midiBox, TRUE);
        gtk_widget_set_visible(led, TRUE);
        gtk_widget_set_visible(midiLabel, TRUE);
        gtk_widget_set_visible(midiSpacer, TRUE);
    } else {
        GtkWidget *led = gtk_label_new("");
        gtk_widget_set_margin_start(led, 5);
        gtk_widget_set_margin_end(led, 5);
        gtk_widget_set_margin_top(led, 3);
        gtk_widget_set_margin_bottom(led, 3);
        gtk_grid_attach(GTK_GRID(strip->strip_table), led, 3, 1, 1, 1);
        gtk_widget_set_hexpand(led, FALSE);
        gtk_widget_set_vexpand(led, TRUE);
        gtk_widget_set_size_request(GTK_WIDGET(led), 25, 25);
        strip->midi_in = led;
        gtk_widget_set_visible(strip->midi_in, TRUE);
    }

    strip->inBox = NULL;
    strip->outBox = NULL;
    strip->audio_in.clear();
    strip->audio_out.clear();

    GtkWidget *vu;
    if (metadata->get_input_count()) {

        GtkWidget *inBox  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

        gtk_box_append(GTK_BOX(inBox), gtk_label_new("Audio In"));

        for (int i = 0; i < metadata->get_input_count(); i++)
        {
            vu = create_vu_meter();
            gtk_box_append(GTK_BOX(inBox), vu);
            gtk_widget_set_hexpand(vu, TRUE);
            gtk_widget_set_vexpand(vu, TRUE);
            strip->audio_in.push_back(vu);
        }
        vu = create_meter_scale();
        gtk_box_append(GTK_BOX(inBox), vu);
        gtk_widget_set_hexpand(vu, TRUE);
        gtk_widget_set_vexpand(vu, TRUE);

        // inBox is the inner box directly (no alignment wrapper)
        strip->inBox = inBox;
        gtk_widget_set_hexpand(strip->inBox, TRUE);
        gtk_widget_set_halign(strip->inBox, GTK_ALIGN_FILL);
        gtk_widget_set_valign(strip->inBox, GTK_ALIGN_START);
        gtk_widget_set_margin_start(strip->inBox, 5);
        gtk_widget_set_margin_end(strip->inBox, 5);
        gtk_widget_set_margin_top(strip->inBox, 3);
        gtk_widget_set_margin_bottom(strip->inBox, 3);

        gtk_grid_attach(GTK_GRID(strip->strip_table), strip->inBox, 4, 1, 1, 1);
        gtk_widget_set_hexpand(strip->inBox, TRUE);
        gtk_widget_set_vexpand(strip->inBox, TRUE);

        if (get_config()->vu_meters)
            gtk_widget_set_visible(strip->inBox, TRUE);

        gtk_widget_set_size_request(GTK_WIDGET(strip->inBox), 180, -1);
    } else {
        GtkWidget *inBox = gtk_label_new("");
        gtk_widget_set_margin_start(inBox, 5);
        gtk_widget_set_margin_end(inBox, 5);
        gtk_widget_set_margin_top(inBox, 3);
        gtk_widget_set_margin_bottom(inBox, 3);
        gtk_grid_attach(GTK_GRID(strip->strip_table), inBox, 4, 1, 1, 1);
        gtk_widget_set_hexpand(inBox, FALSE);
        gtk_widget_set_vexpand(inBox, TRUE);
        gtk_widget_set_size_request(GTK_WIDGET(inBox), 180, -1);
        gtk_widget_set_visible(inBox, TRUE);
    }

    if (metadata->get_output_count()) {

        GtkWidget *outBox  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

        GtkWidget *outLabel = gtk_label_new("Audio Out");
        gtk_box_append(GTK_BOX(outBox), outLabel);
        gtk_widget_set_hexpand(outLabel, TRUE);
        gtk_widget_set_vexpand(outLabel, TRUE);

        for (int i = 0; i < metadata->get_output_count(); i++)
        {
            vu = create_vu_meter();
            gtk_box_append(GTK_BOX(outBox), vu);
            gtk_widget_set_hexpand(vu, TRUE);
            gtk_widget_set_vexpand(vu, TRUE);
            strip->audio_out.push_back(vu);
        }
        vu = create_meter_scale();
        gtk_box_append(GTK_BOX(outBox), vu);
        gtk_widget_set_hexpand(vu, TRUE);
        gtk_widget_set_vexpand(vu, TRUE);

        // outBox is the inner box directly (no alignment wrapper)
        strip->outBox = outBox;
        gtk_widget_set_hexpand(strip->outBox, TRUE);
        gtk_widget_set_halign(strip->outBox, GTK_ALIGN_FILL);
        gtk_widget_set_valign(strip->outBox, GTK_ALIGN_START);
        gtk_widget_set_margin_start(strip->outBox, 5);
        gtk_widget_set_margin_end(strip->outBox, 5);
        gtk_widget_set_margin_top(strip->outBox, 3);
        gtk_widget_set_margin_bottom(strip->outBox, 3);

        gtk_grid_attach(GTK_GRID(strip->strip_table), strip->outBox, 5, 1, 1, 1);
        gtk_widget_set_hexpand(strip->outBox, TRUE);
        gtk_widget_set_vexpand(strip->outBox, TRUE);

        if (get_config()->vu_meters)
            gtk_widget_set_visible(strip->outBox, TRUE);

        gtk_widget_set_size_request(GTK_WIDGET(strip->outBox), 180, -1);
    } else {
        GtkWidget *outBox = gtk_label_new("");
        gtk_widget_set_margin_start(outBox, 5);
        gtk_widget_set_margin_end(outBox, 5);
        gtk_widget_set_margin_top(outBox, 3);
        gtk_widget_set_margin_bottom(outBox, 3);
        gtk_grid_attach(GTK_GRID(strip->strip_table), outBox, 5, 1, 1, 1);
        gtk_widget_set_hexpand(outBox, FALSE);
        gtk_widget_set_vexpand(outBox, TRUE);
        gtk_widget_set_size_request(GTK_WIDGET(outBox), 180, -1);
        gtk_widget_set_visible(outBox, TRUE);
    }


    // bottom light @ columns 1-5, row 3
    GtkWidget *botImg     = image_from_pixbuf(images.get("light_bottom"));
    gtk_widget_set_size_request(GTK_WIDGET(botImg), 1, 1);
    gtk_grid_attach(GTK_GRID(strip->strip_table), botImg, 1, 3, 5, 1);
    gtk_widget_set_hexpand(botImg, TRUE);
    gtk_widget_set_visible(botImg, TRUE);

    gtk_widget_set_visible(GTK_WIDGET(strip->strip_table), TRUE);

    return strip;
}

void gtk_main_window::sort_strips()
{
    if(plugins.size() <= 0) return;
    int rack_float = get_config()->rack_float; // 0=horiz, 1=vert
    int float_size = get_config()->float_size; // amount of rows/cols before line break
    int posx, posy;
    // GtkGrid auto-sizes; no resize call needed
    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        switch (rack_float) {
            case 0:
            default:
                posx = i->second->id % float_size;
                posy = (int)(i->second->id / float_size);
                break;
            case 1:
                posy = i->second->id % float_size;
                posx = (int)(i->second->id / float_size);
                break;
        }
        bool rem = false;
        if(gtk_widget_get_parent(i->second->strip_table) != NULL) {
            rem = true;
            g_object_ref(i->second->strip_table);
            gtk_grid_remove(GTK_GRID(strips_table), GTK_WIDGET(i->second->strip_table));
        }
        gtk_grid_attach(GTK_GRID(strips_table), i->second->strip_table, posx, posy, 1, 1);
        gtk_widget_set_hexpand(i->second->strip_table, TRUE);
        gtk_widget_set_vexpand(i->second->strip_table, TRUE);
        if(rem) g_object_unref(i->second->strip_table);
    }
}

void gtk_main_window::update_strip(plugin_ctl_iface *plugin)
{
    // plugin_strip *strip = plugins[plugin];
    // assert(strip);

}

void gtk_main_window::open_gui(plugin_ctl_iface *plugin)
{
    plugin_gui_window *gui_win = new plugin_gui_window(this, this);
    std::string title = "Calf - ";
    gui_win->create(plugin, (title + ((jack_host *)plugin)->get_instance_name()).c_str(), plugin->get_metadata_iface()->get_id());
    gtk_widget_set_visible(GTK_WIDGET(gui_win->toplevel), TRUE);
    plugins[plugin]->gui_win = gui_win;
}

#define countof(X) ( (size_t) ( sizeof(X)/sizeof*(X) ) )

void gtk_main_window::register_icons()
{
    // icon factories removed in GTK4
}

void gtk_main_window::add_plugin_action(GSimpleAction *action, GVariant *param, gpointer data)
{
    add_plugin_params *app = (add_plugin_params *)data;
    app->main_win->new_plugin(app->name.c_str());
}

static void action_destroy_notify(gpointer data)
{
    delete (gtk_main_window::add_plugin_params *)data;
}

void gtk_main_window::fill_plugin_menu(GMenu *plugin_menu)
{
    const plugin_registry::plugin_vector &plugins = plugin_registry::instance().get_all();
    std::string last_type = "";
    GMenu *submenu = NULL;
    int count_in_submenu = 0;

    for (size_t i = 0; i <= plugins.size(); i++) {
        if (i == plugins.size()) {
            // flush last group
            if (submenu) {
                if (count_in_submenu == 1)
                    g_menu_append_section(plugin_menu, NULL, G_MENU_MODEL(submenu));
                else
                    g_menu_append_submenu(plugin_menu, last_type.c_str(), G_MENU_MODEL(submenu));
            }
            break;
        }
        const plugin_metadata_iface *p = plugins[i];
        std::string type = p->get_plugin_info().plugin_type;
        type = type.substr(0, type.length() - 6);

        if (type != last_type) {
            if (submenu) {
                if (count_in_submenu == 1)
                    g_menu_append_section(plugin_menu, NULL, G_MENU_MODEL(submenu));
                else
                    g_menu_append_submenu(plugin_menu, last_type.c_str(), G_MENU_MODEL(submenu));
            }
            submenu = g_menu_new();
            last_type = type;
            count_in_submenu = 0;
        }

        std::string action_name = std::string("add-plugin-") + p->get_id();
        std::string action_full = "win." + action_name;
        g_menu_append(submenu, p->get_label(), action_full.c_str());

        // Create and register the action
        GSimpleAction *act = g_simple_action_new(action_name.c_str(), NULL);
        g_signal_connect_data(act, "activate", G_CALLBACK(add_plugin_action),
            (gpointer)new add_plugin_params(this, p->get_id()),
            (GClosureNotify)action_destroy_notify, (GConnectFlags)0);
        g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act));
        count_in_submenu++;
    }
}


window_state describe_window(GtkWindow *win)
{
    window_state state = {};
    int width, height;
    gtk_window_get_default_size(win, &width, &height);
    state.x = 0;
    state.y = 0;
    state.width = width;
    state.height = height;
    return state;
}

void position_window(GtkWidget *win, window_state state)
{
    gtk_window_set_default_size(GTK_WINDOW(win), state.width, state.height);
}

static gint window_delete_cb(GtkWindow *window, gpointer data)
{
    // tray icon not available in GTK4; just allow normal close
    return FALSE;
}

static void window_destroy_cb(GtkWindow *window, gtk_main_window *main)
{
    main->owner->on_main_window_destroy();
}

static gint window_hide(gtk_main_window *main)
{
    main->winstate = describe_window(main->toplevel);
    gtk_widget_set_visible(GTK_WIDGET(main->toplevel), FALSE);
    return FALSE;
}

// Helper: run a GtkDialog synchronously (gtk_dialog_run removed in GTK4)
static void run_dialog_response_cb(GtkDialog *dialog, gint response, gpointer data)
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
    g_signal_connect(dialog, "response", G_CALLBACK(run_dialog_response_cb), &result);
    gtk_widget_set_visible(GTK_WIDGET(dialog), TRUE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return result;
}

void gtk_main_window::create()
{
    register_icons();
    toplevel = GTK_WINDOW(gtk_window_new());
    std::string title = "Calf JACK Host";
    if (!owner->get_client_name().empty())
        title = title + " - session: " + owner->get_client_name();
    gtk_window_set_title(toplevel, title.c_str());
    gtk_window_set_icon_name(toplevel, "calf");

    load_style((PKGLIBDIR "styles/" + get_config()->style).c_str());

    is_closed = false;
    gtk_window_set_resizable(toplevel, false);

    all_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Build action group
    win_actions = g_simple_action_group_new();

    GSimpleAction *act_open = g_simple_action_new("file-open", NULL);
    g_signal_connect(act_open, "activate", G_CALLBACK(on_open_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_open));

    GSimpleAction *act_save = g_simple_action_new("file-save", NULL);
    g_signal_connect(act_save, "activate", G_CALLBACK(on_save_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_save));

    GSimpleAction *act_save_as = g_simple_action_new("file-save-as", NULL);
    g_signal_connect(act_save_as, "activate", G_CALLBACK(on_save_as_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_save_as));

    GSimpleAction *act_reorder = g_simple_action_new("file-reorder", NULL);
    g_signal_connect(act_reorder, "activate", G_CALLBACK(on_reorder_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_reorder));

    GSimpleAction *act_prefs = g_simple_action_new("file-preferences", NULL);
    g_signal_connect(act_prefs, "activate", G_CALLBACK(on_preferences_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_prefs));

    GSimpleAction *act_quit = g_simple_action_new("file-quit", NULL);
    g_signal_connect(act_quit, "activate", G_CALLBACK(on_exit_action), this);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(act_quit));

    gtk_widget_insert_action_group(GTK_WIDGET(toplevel), "win", G_ACTION_GROUP(win_actions));

    // Build menu model
    menu_model = g_menu_new();

    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "_Open", "win.file-open");
    g_menu_append(file_menu, "_Save", "win.file-save");
    g_menu_append(file_menu, "Save _as...", "win.file-save-as");
    g_menu_append_section(file_menu, NULL, G_MENU_MODEL(g_menu_new()));
    g_menu_append(file_menu, "_Reorder plugins", "win.file-reorder");
    g_menu_append(file_menu, "_Preferences...", "win.file-preferences");
    g_menu_append(file_menu, "_Quit", "win.file-quit");
    g_menu_append_submenu(menu_model, "_File", G_MENU_MODEL(file_menu));

    GMenu *plugin_menu = g_menu_new();
    fill_plugin_menu(plugin_menu);
    g_menu_append_submenu(menu_model, "_Add plugin", G_MENU_MODEL(plugin_menu));

    GtkWidget *menubar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menu_model));
    gtk_widget_set_size_request(menubar, 640, -1);
    gtk_widget_set_name(menubar, "Calf-Menu");
    gtk_box_append(GTK_BOX(all_vbox), menubar);

    // Keyboard shortcuts
    GtkEventController *key_ctrl = gtk_shortcut_controller_new();
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(key_ctrl),
        gtk_shortcut_new(gtk_keyval_trigger_new('q', GDK_CONTROL_MASK),
                         gtk_named_action_new("win.file-quit")));
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(key_ctrl),
        gtk_shortcut_new(gtk_keyval_trigger_new('o', GDK_CONTROL_MASK),
                         gtk_named_action_new("win.file-open")));
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(key_ctrl),
        gtk_shortcut_new(gtk_keyval_trigger_new('s', GDK_CONTROL_MASK),
                         gtk_named_action_new("win.file-save")));
    gtk_widget_add_controller(GTK_WIDGET(toplevel), key_ctrl);

    strips_table = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(strips_table), 0);
    gtk_grid_set_row_spacing(GTK_GRID(strips_table), 0);

    for (std::vector<jack_host *>::iterator i = plugin_queue.begin(); i != plugin_queue.end(); ++i)
    {
        plugin_strip *st = create_strip(*i);
        plugins[*i] = st;
        update_strip(*i);
    }
    sort_strips();

    GtkWidget *evbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(evbox, "Calf-Rack");
    gtk_widget_set_can_focus(evbox, TRUE);
    GtkGesture *rack_gesture = gtk_gesture_click_new();
    g_signal_connect(rack_gesture, "pressed", G_CALLBACK(on_table_clicked), NULL);
    gtk_widget_add_controller(evbox, GTK_EVENT_CONTROLLER(rack_gesture));
    gtk_box_append(GTK_BOX(evbox), strips_table);
    gtk_box_append(GTK_BOX(all_vbox), evbox);
    gtk_window_set_child(GTK_WINDOW(toplevel), all_vbox);

    gtk_widget_set_name(GTK_WIDGET(strips_table), "Calf-Container");

    gtk_widget_set_visible(GTK_WIDGET(strips_table), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(evbox), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(all_vbox), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(toplevel), TRUE);

    source_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 1000/30, on_idle, this, NULL); // 30 fps should be enough for everybody

    notifier = get_config_db()->add_listener(this);
    on_config_change();
    g_signal_connect(G_OBJECT(toplevel), "destroy", G_CALLBACK(window_destroy_cb), this);
    g_signal_connect(G_OBJECT(toplevel), "close-request", G_CALLBACK(window_delete_cb), this);

    if (get_config()->win_start_hidden)
        g_idle_add((GSourceFunc)window_hide, this);
}

void gtk_main_window::create_status_icon()
{
    // GtkStatusIcon removed in GTK4; tray icon not implemented
}

void gtk_main_window::on_config_change()
{
    get_config()->load(get_config_db());
    show_rack_ears(get_config()->rack_ears);
    show_vu_meters(get_config()->vu_meters);
    sort_strips();
}

void gtk_main_window::refresh_plugin(plugin_ctl_iface *plugin)
{
    if (plugins.count(plugin))
    {
        if (plugins[plugin]->gui_win)
            plugins[plugin]->gui_win->refresh();
        if (plugins[plugin]->gui_widget)
            plugins[plugin]->gui_widget->refresh();
    }
}

void gtk_main_window::refresh_plugin_param(plugin_ctl_iface *plugin, int param_no)
{
    if (plugins.count(plugin))
    {
        if (plugins[plugin]->gui_win)
            plugins[plugin]->gui_win->get_gui()->refresh(param_no);
        if (plugins[plugin]->gui_widget)
            plugins[plugin]->gui_widget->get_gui()->refresh(param_no);
    }
}

void gtk_main_window::on_closed()
{
    if (notifier)
    {
        delete notifier;
        notifier = NULL;
    }
    if (source_id)
        g_source_remove(source_id);
    is_closed = true;
    toplevel = NULL;

    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = plugins.begin(); i != plugins.end(); ++i)
    {
        if (i->second && i->second->gui_win) {
            i->second->gui_win->close();
        }
    }
    plugins.clear();
}

static inline float LVL(float value)
{
    return value; //sqrt(value) * 0.75;
}

gboolean gtk_main_window::on_idle(void *data)
{
    gtk_main_window *self = (gtk_main_window *)data;

    self->owner->on_idle();

    if (!self->refresh_controller.check_redraw(GTK_WIDGET(self->toplevel)))
        return TRUE;

    for (std::map<plugin_ctl_iface *, plugin_strip *>::iterator i = self->plugins.begin(); i != self->plugins.end(); ++i)
    {
        if (i->second)
        {
            plugin_ctl_iface *plugin = i->first;
            plugin_strip *strip = i->second;
            int idx = 0;
            if (strip->inBox && gtk_widget_is_drawable(strip->inBox)) {
                for (int i = 0; i < (int)strip->audio_in.size(); i++) {
                    calf_vumeter_set_value(CALF_VUMETER(strip->audio_in[i]), LVL(plugin->get_level(idx++)));
                }
            }
            else
                idx += strip->audio_in.size();
            if (strip->outBox && gtk_widget_is_drawable(strip->outBox)) {
                for (int i = 0; i < (int)strip->audio_out.size(); i++) {
                    calf_vumeter_set_value(CALF_VUMETER(strip->audio_out[i]), LVL(plugin->get_level(idx++)));
                }
            }
            else
                idx += strip->audio_out.size();
            if (plugin->get_metadata_iface()->get_midi()) {
                calf_led_set_value(CALF_LED(strip->midi_in), plugin->get_level(idx++));
            }
        }
    }
    return TRUE;
}

static void on_open_file_done(GObject *src, GAsyncResult *res, gpointer data)
{
    gtk_main_window *win = (gtk_main_window *)data;
    GError *err = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &err);
    g_object_unref(src);
    if (file) {
        char *filename = g_file_get_path(file);
        g_object_unref(file);
        char *error = win->owner->open_file(filename);
        if (error)
            win->display_error(error, filename);
        else
            win->owner->set_current_filename(filename);
        g_free(filename);
        free(error);
    }
    if (err) g_error_free(err);
}

static void on_save_file_done(GObject *src, GAsyncResult *res, gpointer data)
{
    gtk_main_window *win = (gtk_main_window *)data;
    GError *err = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, &err);
    g_object_unref(src);
    if (file) {
        char *filename = g_file_get_path(file);
        g_object_unref(file);
        char *error = win->owner->save_file(filename);
        if (error)
            win->display_error(error, filename);
        else
            win->owner->set_current_filename(filename);
        g_free(filename);
        free(error);
    }
    if (err) g_error_free(err);
}

void gtk_main_window::open_file()
{
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Open File");
    gtk_file_dialog_open(dlg, toplevel, NULL, on_open_file_done, this);
}

bool gtk_main_window::save_file()
{
    if (owner->get_current_filename().empty())
        return save_file_as();

    const char *error = owner->save_file(owner->get_current_filename().c_str());
    if (error)
    {
        display_error(error, owner->get_current_filename().c_str());
        return false;
    }
    return true;
}

bool gtk_main_window::save_file_as()
{
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Save File");
    gtk_file_dialog_save(dlg, toplevel, NULL, on_save_file_done, this);
    return true;
}

void gtk_main_window::display_error(const char *error, const char *filename)
{
    char msg[1024];
    snprintf(msg, sizeof(msg), error ? error : "", filename ? filename : "");
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Error");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    if (toplevel)
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(toplevel));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(win), vbox);
    gtk_box_append(GTK_BOX(vbox), gtk_label_new(msg));
    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(btnbox, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(vbox), btnbox);
    GtkWidget *ok_btn = gtk_button_new_with_label("Close");
    gtk_box_append(GTK_BOX(btnbox), ok_btn);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(g_main_loop_quit), loop);
    g_signal_connect_swapped(win, "destroy", G_CALLBACK(g_main_loop_quit), loop);
    gtk_widget_set_visible(win, TRUE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    if (gtk_widget_get_visible(win))
        gtk_window_destroy(GTK_WINDOW(win));
}

GtkWidget *gtk_main_window::create_progress_window()
{
    GtkWidget *tlw = gtk_window_new();
    GtkWidget *pbar = gtk_progress_bar_new();
    gtk_window_set_child(GTK_WINDOW(tlw), pbar);
    gtk_widget_set_visible(pbar, TRUE);
    return tlw;
}

void gtk_main_window::report_progress(float percentage, const std::string &message)
{
    if (percentage < 100)
    {
        if (!progress_window) {
            progress_window = create_progress_window();
            gtk_window_set_modal(GTK_WINDOW(progress_window), TRUE);
            if (toplevel)
                gtk_window_set_transient_for(GTK_WINDOW(progress_window), toplevel);
            gtk_widget_set_visible(progress_window, TRUE);
        }
        GtkWidget *pbar = gtk_window_get_child(GTK_WINDOW(progress_window));
        if (!message.empty())
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pbar), message.c_str());
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), percentage / 100.0);
    }
    else
    {
        if (progress_window) {
            gtk_window_set_modal(GTK_WINDOW(progress_window), FALSE);
            gtk_window_destroy(GTK_WINDOW(progress_window));
            progress_window = NULL;
        }
    }

    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
}

void gtk_main_window::add_condition(const std::string &name)
{
    conditions.insert(name);
}

void gtk_main_window::show_error(const std::string &text)
{
    display_error(text.c_str(), NULL);
}

GtkListStore *gtk_main_window::get_styles()
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    std::vector<calf_utils::direntry> list = calf_utils::list_directory(PKGLIBDIR"styles");
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    for (std::vector<calf_utils::direntry>::iterator i = list.begin(); i != list.end(); i++) {
        string title = i->name;
        // Try gtk.css first (GTK4); first line is "/* Theme Name */"
        std::string cssf = i->full_path + "/gtk.css";
        ifstream cssfile(cssf.c_str());
        if (cssfile.good()) {
            string line;
            getline(cssfile, line);
            // Strip leading "/* " and trailing " */"
            if (line.size() > 6 && line.substr(0, 3) == "/* ") {
                size_t end = line.rfind(" */");
                if (end != string::npos)
                    title = line.substr(3, end - 3);
                else
                    title = line.substr(3);
            }
        } else {
            // Fall back to gtk.rc (GTK2); first line is "#Theme Name"
            std::string rcf = i->full_path + "/gtk.rc";
            ifstream infile(rcf.c_str());
            if (infile.good()) {
                string line;
                getline(infile, line);
                title = line.substr(1);
            }
        }
        gtk_list_store_insert_with_values(store, NULL, -1,
                                  0, title.c_str(),
                                  1, i->name.c_str(),
                                  -1);
    }
G_GNUC_END_IGNORE_DEPRECATIONS
    return store;
}

void gtk_main_window::load_style(std::string path) {
    std::string css_file = path + "/gtk.css";
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, css_file.c_str());
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    images.set_path(path);
}
