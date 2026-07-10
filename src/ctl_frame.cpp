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

#include <calf/ctl_frame.h>

using namespace calf_plugins;
using namespace dsp;


///////////////////////////////////////// frame ///////////////////////////////////////////////


GtkWidget *
calf_frame_new(const char *label)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_FRAME, NULL ));
    /* The label is drawn manually in calf_frame_snapshot(), so it is stashed
     * as object data instead of via gtk_frame_set_label() - the latter makes
     * GtkFrame create its own internal GtkLabel child, which the chained-up
     * parent snapshot below then renders a second time, slightly offset. */
    g_object_set_data_full(G_OBJECT(widget), "calf-frame-label",
                            g_strdup(label), g_free);
    return widget;
}
static void
calf_frame_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_FRAME(widget));
    if (gtk_widget_is_drawable (widget)) {

        int width  = gtk_widget_get_width(widget);
        int height = gtk_widget_get_height(widget);
        graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
        cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);
        cairo_text_extents_t extents;

        int ox = 0;
        int oy = 0;
        int sx = width;
        int sy = height;

        float rad = 4.f;

        double pad  = 1;
        double txp  = 4;
        double m    = 0.5;
        double size = 10;

        float r, g, b;

        cairo_rectangle(c, ox, oy, sx, sy);
        cairo_clip(c);


        const gchar *lab = (const gchar *)g_object_get_data(G_OBJECT(widget), "calf-frame-label");
        if (!lab) lab = "";

        cairo_select_font_face(c, "Sans",
              CAIRO_FONT_SLANT_NORMAL,
              CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(c, size);

        cairo_text_extents(c, lab, &extents);

        double lw = extents.width + txp * 2.;

        cairo_set_line_width(c, 1.);

        cairo_move_to(c, ox + rad + txp + m, oy + size - 2 + m);
        get_text_color(widget, NULL, &r, &g, &b);
        cairo_set_source_rgb(c, r, g, b);
        cairo_show_text(c, lab);
        get_fg_color(widget, NULL, &r, &g, &b);
        cairo_set_source_rgb(c, r, g, b);

        // top left
        cairo_move_to(c, ox + m, oy + pad + rad + m);
        cairo_arc (c, ox + rad + m, oy + rad + pad + m, rad, 1 * M_PI, 1.5 * M_PI);
        // top
        cairo_move_to(c, ox + rad + lw + m, oy + pad + m);
        cairo_line_to(c, ox + sx - rad - m, oy + pad + m);
        // top right
        cairo_arc (c, ox + sx - rad - m, oy + rad + pad + m, rad, 1.5 * M_PI, 2 * M_PI);
        // right
        cairo_line_to(c, ox + sx - m, oy + sy - rad - m);
        // bottom right
        cairo_arc (c, ox + sx - rad - m, oy + sy - rad - m, rad, 0 * M_PI, 0.5 * M_PI);
        // bottom
        cairo_line_to(c, ox + rad + m, oy + sy - m);
        // bottom left
        cairo_arc (c, ox + rad + m, oy + sy - rad - m, rad, 0.5 * M_PI, 1 * M_PI);
        // left
        cairo_line_to(c, ox + m, oy + rad + pad + m);
        cairo_stroke(c);

        cairo_destroy(c);
    }

    // Let the parent GtkFrame snapshot handle the child widget
    GtkWidgetClass *parent_class = (GtkWidgetClass*)g_type_class_peek_parent(CALF_FRAME_GET_CLASS(CALF_FRAME(widget)));
    if (parent_class->snapshot)
        parent_class->snapshot(widget, snapshot);
}

static void
calf_frame_class_init (CalfFrameClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_frame_snapshot;
}

static void
calf_frame_init (CalfFrame *self)
{
    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_size_request(widget, 40, 40);
}

GType
calf_frame_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfFrameClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_frame_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfFrame),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_frame_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfFrame";
            //char *name = g_strdup_printf("CalfFrame%u%d",
                //((unsigned int)(intptr_t)calf_frame_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_FRAME,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
