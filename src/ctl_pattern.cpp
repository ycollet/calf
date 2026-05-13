/* Calf DSP Library
 * Custom controls (line graph, knob).
 * Copyright (C) 2007-2010 Krzysztof Foltman, Torben Hohn, Markus Schmidt
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

#include "config.h"
#include <calf/drawingutils.h>
#include <calf/ctl_pattern.h>
#include <gdk/gdk.h>
#include <stdint.h>
#include <algorithm>

#define RGBAtoINT(r, g, b, a) ((uint32_t)(r * 255) << 24) + ((uint32_t)(g * 255) << 16) + ((uint32_t)(b * 255) << 8) + (uint32_t)(a * 255)
#define INTtoR(color) (float)((color & 0xff000000) >> 24) / 255.f
#define INTtoG(color) (float)((color & 0x00ff0000) >> 16) / 255.f
#define INTtoB(color) (float)((color & 0x0000ff00) >>  8) / 255.f
#define INTtoA(color) (float)((color & 0x000000ff) >>  0) / 255.f

using namespace std;
using namespace calf_plugins;

static GdkRectangle calf_pattern_handle_rect (CalfPattern *p, int bar, int beat, double value)
{
    g_assert(CALF_IS_PATTERN(p));

    float top    = round(p->pad_y + p->border_v + p->mbars);
    float bottom = round(top + p->beat_height);
    float height = round(p->beat_height * value);
    float max    = bottom - height;

    // move to bars left edge
    float x = p->pad_x + p->border_h + p->mbars + bar * p->bar_width;
    // move to beats left edge
    x += (p->beat_width + p->minner) * beat;
    x = floor(x);

    GdkRectangle rect;
    rect.x = (int)x;
    rect.y = (int)max;
    rect.width = (int)p->beat_width;
    rect.height = (int)height;
    return rect;
}

static void calf_pattern_draw_background (GtkWidget *wi, cairo_t *cr)
{
    g_assert(CALF_IS_PATTERN(wi));
    CalfPattern *p = CALF_PATTERN(wi);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 8);

    cairo_text_extents_t tx, tx2;
    cairo_text_extents(cr, "Beats", &tx);

    p->border_h = tx.width + p->border;
    p->border_v = tx.height + p->border;
    p->bar_width = (p->size_x - p->border_h - p->mbars) / p->bars;
    p->beat_width = floor((p->bar_width - p->mbars - (p->beats - 1) * p->minner) / p->beats);
    p->beat_height = p->size_y - 2 * p->border_v - 2 * p->mbars;

    float r, g, b;
    get_text_color(wi, NULL, &r, &g, &b);
    cairo_set_source_rgb(cr, r, g, b);

    float _x = p->pad_x + p->border;
    float _y = p->pad_y + p->border - tx.y_bearing;

    //cairo_move_to(cr, _x, _y);
    //cairo_show_text(cr, "Bars");

    //cairo_move_to(cr, _x, p->height - p->pad_y - p->border + tx.height + tx.y_bearing);
    //cairo_show_text(cr, "Beats");

    cairo_move_to(cr, _x, _y + p->border + tx.height + p->mbars);
    cairo_show_text(cr, "100%");

    cairo_move_to(cr, _x, p->height / 2 - tx.height / 2 - tx.y_bearing);
    cairo_show_text(cr, "50%");

    cairo_move_to(cr, _x, p->height - p->pad_y - p->border * 2 - tx.height * 2 - tx.y_bearing - p->mbars);
    cairo_show_text(cr, "0%");

    for (int i = 0; i < p->bars; i++) {
        _x = p->pad_x + p->border_h + p->mbars + i * p->bar_width;
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        cairo_set_font_size(cr, 8);
        cairo_text_extents(cr, num, &tx2);
        get_text_color(wi, NULL, &r, &g, &b);
        cairo_set_source_rgb(cr, r, g, b);
        cairo_move_to(cr, _x + (p->bar_width - p->mbars) / 2 - tx2.width / 2 - 1, _y);
        cairo_show_text(cr, num);
        for (int j = 0; j < p->beats; j++) {
            calf_pattern_draw_handle(wi, cr, i, j, 0, 0, 1, 0.1, false);
            get_text_color(wi, NULL, &r, &g, &b);
            cairo_set_source_rgb(cr, r, g, b);
            snprintf(num, sizeof(num), "%d", j + 1);
            cairo_set_font_size(cr, p->bars * p->beats * 7 > p->width ? 7 : 8);
            cairo_text_extents(cr, num, &tx2);
            cairo_move_to(cr, _x + (p->beat_width + p->minner) * j + p->beat_width / 2 - tx2.width / 2 - 1,
                              p->height - p->pad_y - p->border + tx2.height + tx2.y_bearing);
            cairo_show_text(cr, num);
        }
    }
}

void calf_pattern_draw_handle (GtkWidget *wi, cairo_t *cr, int bar, int beat, int x, int y, double value, float alpha, bool outline)
{
    g_assert(CALF_IS_PATTERN(wi));
    CalfPattern *p = CALF_PATTERN(wi);

    GdkRectangle rect = calf_pattern_handle_rect(p, bar, beat, value);
    // move to lower edge
    int bottom = y + rect.y + rect.height;
    int _y = bottom;

    int c = 0;
    float r, g, b;
    get_fg_color(wi, NULL, &r, &g, &b);
    cairo_set_source_rgba(cr, r, g, b, alpha);
    while (c++, _y > rect.y + y) {
        // loop over segments, begin at the bottom
        int next = std::max(y + rect.y, (int)round(bottom - p->beat_height / 10.f * c));
        cairo_rectangle(cr, x + rect.x, _y, rect.width, next - _y + 1);
        cairo_fill(cr);
        _y = next;
    }
}

static void
calf_pattern_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);

    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

    if (p->force_redraw) {
        p->pad_x  = 1; /* hardcoded xthickness */
        p->pad_y  = 1; /* hardcoded ythickness */
        p->x      = 0;
        p->y      = 0;
        p->width  = width;
        p->height = height;
        p->size_x = p->width  - 2 * p->pad_x;
        p->size_y = p->height - 2 * p->pad_y;
        float radius = 4.f, bevel = 0.2f, shadow = 4.f, lights = 1.f, dull = 0.25f;
        cairo_t *bg = cairo_create(p->background_surface);
        display_background(widget, bg, 0, 0, p->size_x, p->size_y, p->pad_x, p->pad_y, radius, bevel, 1, shadow, lights, dull);
        calf_pattern_draw_background(widget, bg);
        cairo_destroy(bg);
    }
    cairo_rectangle(c, p->x, p->y, p->width, p->height);
    cairo_clip(c);

    cairo_rectangle(c, p->x, p->y, p->width, p->height);
    cairo_set_source_surface(c, p->background_surface, p->x, p->y);
    cairo_fill(c);

    for (int i = 0; i < p->bars; i ++) {
        for (int j = 0; j < p->beats; j++) {
            if  ((p->handle_grabbed.bar == i  and p->handle_grabbed.beat == j)
            or  ((p->handle_hovered.bar == i  and p->handle_hovered.beat == j)
            and  (p->handle_grabbed.bar == -1 and p->handle_grabbed.beat == -1))) {
                calf_pattern_draw_handle(widget, c, i, j, p->x, p->y, 1.0, 0.1);
            }
        }
    }

    for (int i = 0; i < p->bars; i++) {
        for (int j = 0; j < p->beats; j++) {
            double val = p->values[i][j];
            if (val > 0)
                calf_pattern_draw_handle(widget, c, i, j, p->x, p->y, val, 0.8);
        }
    }

    p->force_redraw = false;
    cairo_destroy(c);
}

