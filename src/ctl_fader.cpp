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

#include <calf/ctl_fader.h>

using namespace calf_plugins;
using namespace dsp;

static void calf_cairo_set_source_pixbuf(cairo_t *cr, GdkPixbuf *pb, double px, double py)
{
    int w          = gdk_pixbuf_get_width(pb);
    int h          = gdk_pixbuf_get_height(pb);
    int nc         = gdk_pixbuf_get_n_channels(pb);
    int src_stride = gdk_pixbuf_get_rowstride(pb);
    const guchar *src = gdk_pixbuf_get_pixels(pb);
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    guchar *dst       = cairo_image_surface_get_data(surf);
    int dst_stride    = cairo_image_surface_get_stride(surf);
    for (int row = 0; row < h; row++) {
        const guchar *s = src + row * src_stride;
        guint32 *d = (guint32 *)(dst + row * dst_stride);
        for (int col = 0; col < w; col++, s += nc) {
            guint8 r = s[0], g = s[1], b = s[2];
            guint8 a = (nc == 4) ? s[3] : 255u;
            *d++ = ((guint32)a << 24) | ((guint32)(r * a / 255) << 16)
                 | ((guint32)(g * a / 255) << 8)  |  (guint32)(b * a / 255);
        }
    }
    cairo_surface_mark_dirty(surf);
    cairo_set_source_surface(cr, surf, px, py);
    cairo_surface_destroy(surf);
}


///////////////////////////////////////// fader ///////////////////////////////////////////////


void calf_fader_set_layout(GtkWidget *widget)
{
    GtkRange *range   = GTK_RANGE(widget);
    CalfFader *fader  = CALF_FADER(widget);
    CalfFaderLayout l = fader->layout;
    GdkRectangle t;
    gint sstart, send;

    gtk_range_get_range_rect(range, &t);
    gtk_range_get_slider_range(range, &sstart, &send);

    int hor = fader->horizontal;
    int slength = 17;

    // widget layout
    l.x = t.x;
    l.y = t.y;
    l.w = t.width;
    l.h = t.height;

    // image layout
    l.iw = gdk_pixbuf_get_width(fader->image);
    l.ih = gdk_pixbuf_get_height(fader->image);

    // first screw layout
    l.s1w  = hor  ? slength : gdk_pixbuf_get_width(fader->image);
    l.s1h  = !hor ? slength : gdk_pixbuf_get_height(fader->image);
    l.s1x1 = 0;
    l.s1y1 = 0;
    l.s1x2 = l.x;
    l.s1y2 = l.y;

    // second screw layout
    l.s2w  = l.s1w;
    l.s2h  = l.s1h;
    l.s2x1 = hor  ? l.iw - 3 * l.s2w : 0;
    l.s2y1 = !hor ? l.ih - 3 * l.s2h : 0;
    l.s2x2 = hor  ? l.w - l.s2w + l.x : l.x;
    l.s2y2 = !hor ? l.h - l.s2h + l.y : l.y;

    // trough 1 layout
    l.t1w  = l.s1w;
    l.t1h  = l.s1h;
    l.t1x1 = hor  ? l.iw - 2 * l.s2w : 0;
    l.t1y1 = !hor ? l.ih - 2 * l.s2h : 0;

    // trough 2 layout
    l.t2w  = l.s1w;
    l.t2h  = l.s1h;
    l.t2x1 = hor  ? l.iw - l.s2w : 0;
    l.t2y1 = !hor ? l.ih - l.s2h : 0;

    // slit layout
    l.sw  = hor  ? l.iw - 4 * l.s1w : l.ih;
    l.sh  = !hor ? l.ih - 4 * l.s1h : l.iw;
    l.sx1 = hor  ? l.s1w : 0;
    l.sy1 = !hor ? l.s1h : 0;
    l.sx2 = hor  ? l.s1w + l.x : l.x;
    l.sy2 = !hor ? l.s1h + l.y : l.y;
    l.sw2 = hor  ? l.w - 2 * l.s1w : l.iw;
    l.sh2 = !hor ? l.h - 2 * l.s1h : l.ih;

    fader->layout = l;
}

