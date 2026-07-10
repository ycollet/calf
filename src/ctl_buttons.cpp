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

#include <calf/ctl_buttons.h>

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


///////////////////////////////////////// toggle ///////////////////////////////////////////////


static void
calf_toggle_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_TOGGLE(widget));
    CalfToggle *self = CALF_TOGGLE(widget);
    if (!self->toggle_image)
        return;
    float off = floor(.5 + gtk_range_get_value(GTK_RANGE(widget)));
    float pw  = gdk_pixbuf_get_width(self->toggle_image);
    float ph  = gdk_pixbuf_get_height(self->toggle_image);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    float wcx = 0 + width / 2;
    float wcy = 0 + height / 2;
    float pcx = pw / 2;
    float pcy = ph / 4;
    float sy = off * ph / 2;
    float x = wcx - pcx;
    float y = wcy - pcy;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)gtk_widget_get_width(widget), (float)gtk_widget_get_height(widget));
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);
    calf_cairo_set_source_pixbuf(c, self->toggle_image, x - 0, y - sy);
    cairo_rectangle(c, x, y, pw, ph / 2);
    cairo_fill(c);
    cairo_destroy(c);
}

static void
calf_toggle_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                     int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
    g_assert(CALF_IS_TOGGLE(widget));
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = *natural = 1;
    } else {
        *minimum = *natural = 1;
    }
    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

static void
calf_toggle_button_press (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget *)user_data;
    g_assert(CALF_IS_TOGGLE(widget));
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(widget));
    if (gtk_range_get_value(GTK_RANGE(widget)) == gtk_adjustment_get_lower(adj))
    {
        gtk_range_set_value(GTK_RANGE(widget), gtk_adjustment_get_upper(adj));
    } else {
        gtk_range_set_value(GTK_RANGE(widget), gtk_adjustment_get_lower(adj));
    }
}

static gboolean
calf_toggle_key_press (GtkEventControllerKey *ctrl, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    switch(keyval)
    {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_space:
            calf_toggle_button_press(NULL, 1, 0.0, 0.0, widget);
            return TRUE;
    }
    return FALSE;
}

static void
calf_toggle_class_init (CalfToggleClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_toggle_snapshot;
    widget_class->measure = calf_toggle_measure;
}

static void
calf_toggle_init (CalfToggle *self)
{
    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_focusable(widget, TRUE);
    gtk_widget_set_size_request(widget, 30, 20);
    gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(widget, GTK_ALIGN_CENTER);
    self->size = 1;

    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(calf_toggle_button_press), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(calf_toggle_key_press), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), key_ctrl);
}

void
calf_toggle_set_size (CalfToggle *self, int size)
{
    char name[128];
    GtkWidget *widget = GTK_WIDGET(self);
    self->size = size;
    snprintf(name, sizeof(name), "%s_%d\n", gtk_widget_get_name(widget), size);
    gtk_widget_set_name(widget, name);
    gtk_widget_queue_resize(widget);
}
void
calf_toggle_set_pixbuf (CalfToggle *self, GdkPixbuf *pixbuf)
{
    GtkWidget *widget = GTK_WIDGET(self);
    self->toggle_image = pixbuf;
    gtk_widget_queue_resize(widget);
}

GtkWidget *
calf_toggle_new()
{
    GtkAdjustment *adj = (GtkAdjustment *)gtk_adjustment_new(0, 0, 1, 1, 0, 0);
    return calf_toggle_new_with_adjustment(adj);
}

static gboolean calf_toggle_value_changed(gpointer obj)
{
    GtkWidget *widget = (GtkWidget *)obj;
    CalfToggle *self = CALF_TOGGLE(widget);
    float sx = self->size ? self->size : 1.f / 3.f * 2.f;
    float sy = self->size ? self->size : 1;
    gtk_widget_queue_draw(widget);
    return FALSE;
}

GtkWidget *calf_toggle_new_with_adjustment(GtkAdjustment *_adjustment)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_TOGGLE, NULL ));
    if (widget) {
        gtk_range_set_adjustment(GTK_RANGE(widget), _adjustment);
        g_signal_connect(widget, "value-changed", G_CALLBACK(calf_toggle_value_changed), widget);
    }
    return widget;
}

