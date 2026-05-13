/* Calf DSP Library
 * Light emitting diode-like control.
 *
 * Copyright (C) 2008 Krzysztof Foltman
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
#include <calf/ctl_led.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <calf/drawingutils.h>

GtkWidget *
calf_led_new()
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_LED, NULL ));
    return widget;
}

static void
calf_led_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_LED(widget));

    CalfLed *self = CALF_LED(widget);

    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    /* border thickness hardcoded (was widget->style->xthickness/ythickness) */
    int ox = 1;
    int oy = 1;
    int sx = width - ox * 2;
    int sy = height - oy * 2;
    int xc = width / 2;
    int yc = height / 2;
    float r, g, b;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

    if( self->cache_surface == NULL ) {
        // first call or widget resized — rebuild cache
        self->cache_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width, height );
        cairo_t *cache_cr = cairo_create( self->cache_surface );

        /* border-radius default 4, bevel default 0.2 */
        float radius = 4.f, bevel = 0.2f;
        get_bg_color(widget, NULL, &r, &g, &b);
        create_rectangle(cache_cr, 0, 0, width, height, radius);
        cairo_set_source_rgb(cache_cr, r, g, b);
        cairo_fill(cache_cr);
        draw_bevel(cache_cr, 0, 0, width, height, radius, bevel);
        cairo_rectangle(cache_cr, ox, oy, sx, sy);
        cairo_set_source_rgb (cache_cr, 0, 0, 0);
        cairo_fill(cache_cr);

        cairo_destroy( cache_cr );
    }

    cairo_set_source_surface( c, self->cache_surface, 0, 0 );
    cairo_paint( c );

    cairo_pattern_t *pt = cairo_pattern_create_radial(xc, yc, 0, xc, yc, sx > sy ? sx/2 : sy/2);

    float value = self->led_value;

    if(self->led_mode >= 4 && self->led_mode <= 5 && value > 1.f) {
        value = 1.f;
    }
    switch (self->led_mode) {
        default:
        case 0:
            // blue-on/off
            cairo_pattern_add_color_stop_rgb(pt, 0.0, value > 0.f ? 0.2 : 0.0, value > 0.f ? 1.0 : 0.25, value > 0.f ? 1.0 : 0.35);
            cairo_pattern_add_color_stop_rgb(pt, 0.5, value > 0.f ? 0.1 : 0.0, value > 0.f ? 0.6 : 0.15, value > 0.f ? 0.75 : 0.2);
            cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.0,                     value > 0.f ? 0.3 : 0.1,  value > 0.f ? 0.5 : 0.1);
            break;
        case 1:
            // red-on/off
            cairo_pattern_add_color_stop_rgb(pt, 0.0, value > 0.f ? 1.0 : 0.35, value > 0.f ? 0.5 : 0.0, value > 0.f ? 0.2 : 0.0);
            cairo_pattern_add_color_stop_rgb(pt, 0.5, value > 0.f ? 0.80 : 0.2, value > 0.f ? 0.2 : 0.0, value > 0.f ? 0.1 : 0.0);
            cairo_pattern_add_color_stop_rgb(pt, 1.0, value > 0.f ? 0.65 : 0.1, value > 0.f ? 0.1 : 0.0, 0.0);
            break;
        case 2:
        case 4:
            // blue-dynamic (limited)
            cairo_pattern_add_color_stop_rgb(pt, 0.0, value * 0.2, value * 0.75 + 0.25, value * 0.65 + 0.35);
            cairo_pattern_add_color_stop_rgb(pt, 0.5, value * 0.1, value * 0.45 + 0.15, value * 0.55 + 0.2);
            cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.0,         value * 0.2 + 0.1,   value * 0.4 + 0.1);
            break;
        case 3:
        case 5:
            // red-dynamic (limited)
            cairo_pattern_add_color_stop_rgb(pt, 0.0, value * 0.65 + 0.35, value * 0.5, value * 0.2);
            cairo_pattern_add_color_stop_rgb(pt, 0.5, value * 0.6 + 0.2,   value * 0.2, value * 0.1);
            cairo_pattern_add_color_stop_rgb(pt, 1.0, value * 0.66 + 0.1,  value * 0.1, 0.0);
            break;
        case 6:
            // blue-dynamic with red peak at >= 1.f
            if(value < 1.0) {
                cairo_pattern_add_color_stop_rgb(pt, 0.0, value * 0.2, value * 0.75 + 0.25, value * 0.65 + 0.35);
                cairo_pattern_add_color_stop_rgb(pt, 0.5, value * 0.1, value * 0.45 + 0.15, value * 0.55 + 0.2);
                cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.0,         value * 0.2 + 0.1,   value * 0.4 + 0.1);
            } else {
                cairo_pattern_add_color_stop_rgb(pt, 0.0, 1.0,  0.5, 0.2);
                cairo_pattern_add_color_stop_rgb(pt, 0.5, 0.80, 0.2, 0.1);
                cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.66, 0.1, 0.0);
            }
            break;
        case 7:
            // off @ 0.0, blue < 1.0, red @ 1.0
            if(value < 1.f and value > 0.f) {
                cairo_pattern_add_color_stop_rgb(pt, 0.0, 0.2, 1.0, 1.0);
                cairo_pattern_add_color_stop_rgb(pt, 0.5, 0.1, 0.6, 0.75);
                cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.0, 0.3, 0.5);
            } else if(value == 0.f) {
                cairo_pattern_add_color_stop_rgb(pt, 0.0, 0.0, 0.25, 0.35);
                cairo_pattern_add_color_stop_rgb(pt, 0.5, 0.0, 0.15, 0.2);
                cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.0, 0.1,  0.1);
            } else {
                cairo_pattern_add_color_stop_rgb(pt, 0.0, 1.0,  0.5, 0.2);
                cairo_pattern_add_color_stop_rgb(pt, 0.5, 0.80, 0.2, 0.1);
                cairo_pattern_add_color_stop_rgb(pt, 1.0, 0.66, 0.1, 0.0);
            }
            break;
    }

    cairo_rectangle(c, ox + 1, oy + 1, sx - 2, sy - 2);
    cairo_set_source (c, pt);
    cairo_fill_preserve(c);

    /* glass effect default 1.0 */
    float glass = 1.0f;
    if (glass > 0.f) {
        pt = cairo_pattern_create_linear (ox, oy, ox, oy + sy);
        cairo_pattern_add_color_stop_rgba (pt, 0,     1, 1, 1, 0.4 * glass);
        cairo_pattern_add_color_stop_rgba (pt, 0.4,   1, 1, 1, 0.1 * glass);
        cairo_pattern_add_color_stop_rgba (pt, 0.401, 0, 0, 0, 0.0);
        cairo_pattern_add_color_stop_rgba (pt, 1,     0, 0, 0, 0.2 * glass);
        cairo_set_source (c, pt);
        cairo_fill(c);
        cairo_pattern_destroy(pt);
    }

    cairo_destroy(c);
}

