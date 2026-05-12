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

#include <calf/ctl_notebook.h>

using namespace calf_plugins;
using namespace dsp;


///////////////////////////////////////// notebook ///////////////////////////////////////////////

/* GTK4: GtkNotebook is a final type and cannot be subclassed.
 * The screw-decoration snapshot override is not available in GTK4.
 * The screw pixbuf is stored as object data for potential future use. */

GtkWidget *
calf_notebook_new()
{
    GtkWidget *nb = gtk_notebook_new();
    gtk_widget_set_name(nb, "CalfNotebook");
    return nb;
}

void
calf_notebook_set_pixbuf(CalfNotebook *self, GdkPixbuf *image)
{
    if (image)
        g_object_set_data_full(G_OBJECT(self), "calf-screw",
                               g_object_ref(image),
                               (GDestroyNotify)g_object_unref);
}