GType
calf_toggle_get_type (void)
{
    static GType type = 0;
    if (!type) {

        static const GTypeInfo type_info = {
            sizeof(CalfToggleClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_toggle_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfToggle),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_toggle_init
        };

        for (int i = 0; ; i++) {
            //char *name = g_strdup_printf("CalfToggle%u%d",
                //((unsigned int)(intptr_t)calf_toggle_class_init) >> 16, i);
            const char *name = "CalfToggle";
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static( GTK_TYPE_RANGE,
                                           name,
                                           &type_info,
                                           (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}


///////////////////////////////////////// button ///////////////////////////////////////////////

GtkWidget *
calf_button_new(const gchar *label)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_BUTTON, NULL ));
    gtk_button_set_label(GTK_BUTTON(widget), label);
    return widget;
}
static void
calf_button_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_BUTTON(widget) || CALF_IS_TOGGLE_BUTTON(widget) || CALF_IS_RADIO_BUTTON(widget));

    if (gtk_widget_is_drawable (widget)) {

        GtkWidget *child     = gtk_widget_get_first_child(widget);
        int width  = gtk_widget_get_width(widget);
        int height = gtk_widget_get_height(widget);

        graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)gtk_widget_get_width(widget), (float)gtk_widget_get_height(widget));
        cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

        int x  = 0;
        int y  = 0;
        int sx = width;
        int sy = height;
        int ox = 1;
        int oy = 1;
        int bx = x + ox + 1;
        int by = y + oy + 1;
        int bw = sx - 2 * ox - 2;
        int bh = sy - 2 * oy - 2;

        float r, g, b;
        float radius = 4.f;
        float bevel = 0.2f;
        float inset = 0.2f;
        GtkBorder *border = NULL;

        cairo_rectangle(c, x, y, sx, sy);
        cairo_clip(c);

        get_bg_color(widget, NULL, &r, &g, &b);

        border = gtk_border_new();
        border->left = border->right = border->top = border->bottom = 4;

        // inset
        draw_bevel(c, x, y, sx, sy, radius, inset*-1);

        // space
        create_rectangle(c, x + ox, y + oy, sx - ox * 2, sy - oy * 2, std::max(0.f, radius - ox));
        cairo_set_source_rgba(c, 0, 0, 0, 0.6);
        cairo_fill(c);

        // button
        create_rectangle(c, bx, by, bw, bh, std::max(0.f, radius - ox - 1));
        cairo_set_source_rgb(c, r, g, b);
        cairo_fill(c);
        draw_bevel(c, bx, by, bw, bh, std::max(0.f, radius - ox - 1), bevel);

        // pin — small state indicator at right edge of toggle/radio buttons
        if (CALF_IS_TOGGLE_BUTTON(widget) or CALF_IS_RADIO_BUTTON(widget)) {
            int pinh = 4;
            int pinw = 8;
            get_text_color(widget, NULL, &r, &g, &b);
            float a;
            if ((gtk_widget_get_state_flags(widget) & GTK_STATE_FLAG_PRELIGHT) != 0)
                a = 1.0f;
            else if ((gtk_widget_get_state_flags(widget) & GTK_STATE_FLAG_ACTIVE) != 0)
                a = 0.2f;
            else
                a = 0.2f;
            cairo_rectangle(c, x + sx - 14, y + sy / 2 - pinh / 2, pinw, pinh);
            cairo_set_source_rgba(c, r, g, b, a);
            cairo_fill(c);
        }

        gtk_border_free(border);
        cairo_destroy(c);

        if (child)
            gtk_widget_snapshot_child(widget, child, snapshot);
    }
}

static void
calf_button_class_init (CalfButtonClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_button_snapshot;
}

static void
calf_button_init (CalfButton *self)
{
    gtk_widget_set_size_request(GTK_WIDGET(self), 40, 20);
    gtk_widget_set_valign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(GTK_WIDGET(self), "calf-button");
}

GType
calf_button_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfButtonClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_button_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfButton),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_button_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfButton";
            //char *name = g_strdup_printf("CalfButton%u%d",
                //((unsigned int)(intptr_t)calf_button_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_BUTTON,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}