GtkWidget *
calf_fader_new(const int horiz, const int size, const double min, const double max, const double step)
{
    GtkAdjustment *adj;
    gint digits;

    adj = gtk_adjustment_new(min, min, max, step, 10 * step, 0);

    if (fabs(step) >= 1.0 || step == 0.0)
        digits = 0;
    else
        digits = std::min(5, abs((gint) floor(log10(fabs(step)))));

    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_FADER, NULL ));
    CalfFader *self = CALF_FADER(widget);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(widget),
        horiz ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
    gtk_range_set_adjustment(GTK_RANGE(widget), GTK_ADJUSTMENT(adj));
    gtk_scale_set_digits(GTK_SCALE(widget), digits);

    self->size = size;
    self->horizontal = horiz;
    self->hover = 0;

    return widget;
}

static bool calf_fader_hover(GtkWidget *widget, int mx, int my)
{
    CalfFader *fader  = CALF_FADER(widget);

    GtkRange *range   = GTK_RANGE(widget);
    GdkRectangle trough;
    gint sstart, send;
    gtk_range_get_range_rect(range, &trough);
    gtk_range_get_slider_range(range, &sstart, &send);

    int hor = fader->horizontal;

    int x1 = hor  ? sstart : trough.x;
    int x2 = hor  ? send : trough.x + trough.width;
    int y1 = !hor ? sstart : trough.y;
    int y2 = !hor ? send : trough.y + trough.height;

    return mx >= x1 and mx <= x2 and my >= y1 and my <= y2;
}

static void calf_fader_check_hover_change(GtkWidget *widget, int mx, int my)
{
    CalfFader *fader = CALF_FADER(widget);
    bool hover = calf_fader_hover(widget, mx, my);
    if (hover != fader->hover)
        gtk_widget_queue_draw(widget);
    fader->hover = hover;
}

static void
calf_fader_motion (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    calf_fader_check_hover_change(widget, (int)x, (int)y);
}

static void
calf_fader_enter (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    calf_fader_check_hover_change(widget, (int)x, (int)y);
}

static void
calf_fader_leave (GtkEventControllerMotion *controller, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    CALF_FADER(widget)->hover = false;
    gtk_widget_queue_draw(widget);
}

static void
calf_fader_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    GtkWidgetClass *parent_class = (GtkWidgetClass *)g_type_class_peek(GTK_TYPE_SCALE);
    if (parent_class->size_allocate)
        parent_class->size_allocate(widget, width, height, baseline);
    calf_fader_set_layout(widget);
    /* Without this, the fader can remain unpainted after its very first
     * layout pass - confirmed by opening GTK Inspector (which forces a
     * style/redraw pass across the whole display) making an otherwise
     * invisible fader appear correctly. Explicitly request a repaint any
     * time the layout the snapshot depends on is recomputed. */
    gtk_widget_queue_draw(widget);
}

static void
calf_fader_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
    CalfFader *fader = CALF_FADER(widget);
    int sz = 40;
    if (fader->image) {
        /* A horizontal fader's width should track the pixbuf's own width
         * (its long/travel axis) and its height the pixbuf's height (its
         * thickness) - and vice versa for a vertical fader. This was
         * swapped, making every hscale/vscale report a wildly wrong size
         * (e.g. a 238x36 horizontal slider image reporting a 36-wide,
         * 238-tall widget instead of 238 wide, 36 tall). */
        sz = (orientation == GTK_ORIENTATION_HORIZONTAL)
             ? gdk_pixbuf_get_width(fader->image)
             : gdk_pixbuf_get_height(fader->image);
    }
    *minimum = sz;
    *natural = sz;
    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline)  *natural_baseline  = -1;
}

