/* Calf DSP Library
 * Knob control.
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
#include <calf/ctl_knob.h>
#include <calf/drawingutils.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include <algorithm>
#include <stdlib.h>

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

#define range01(tick) std::min(1., std::max(0., tick))

///////////////////////////////////////// knob ///////////////////////////////////////////////

static void
calf_knob_get_color (CalfKnob *self, float deg, float phase, float start, float last, float tickw, float *r, float *g, float *b, float *a)
{
    GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
    GtkWidget *widget = GTK_WIDGET(self);

    //printf ("get color: phase %.2f deg %.2f\n", phase, deg);
    if (self->type == 0) {
        // normal
        if (!(deg > phase or phase == start))
            state = GTK_STATE_FLAG_PRELIGHT;
    }
    if (self->type == 1) {
        // centered
        if (deg > 270 and deg <= phase and phase > 270)
            state = GTK_STATE_FLAG_PRELIGHT;
        if (deg <= 270 and deg > phase and phase < 270)
            state = GTK_STATE_FLAG_PRELIGHT;
        if ((deg == start and phase == start)
        or  (deg == 270.  and phase > 270.))
            state = GTK_STATE_FLAG_PRELIGHT;
    }
    if (self->type == 2) {
        // reverse
        if (deg > phase or phase == start)
            state = GTK_STATE_FLAG_PRELIGHT;
    }
    if (self->type == 3) {
        for (unsigned j = 0; j < self->ticks.size(); j++) {
            float tp = fmod((start + range01(self->ticks[j]) * 360.) - phase + 360, 360);
            if (tp > 360 - tickw or tp < tickw) {
                state = GTK_STATE_FLAG_PRELIGHT;
            }
        }
        if (deg > phase and deg > last + tickw and last < phase)
            state = GTK_STATE_FLAG_PRELIGHT;

    }
    get_fg_color(widget, &state, r, g, b);
    if (state == GTK_STATE_FLAG_NORMAL)
        *a = 0.2f;
    else
        *a = 1.0f;

}

static void
calf_knob_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);

    if (!self->knob_image)
        return;

    GdkPixbuf *pixbuf = self->knob_image;
    gint iw = gdk_pixbuf_get_width(pixbuf);
    gint ih = gdk_pixbuf_get_height(pixbuf);

    if (self->debug > 1)
        printf("pixbuf: %d x %d\n", iw, ih);

    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(widget));

    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
    cairo_t *ctx = gtk_snapshot_append_cairo(snapshot, &bounds);

    float r, g, b;
    GtkStateFlags state;

    float rmargin, rwidth, tmargin, twidth, tlength;
    switch (self->size) {
        case 1:  rmargin=2.2f; rwidth=2.2f; tmargin=6.0f;  twidth=3.0f; tlength=3.0f;  break;
        case 2:  rmargin=5.5f; rwidth=3.5f; tmargin=12.0f; twidth=1.0f; tlength=8.0f;  break;
        case 3:  rmargin=4.0f; rwidth=3.8f; tmargin=9.2f;  twidth=1.0f; tlength=5.5f;  break;
        case 4:  rmargin=6.2f; rwidth=4.2f; tmargin=12.0f; twidth=1.0f; tlength=14.0f; break;
        case 5:  rmargin=8.5f; rwidth=4.5f; tmargin=16.5f; twidth=1.0f; tlength=17.0f; break;
        default: rmargin=5.5f; rwidth=3.5f; tmargin=12.0f; twidth=1.0f; tlength=8.0f;  break;
    }

    if (self->debug > 1)
        printf("gtkrc: rm %.2f | rw %.2f | tm %.2f | tw %.2f | tl %.2f\n", rmargin, rwidth, tmargin, twidth, tlength);

    double ox   = (width - iw) / 2;
    double oy   = (height - ih) / 2;
    double size = iw;
    float  rad  = size / 2;
    double xc   = ox + rad;
    double yc   = oy + rad;

    if (self->debug > 1)
        printf("position: %.2f x %.2f\n", ox, oy);

    unsigned int tick;
    double phase;
    double base;
    double deg;
    double end;
    double last;
    double start;
    double nend;
    double zero;
    float opac = 0;

    double perim  = (rad - rmargin) * 2 * M_PI;
    double tickw  = 2. / perim * 360.;
    double tickw2 = tickw / 2.;

    cairo_rectangle(ctx, ox, oy, size + size / 2, size + size / 2);
    cairo_clip(ctx);

    // draw background
    calf_cairo_set_source_pixbuf(ctx, pixbuf, ox, oy);
    cairo_rectangle(ctx, ox, oy, iw, ih);
    cairo_fill(ctx);

    switch (self->type) {
        default:
        case 0:
            // normal knob
            start = 135.;
            end   = 405.;
            base  = 270.;
            zero  = 135.;
        case 1:
            // centered @ 270°
            start = 135.;
            end   = 405.;
            base  = 270.;
            zero  = 270.;
        case 2:
            // reversed
            start = 135.;
            end   = 405.;
            base  = 270.;
            zero  = 135.;
            break;
        case 3:
            // 360°
            start = -90.;
            end   = 270.;
            base  = 360.;
            zero  = -90.;
            break;
    }

    GtkAdjustment *range_adj = gtk_range_get_adjustment(GTK_RANGE(widget));
    double adj_upper = gtk_adjustment_get_upper(range_adj);
    double adj_lower = gtk_adjustment_get_lower(range_adj);
    double adj_value = gtk_adjustment_get_value(range_adj);

    tick  = 0;
    nend  = 0.;
    deg = last = start;
    phase = (adj_value - adj_lower) * base / (adj_upper - adj_lower) + start;

    // draw pin
    state = GTK_STATE_FLAG_ACTIVE;
    get_fg_color(widget, &state, &r, &g, &b);
    float x1 = ox + rad + (rad - tmargin) * cos(phase * (M_PI / 180.));
    float y1 = oy + rad + (rad - tmargin) * sin(phase * (M_PI / 180.));
    float x2 = ox + rad + (rad - tlength - tmargin) * cos(phase * (M_PI / 180.));
    float y2 = oy + rad + (rad - tlength - tmargin) * sin(phase * (M_PI / 180.));
    cairo_move_to(ctx, x1, y1);
    cairo_line_to(ctx, x2, y2);
    cairo_set_source_rgba(ctx, r, g, b, 1);
    cairo_set_line_width(ctx, twidth);
    cairo_stroke(ctx);

    if (self->debug > 1)
        printf("pin color: %.2f | %.2f | %.2f\n", r, g, b);

    cairo_set_line_width(ctx, rwidth);

    // draw ticks and rings
    state = GTK_STATE_FLAG_NORMAL;
    get_fg_color(widget, &state, &r, &g, &b);
    unsigned int evsize = 4;
    double events[4] = { start, zero, end, phase };
    if (self->type == 3)
        evsize = 3;
    std::sort(events, events + evsize);
    if (self->debug) {
        printf("start %.2f end %.2f last %.2f deg %.2f tick %d ticks %d phase %.2f base %.2f nend %.2f\n", start, end, last, deg, tick, int(self->ticks.size()), phase, base, nend);
        for (unsigned int i = 0; i < self->ticks.size(); i++) {
            printf("tick %d %.2f\n", i, self->ticks[i]);
        }
    }
    while (deg <= end) {
        if (self->debug) printf("tick %d deg %.2f last %.2f end %.2f\n", tick, deg, last, end);
        if (self->ticks.size() and tick < self->ticks.size() and deg == start + range01(self->ticks[tick]) * base) {
            // seems we want to draw a tick on this angle.
            // so we have to fill the void between the last set angle
            // and the point directly before the tick first.
            // (draw from last known angle to tickw2 + tickw before actual deg)
            if (last < deg - tickw - tickw2) {
                calf_knob_get_color(self, (deg - tickw - tickw2), phase, start, last, tickw + tickw2, &r, &g, &b, &opac);
                cairo_set_source_rgba(ctx, r, g, b, opac);
                cairo_arc(ctx, xc, yc, rad - rmargin, last * (M_PI / 180.), std::max(last, std::min(nend, (deg - tickw - tickw2))) * (M_PI / 180.));
                cairo_stroke(ctx);
                if (self->debug) printf("fill from %.2f to %.2f @ %.2f\n", last, (deg - tickw - tickw2), opac);
                if (self->debug > 1)
                    printf("color: %.2f | %.2f | %.2f\n", r, g, b);
            }
            // draw the tick itself
            calf_knob_get_color(self, deg, phase, start, end, tickw + tickw2, &r, &g, &b, &opac);
            cairo_set_source_rgba(ctx, r, g, b, opac);
            cairo_arc(ctx, xc, yc, rad - rmargin, (deg - tickw2) * (M_PI / 180.), (deg + tickw2) * (M_PI / 180.));
            cairo_stroke(ctx);
            if (self->debug) printf("tick from %.2f to %.2f @ %.2f\n", (deg - tickw2), (deg + tickw2), opac);
            if (self->debug > 1)
                printf("color: %.2f | %.2f | %.2f\n", r, g, b);
            // set last known angle to deg plus tickw + tickw2
            last = deg + tickw + tickw2;
            // and count up tick
            tick ++;
            // remember the next ticks void end
            if (tick < self->ticks.size())
                nend = range01(self->ticks[tick]) * base + start - tickw - tickw2;
            else
                nend = end;
        } else {
            // seems we want to fill a gap between the last event and
            // the actual one, while the actual one isn't a tick (but a
            // knobs position or a center)
            if ((last < deg)) {
                calf_knob_get_color(self, deg, phase, start, last, tickw + tickw2, &r, &g, &b, &opac);
                cairo_set_source_rgba(ctx, r, g, b, opac);
                cairo_arc(ctx, xc, yc, rad - rmargin, last * (M_PI / 180.), std::min(nend, std::max(last, deg)) * (M_PI / 180.));
                cairo_stroke(ctx);
                if (self->debug) printf("void from %.2f to %.2f @ %.2f\n", last, std::min(nend, std::max(last, deg)), opac);
                if (self->debug > 1)
                    printf("color: %.2f | %.2f | %.2f\n", r, g, b);
            }
            last = deg;
        }
        if (deg >= end)
            break;
        // set deg to next event
        for (unsigned int i = 0; i < evsize; i++) {
            if (self->debug > 1) printf("checking %.2f (start %.2f zero %.2f phase %.2f end %.2f)\n", events[i], start, zero, phase, end);
            if (events[i] > deg) {
                deg = events[i];
                if (self->debug > 1) printf("taken.\n");
                break;
            }
        }
        if (tick < self->ticks.size()) {
            deg = std::min(deg, start + range01(self->ticks[tick]) * base);
            if (self->debug > 1) printf("checking tick %d %.2f\n", tick, start + range01(self->ticks[tick]) * base);
        }
        //deg = std::max(last, deg);
        if (self->debug > 1) printf("finally! deg %.2f\n", deg);
    }
    if (self->debug) printf("\n");
    cairo_destroy(ctx);
}

static void
calf_knob_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                   int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);
    int size = 40;
    if (self->knob_image) {
        if (orientation == GTK_ORIENTATION_HORIZONTAL)
            size = gdk_pixbuf_get_width(self->knob_image);
        else
            size = gdk_pixbuf_get_height(self->knob_image);
    }
    if (minimum)         *minimum = size;
    if (natural)         *natural = size;
    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

void
calf_knob_set_size (CalfKnob *self, int size)
{
    char name[128];
    GtkWidget *widget = GTK_WIDGET(self);
    self->size = size;
    snprintf(name, sizeof(name), "%s_%d\n", gtk_widget_get_name(widget), size);
    gtk_widget_set_name(widget, name);
    gtk_widget_queue_resize(widget);
}

void
calf_knob_set_pixbuf (CalfKnob *self, GdkPixbuf *pixbuf)
{
    self->knob_image = pixbuf;
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void calf_knob_enter (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    if (gtk_widget_get_state_flags(widget) == GTK_STATE_FLAG_NORMAL) {
        gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
        gtk_widget_queue_draw(widget);
    }
}

static void calf_knob_leave (GtkEventControllerMotion *controller, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    if ((gtk_widget_get_state_flags(widget) & GTK_STATE_FLAG_PRELIGHT) != 0) {
        gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_PRELIGHT);
        gtk_widget_queue_draw(widget);
    }
}

static void
calf_knob_incr (GtkWidget *widget, int dir_down)
{
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(widget));

    double adj_value = gtk_adjustment_get_value(adj);
    double adj_lower = gtk_adjustment_get_lower(adj);
    double adj_upper = gtk_adjustment_get_upper(adj);
    double adj_step  = gtk_adjustment_get_step_increment(adj);

    int oldstep = (int)(0.5f + (adj_value - adj_lower) / adj_step);
    int step;
    int nsteps = (int)(0.5f + (adj_upper - adj_lower) / adj_step); // less 1 actually
    if (dir_down)
        step = oldstep - 1;
    else
        step = oldstep + 1;
    if (self->type == 3 && step >= nsteps)
        step %= nsteps;
    if (self->type == 3 && step < 0)
        step = nsteps - (nsteps - step) % nsteps;

    // trying to reduce error cumulation here, by counting from lowest or from highest
    float value = adj_lower + step * double(adj_upper - adj_lower) / nsteps;
    gtk_range_set_value(GTK_RANGE(widget), value);
    // printf("step %d:%d nsteps %d value %f:%f\n", oldstep, step, nsteps, oldvalue, value);
}

static gboolean
calf_knob_key_press (GtkEventControllerKey *ctrl, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(widget));
    gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_ACTIVE, FALSE);
    gtk_widget_queue_draw(widget);
    switch(keyval)
    {
        case GDK_KEY_Home:
            gtk_range_set_value(GTK_RANGE(widget), gtk_adjustment_get_lower(adj));
            return TRUE;

        case GDK_KEY_End:
            gtk_range_set_value(GTK_RANGE(widget), gtk_adjustment_get_upper(adj));
            return TRUE;

        case GDK_KEY_Up:
            calf_knob_incr(widget, 0);
            return TRUE;

        case GDK_KEY_Down:
            calf_knob_incr(widget, 1);
            return TRUE;

        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
            self->start_value = gtk_range_get_value(GTK_RANGE(widget));
            self->start_y = self->last_y;
            return TRUE;
    }

    return FALSE;
}

static gboolean
calf_knob_key_release (GtkEventControllerKey *ctrl, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);

    if(keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R)
    {
        self->start_value = gtk_range_get_value(GTK_RANGE(widget));
        self->start_y = self->last_y;
        return TRUE;
    }
    gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_ACTIVE);
    gtk_widget_queue_draw(widget);
    return FALSE;
}

static void
calf_knob_button_press (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);
    int button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (n_press == 2 && button == 1) {
        gtk_range_set_value(GTK_RANGE(widget), self->default_value);
    }

    gtk_widget_grab_focus(widget);
    self->drag_active = TRUE;
    self->start_x = x;
    self->last_y = self->start_y = y;
    self->start_value = gtk_range_get_value(GTK_RANGE(widget));
    gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_ACTIVE, FALSE);
    gtk_widget_queue_draw(widget);
}

static void
calf_knob_button_release (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET(user_data);
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);
    self->drag_active = FALSE;
    gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_ACTIVE);
    gtk_widget_queue_draw(widget);
}

static inline float endless(float value)
{
    if (value >= 0)
        return fmod(value, 1.f);
    else
        return fmod(1.f - fmod(1.f - value, 1.f), 1.f);
}

static inline float deadzone(GtkWidget *widget, float value, float incr)
{
    // map to dead zone
    float ov = value;
    if (ov > 0.5)
        ov = 0.1 + ov;
    if (ov < 0.5)
        ov = ov - 0.1;

    float nv = ov + incr;

    if (nv > 0.6)
        return nv - 0.1;
    if (nv < 0.4)
        return nv + 0.1;
    return 0.5;
}

static void
calf_knob_motion (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    g_assert(CALF_IS_KNOB(widget));
    CalfKnob *self = CALF_KNOB(widget);

    float scale = 250;

    if (self->drag_active)
    {
        if (self->type == 3)
        {
            gtk_range_set_value(GTK_RANGE(widget), endless(self->start_value - (y - self->start_y) / scale));
        }
        else
        if (self->type == 1)
        {
            gtk_range_set_value(GTK_RANGE(widget), deadzone(GTK_WIDGET(widget), self->start_value, -(y - self->start_y) / scale));
        }
        else
        {
            gtk_range_set_value(GTK_RANGE(widget), self->start_value - (y - self->start_y) / scale);
        }
    }
    self->last_y = y;
}

static gboolean
calf_knob_scroll (GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
    GtkWidget *widget = (GtkWidget*)user_data;
    calf_knob_incr(widget, dy > 0 ? 1 : 0);
    return TRUE;
}

static void
calf_knob_class_init (CalfKnobClass *klass)
{
    // GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = calf_knob_snapshot;
    widget_class->measure  = calf_knob_measure;
}

static void
calf_knob_init (CalfKnob *self)
{
    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_focusable(widget, TRUE);
    gtk_widget_set_size_request(widget, 40, 40);
    self->knob_image = NULL;

    /* scroll controller */
    GtkEventControllerScroll *scroll_ctrl = GTK_EVENT_CONTROLLER_SCROLL(
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES));
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(calf_knob_scroll), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(scroll_ctrl));

    /* click controller */
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed",  G_CALLBACK(calf_knob_button_press),   GTK_WIDGET(self));
    g_signal_connect(click, "released", G_CALLBACK(calf_knob_button_release), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

    /* motion controller (for enter/leave/hover) */
    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    g_signal_connect(motion, "enter", G_CALLBACK(calf_knob_enter), GTK_WIDGET(self));
    g_signal_connect(motion, "leave", G_CALLBACK(calf_knob_leave), GTK_WIDGET(self));
    g_signal_connect(motion, "motion", G_CALLBACK(calf_knob_motion), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(motion));

    /* key controller */
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed",  G_CALLBACK(calf_knob_key_press),   GTK_WIDGET(self));
    g_signal_connect(key_ctrl, "key-released", G_CALLBACK(calf_knob_key_release), GTK_WIDGET(self));
    gtk_widget_add_controller(GTK_WIDGET(self), key_ctrl);

    self->drag_active = FALSE;
}