///////////////////////////////////////// toggle button ///////////////////////////////////////////////

GtkWidget *
calf_toggle_button_new(const gchar *label)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_TOGGLE_BUTTON, NULL ));
    gtk_button_set_label(GTK_BUTTON(widget), label);
    return widget;
}

static void
calf_toggle_button_class_init (CalfToggleButtonClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_button_snapshot;
}

static void
calf_toggle_button_init (CalfToggleButton *self)
{
    gtk_widget_set_size_request(GTK_WIDGET(self), 60, 24);
    gtk_widget_add_css_class(GTK_WIDGET(self), "calf-toggle-button");
}

GType
calf_toggle_button_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfToggleButtonClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_toggle_button_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfToggleButton),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_toggle_button_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfToggleButton";
            //char *name = g_strdup_printf("CalfToggleButton%u%d",
                //((unsigned int)(intptr_t)calf_toggle_button_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_TOGGLE_BUTTON,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}

///////////////////////////////////////// radio button ///////////////////////////////////////////////

GtkWidget *
calf_radio_button_new(const gchar *label)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_RADIO_BUTTON, NULL ));
    gtk_button_set_label(GTK_BUTTON(widget), label);
    return widget;
}

static void
calf_radio_button_class_init (CalfRadioButtonClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_button_snapshot;
}

static void
calf_radio_button_init (CalfRadioButton *self)
{
    gtk_widget_set_size_request(GTK_WIDGET(self), 40, 20);
    gtk_widget_set_valign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(GTK_WIDGET(self), "calf-toggle-button");
}

GType
calf_radio_button_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfRadioButtonClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_radio_button_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfRadioButton),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_radio_button_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfRadioButton";
            //char *name = g_strdup_printf("CalfRadioButton%u%d",
                //((unsigned int)(intptr_t)calf_radio_button_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_CHECK_BUTTON,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}

///////////////////////////////////////// tap button ///////////////////////////////////////////////

GtkWidget *
calf_tap_button_new()
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_TAP_BUTTON, NULL ));
    return widget;
}

static void
calf_tap_button_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_TAP_BUTTON(widget));
    CalfTapButton *self = CALF_TAP_BUTTON(widget);

    if (!self->image[self->state])
        return;

    int img_width  = gdk_pixbuf_get_width(self->image[0]);
    int img_height = gdk_pixbuf_get_height(self->image[0]);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    int x = 0 + width / 2 - img_width / 2;
    int y = 0 + height / 2 - img_height / 2;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)gtk_widget_get_width(widget), (float)gtk_widget_get_height(widget));
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);
    calf_cairo_set_source_pixbuf(c, self->image[self->state], x - 0, y - 0);
    cairo_rectangle(c, x, y, img_width, img_height);
    cairo_fill(c);
    cairo_destroy(c);
}

void
calf_tap_button_set_pixbufs (CalfTapButton *self, GdkPixbuf *image1, GdkPixbuf *image2, GdkPixbuf *image3)
{
    GtkWidget *widget = GTK_WIDGET(self);
    self->image[0] = image1;
    self->image[1] = image2;
    self->image[2] = image3;
    gtk_widget_set_size_request(widget,
                                gdk_pixbuf_get_width(self->image[0]),
                                gdk_pixbuf_get_height(self->image[0]));
    gtk_widget_queue_resize(widget);
}

static void
calf_tap_button_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                         int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
    g_assert(CALF_IS_TAP_BUTTON(widget));
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = *natural = 70;
    } else {
        *minimum = *natural = 70;
    }
    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

static void
calf_tap_button_class_init (CalfTapButtonClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_tap_button_snapshot;
    widget_class->measure = calf_tap_button_measure;
}
static void
calf_tap_button_init (CalfTapButton *self)
{
    self->state = 0;
}

GType
calf_tap_button_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfTapButtonClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_tap_button_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfTapButton),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_tap_button_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfTapButton";
            //char *name = g_strdup_printf("CalfTapButton%u%d",
                //((unsigned int)(intptr_t)calf_tap_button_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_BUTTON,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