static void
calf_fader_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_FADER(widget));
    if (gtk_widget_is_drawable(widget)) {

        GtkScale  *scale  = GTK_SCALE(widget);
        GtkRange  *range  = GTK_RANGE(widget);
        CalfFader *fader  = CALF_FADER(widget);
        CalfFaderLayout l = fader->layout;
        int horiz         = fader->horizontal;

        int width  = gtk_widget_get_width(widget);
        int height = gtk_widget_get_height(widget);
        graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
        cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

        cairo_save(c);
        cairo_rectangle(c, l.x, l.y, l.w, l.h);
        cairo_clip(c);

        // position
        GtkAdjustment *adj = gtk_range_get_adjustment(range);
        double adj_upper = gtk_adjustment_get_upper(adj);
        double adj_lower = gtk_adjustment_get_lower(adj);
        double adj_value = gtk_adjustment_get_value(adj);

        double r0  = adj_upper - adj_lower;
        double v0  = adj_value - adj_lower;
        if ((horiz and gtk_range_get_inverted(range))
        or (!horiz and gtk_range_get_inverted(range)))
            v0 = -v0 + r0;
        int vp = v0 / r0 * (horiz ? l.w - l.s1w : l.h - l.s1h);

        l.t1x2 = l.t2x2 = horiz  ? l.x + vp : l.x;
        l.t1y2 = l.t2y2 = !horiz ? l.y + vp : l.y;

        GdkPixbuf *i = fader->image;

        // screw 1
        cairo_rectangle(c, l.s1x2, l.s1y2, l.s1w, l.s1h);
        calf_cairo_set_source_pixbuf(c, i, l.s1x2 - l.s1x1, l.s1y2 - l.s1y1);
        cairo_fill(c);

        // screw 2
        cairo_rectangle(c, l.s2x2, l.s2y2, l.s2w, l.s2h);
        calf_cairo_set_source_pixbuf(c, i, l.s2x2 - l.s2x1, l.s2y2 - l.s2y1);
        cairo_fill(c);

        // trough
        if (horiz) {
            int x = l.sx2;
            while (x < l.sx2 + l.sw2) {
                cairo_rectangle(c, x, l.sy2, std::min(l.sx2 + l.sw2 - x, l.sw), l.sh2);
                calf_cairo_set_source_pixbuf(c, i, x - l.sx1, l.sy2 - l.sy1);
                cairo_fill(c);
                x += l.sw;
            }
        } else {
            int y = l.sy2;
            while (y < l.sy2 + l.sh2) {
                cairo_rectangle(c, l.sx2, y, l.sw2, std::min(l.sy2 + l.sh2 - y, l.sh));
                calf_cairo_set_source_pixbuf(c, i, l.sx2 - l.sx1, y - l.sy1);
                cairo_fill(c);
                y += l.sh;
            }
        }

        // slider
        if (fader->hover or (gtk_widget_get_state_flags(widget) & GTK_STATE_FLAG_ACTIVE) != 0) {
            cairo_rectangle(c, l.t1x2, l.t1y2, l.t1w, l.t1h);
            calf_cairo_set_source_pixbuf(c, i, l.t1x2 - l.t1x1, l.t1y2 - l.t1y1);
        } else {
            cairo_rectangle(c, l.t2x2, l.t2y2, l.t2w, l.t2h);
            calf_cairo_set_source_pixbuf(c, i, l.t2x2 - l.t2x1, l.t2y2 - l.t2y1);
        }
        cairo_fill(c);

        // the trough/slider clip above only covers the range rect; the value
        // label (drawn by GtkScale below/beside it via draw-value) sits
        // outside that rect and must not be clipped away with it.
        cairo_restore(c);

        // draw value label
        if (gtk_scale_get_draw_value(scale)) {
            PangoLayout *layout = gtk_scale_get_layout(scale);
            if (layout) {
                int lx, ly;
                gtk_scale_get_layout_offsets(scale, &lx, &ly);
                cairo_move_to(c, lx, ly);
                pango_cairo_show_layout(c, layout);
            }
        }

        cairo_destroy(c);
    }
}

void
calf_fader_set_pixbuf (CalfFader *self, GdkPixbuf *image)
{
    GtkWidget *widget = GTK_WIDGET(self);
    self->image = image;
    gtk_widget_queue_resize(widget);
}

static void
calf_fader_class_init (CalfFaderClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_css_name(widget_class, "calffader");
    widget_class->snapshot       = calf_fader_snapshot;
    widget_class->size_allocate  = calf_fader_size_allocate;
    widget_class->measure        = calf_fader_measure;
}

static void
calf_fader_init (CalfFader *self)
{
    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_size_request(widget, 40, 40);

    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion", G_CALLBACK(calf_fader_motion), GTK_WIDGET(self));
    g_signal_connect(motion, "enter",  G_CALLBACK(calf_fader_enter),  GTK_WIDGET(self));
    g_signal_connect(motion, "leave",  G_CALLBACK(calf_fader_leave),  GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(motion));

    /* size_allocate and measure are handled via virtual function overrides */
}

GType
calf_fader_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfFaderClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_fader_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfFader),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_fader_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfFader";
            //char *name = g_strdup_printf("CalfFader%u%d",
                //((unsigned int)(intptr_t)calf_fader_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_SCALE,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
