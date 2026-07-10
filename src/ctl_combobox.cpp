/* Calf DSP Library
 * Custom controls (line graph, knob).
 * Copyright (C) 2007-2015 Krzysztof Foltman, Torben Hohn, Markus Schmidt
 * and others
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

#include <calf/ctl_combobox.h>

using namespace calf_plugins;
using namespace dsp;

///////////////////////////////////////// combo box ///////////////////////////////////////////////

/* GTK4: GtkDropDown is final and cannot be subclassed.
 * calf_combobox_new returns a plain GtkDropDown with a CSS name for theming.
 * The model (GtkStringList) is set later by combo_box_param_control. */

GtkWidget *
calf_combobox_new()
{
    GtkStringList *strings = gtk_string_list_new(NULL);
    GtkWidget *widget = gtk_drop_down_new(G_LIST_MODEL(strings), NULL);
    gtk_widget_set_name(widget, "CalfCombobox");
    gtk_widget_add_css_class(widget, "CalfCombobox");
    /* A dropdown shouldn't stretch vertically just because its container
     * has leftover space to give it (XML layouts routinely default to
     * expand/fill on every child) - keep it pinned to its natural height. */
    gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
    return widget;
}

GType
calf_combobox_get_type()
{
    return gtk_drop_down_get_type();
}