static calf_pattern_handle
calf_pattern_get_handle_at(CalfPattern *p, double x, double y)
{
    g_assert(CALF_IS_PATTERN(p));
    GdkRectangle rect;
    calf_pattern_handle ret;
    ret.bar = -1;
    ret.beat = -1;
    for (int i = 0; i < p->bars; i++) {
        for (int j = 0; j < p->beats; j++) {
            rect = calf_pattern_handle_rect(p, i, j, 1.0);
            if (x > rect.x and x < rect.x + rect.width) {
                ret.bar = i;
                ret.beat = j;
                return ret;
            }
        }
    }
    return ret;
}

static double
calf_pattern_get_drag_value(CalfPattern *p, double y, double value)
{
    g_assert(CALF_IS_PATTERN(p));
    return std::max(0., std::min(1., value + (p->mouse_y - y) / p->beat_height));
}
static double
calf_pattern_get_value_from_y(CalfPattern *p, double y)
{
    g_assert(CALF_IS_PATTERN(p));
    double _y = (y - p->border_v - p->mbars - p->pad_y) / p->beat_height;
    return 1 - std::max(0., std::min(1., _y));
}

static void
calf_pattern_motion (GtkEventControllerMotion *controller,
                     double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);

    if (p->handle_grabbed.bar >= 0 and p->handle_grabbed.beat >= 0) {
        // handle grabbed
        double val = p->values[p->handle_grabbed.bar][p->handle_grabbed.beat];
        double new_value = calf_pattern_get_drag_value(p, ey, val);
        p->values[p->handle_grabbed.bar][p->handle_grabbed.beat] = new_value;
        p->mouse_x = ex;
        p->mouse_y = ey;
        g_signal_emit_by_name(widget, "handle-changed", &p->handle_grabbed);
        gtk_widget_queue_draw(widget);
    } else {
        // no handle grabbed
        calf_pattern_handle hh = calf_pattern_get_handle_at(p, ex, ey);
        if (hh.bar != p->handle_hovered.bar or hh.beat != p->handle_hovered.beat) {
            if (hh.bar >= 0 and hh.beat >= 0) {
                p->handle_hovered.bar  = hh.bar;
                p->handle_hovered.beat = hh.beat;
            } else {
                p->handle_hovered.bar  = -1;
                p->handle_hovered.beat = -1;
            }
            gtk_widget_queue_draw(widget);
        }
    }
}