GtkWidget *
calf_knob_new()
{
    GtkAdjustment *adj = (GtkAdjustment *)gtk_adjustment_new(0, 0, 1, 0.01, 0.5, 0);
    return calf_knob_new_with_adjustment(adj);
}

static gboolean calf_knob_value_changed(gpointer obj)
{
    GtkWidget *widget = (GtkWidget *)obj;
    gtk_widget_queue_draw(widget);
    return FALSE;
}

GtkWidget *calf_knob_new_with_adjustment(GtkAdjustment *_adjustment)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_KNOB, NULL ));
    if (widget) {
        gtk_range_set_adjustment(GTK_RANGE(widget), _adjustment);
        g_signal_connect(G_OBJECT(widget), "value-changed", G_CALLBACK(calf_knob_value_changed), widget);
    }
    return widget;
}

GType
calf_knob_get_type (void)
{
    static GType type = 0;
    if (!type) {

        static const GTypeInfo type_info = {
            sizeof(CalfKnobClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_knob_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfKnob),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_knob_init
        };

        for (int i = 0; ; i++) {
            //char *name = g_strdup_printf("CalfKnob%u%d",
                //((unsigned int)(intptr_t)calf_knob_class_init) >> 16, i);
            const char *name = "CalfKnob";
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_RANGE,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