static void
calf_led_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                  int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
    CalfLed *self = CALF_LED(widget);
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimum = *natural = self->size ? 24 : 19;
    else
        *minimum = *natural = self->size ? 18 : 14;
    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

static void
calf_led_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    g_assert(CALF_IS_LED(widget));
    CalfLed *led = CALF_LED(widget);

    GtkWidgetClass *parent_class = (GtkWidgetClass *) g_type_class_peek_parent( CALF_LED_GET_CLASS( led ) );
    parent_class->size_allocate( widget, width, height, baseline );

    if( led->cache_surface )
        cairo_surface_destroy( led->cache_surface );
    led->cache_surface = NULL;
}

static void
calf_led_class_init (CalfLedClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot      = calf_led_snapshot;
    widget_class->measure       = calf_led_measure;
    widget_class->size_allocate = calf_led_size_allocate;
}

static void
calf_led_init (CalfLed *self)
{
    self->led_mode = 0;
    self->size = 0;
    self->led_value = 0.f;
    self->cache_surface = NULL;
    gtk_widget_set_size_request(GTK_WIDGET(self),
                                self->size ? 24 : 19,
                                self->size ? 18 : 14);
}

void calf_led_set_value(CalfLed *led, float value)
{
    if (value != led->led_value)
    {
        float old_value = led->led_value;
        led->led_value = value;
        if (led->led_mode >= 2 || (old_value > 0) != (value > 0))
        {
            GtkWidget *widget = GTK_WIDGET (led);
            if (gtk_widget_get_realized(widget))
                gtk_widget_queue_draw (widget);
        }
    }
}

gboolean calf_led_get_value(CalfLed *led)
{
    return led->led_value;
}

GType
calf_led_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfLedClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_led_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfLed),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_led_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfLed";
            if (g_type_from_name(name)) {
                continue;
            }
            type = g_type_register_static(GTK_TYPE_DRAWING_AREA,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            break;
        }
    }
    return type;
}