static void
calf_pattern_gesture_pressed (GtkGestureClick *gesture, int n_press,
                               double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);
    bool inside_handle = false;

    p->mouse_x = ex;
    p->mouse_y = ey;

    calf_pattern_handle h = calf_pattern_get_handle_at(p, ex, ey);
    if (h.bar >= 0 and h.beat >= 0) {
        p->handle_grabbed.bar  = h.bar;
        p->handle_grabbed.beat = h.beat;
        inside_handle = true;
    }
    double val = p->values[p->handle_grabbed.bar][p->handle_grabbed.beat];
    p->startval = val;

    if (inside_handle && n_press == 2) {
        // double click
        p->values[p->handle_grabbed.bar][p->handle_grabbed.beat] = val < 0.5 ? 1 : 0;
        g_signal_emit_by_name(widget, "handle-changed", &p->handle_grabbed);
        p->mouse_x = -1;
        p->mouse_y = -1;
        p->handle_grabbed.bar  = -1;
        p->handle_grabbed.beat = -1;
        p->dblclick = true;
    }

    gtk_widget_grab_focus(widget);
    gtk_widget_queue_draw(widget);
}

static void
calf_pattern_gesture_released (GtkGestureClick *gesture, int n_press,
                                double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);
    calf_pattern_handle h = p->handle_grabbed;
    if (h.bar < 0 or h.beat < 0)
        return;

    double val = p->values[h.bar][h.beat];
    if (!p->dblclick and abs(p->startval - val) < 0.05) {
        // single click
        val = calf_pattern_get_value_from_y(p, ey);
        p->values[h.bar][h.beat] = val;
        g_signal_emit_by_name(widget, "handle-changed", &p->handle_grabbed);
    }
    p->dblclick            = false;
    p->mouse_x             = -1;
    p->mouse_y             = -1;
    p->handle_grabbed.bar  = -1;
    p->handle_grabbed.beat = -1;

    calf_pattern_handle hh = calf_pattern_get_handle_at(p, ex, ey);
    if (hh.bar >= 0 and hh.beat >= 0) {
        p->handle_hovered.bar  = hh.bar;
        p->handle_hovered.beat = hh.beat;
    }

    gtk_widget_queue_draw(widget);
}

