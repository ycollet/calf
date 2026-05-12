/* Calf DSP Library
 * A combo box widget
 *
 * Copyright (C) 2008-2015 Krzysztof Foltman, Torben Hohn, Markus
 * Schmidt and others
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
 * Boston, MA 02111-1307, USA.
 */

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifndef __CALF_CTL_COMBOBOX
#define __CALF_CTL_COMBOBOX

#include <gtk/gtk.h>
#include <calf/drawingutils.h>
#include <calf/gui.h>

G_BEGIN_DECLS

/// COMBOBOX ///////////////////////////////////////////////////////////

/* GTK4: GtkDropDown is a final type and cannot be subclassed.
 * CalfCombobox is now just a thin alias for GtkDropDown.
 * The custom drawing (display_background, arrow) is delegated to CSS. */

#define CALF_TYPE_COMBOBOX          (gtk_drop_down_get_type())
#define CALF_COMBOBOX(obj)          GTK_DROP_DOWN(obj)
#define CALF_IS_COMBOBOX(obj)       GTK_IS_DROP_DOWN(obj)

typedef GtkDropDown CalfCombobox;

/* No-op: GtkDropDown renders its own arrow; the pixbuf arrow is no longer used. */
static inline void calf_combobox_set_arrow(CalfCombobox * /*self*/, GdkPixbuf * /*arrow*/) {}

extern GtkWidget *calf_combobox_new();
extern GType      calf_combobox_get_type();

G_END_DECLS

#endif