static gboolean
calf_pattern_scroll (GtkEventControllerScroll *controller,
                     double dx, double dy, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);

    /* Obtain pointer position from the controller's widget */
    double ex = p->mouse_x, ey = p->mouse_y;
    calf_pattern_handle h = calf_pattern_get_handle_at(p, ex, ey);
    if (h.bar >= 0 and h.beat >= 0) {
        if (dy < 0) {
            // scroll up — raise handle value
            p->values[h.bar][h.beat] = std::min(1., p->values[h.bar][h.beat] + 0.1);
            g_signal_emit_by_name(widget, "handle-changed", &h);
        } else if (dy > 0) {
            // scroll down — lower handle value
            p->values[h.bar][h.beat] = std::max(0., p->values[h.bar][h.beat] - 0.1);
            g_signal_emit_by_name(widget, "handle-changed", &h);
        }
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

static void
calf_pattern_leave (GtkEventControllerMotion *controller, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);
    p->handle_hovered.bar  = -1;
    p->handle_hovered.beat = -1;
    gtk_widget_queue_draw(widget);
}

static void
calf_pattern_measure (GtkWidget *widget,
                      GtkOrientation orientation,
                      int for_size,
                      int *minimum, int *natural,
                      int *minimum_baseline, int *natural_baseline)
{
    g_assert(CALF_IS_PATTERN(widget));
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = *natural = 300;
    } else {
        *minimum = *natural = 60;
    }
    *minimum_baseline = *natural_baseline = -1;
}

static void
calf_pattern_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    g_assert(CALF_IS_PATTERN(widget));
    CalfPattern *p = CALF_PATTERN(widget);
    int sx = width  - (int)(p->pad_x * 2);
    int sy = height - (int)(p->pad_y * 2);
    if (sx != (int)p->size_x or sy != (int)p->size_y) {
        p->size_x = sx;
        p->size_y = sy;
        if (p->background_surface)
            cairo_surface_destroy( p->background_surface );
        p->background_surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, width, height );
        p->force_redraw = true;
    }
}

static void
calf_pattern_class_init (CalfPatternClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot      = calf_pattern_snapshot;
    widget_class->measure       = calf_pattern_measure;
    widget_class->size_allocate = calf_pattern_size_allocate;

    g_signal_new("handle-changed",
         G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
         0, NULL, NULL,
         g_cclosure_marshal_VOID__POINTER,
         G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
calf_pattern_unrealize (GtkWidget *widget, CalfPattern *p)
{
    cairo_surface_destroy(p->background_surface);
}

static void
calf_pattern_init (CalfPattern *p)
{
    GtkWidget *widget = GTK_WIDGET(p);

    gtk_widget_set_focusable(widget, TRUE);
    gtk_widget_set_size_request(widget, 300, 60);

    p->pad_x         = 1; /* hardcoded xthickness */
    p->pad_y         = 1; /* hardcoded ythickness */
    p->force_redraw  = false;
    p->beats         = 1;
    p->bars          = 1;

    g_signal_connect(G_OBJECT(widget), "unrealize", G_CALLBACK(calf_pattern_unrealize), (gpointer)p);

    p->handle_hovered.bar  = -1;
    p->handle_hovered.beat = -1;
    p->handle_grabbed.bar  = -1;
    p->handle_grabbed.beat = -1;

    p->background_surface = NULL;

    /* Click gesture */
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0); /* all buttons */
    g_signal_connect(click, "pressed",  G_CALLBACK(calf_pattern_gesture_pressed),  widget);
    g_signal_connect(click, "released", G_CALLBACK(calf_pattern_gesture_released), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));

    /* Motion + leave controllers */
    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion", G_CALLBACK(calf_pattern_motion), widget);
    g_signal_connect(motion, "leave",  G_CALLBACK(calf_pattern_leave),  widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(motion));

    /* Scroll controller */
    GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL(
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL));
    g_signal_connect(scroll, "scroll", G_CALLBACK(calf_pattern_scroll), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(scroll));
}

GtkWidget *
calf_pattern_new()
{
    return GTK_WIDGET( g_object_new (CALF_TYPE_PATTERN, NULL ));
}

GType
calf_pattern_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo type_info = {
            sizeof(CalfPatternClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_pattern_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfPattern),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_pattern_init
        };

        GTypeInfo *type_info_copy = new GTypeInfo(type_info);

        for (int i = 0; ; i++) {
            const char *name = "CalfPattern";
            //char *name = g_strdup_printf("CalfPattern%u%d", ((unsigned int)(intptr_t)calf_line_graph_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static( GTK_TYPE_WIDGET,
                                           name,
                                           type_info_copy,
                                           (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
