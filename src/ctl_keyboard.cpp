/* Calf DSP Library
 * Barely started keyboard widget. Planned to be usable as
 * a ruler for curves, and possibly as input widget in future
 * as well (that's what event sink interface is for, at least).
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
#include <calf/ctl_keyboard.h>
#include <stdint.h>
#include <stdlib.h>

static const int semitones_b[] = { 1, 3, -1, 6, 8, 10, -1 };
static const int semitones_w[] = { 0, 2, 4, 5, 7, 9, 11 };


GtkWidget *
calf_keyboard_new()
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_KEYBOARD, NULL ));
    return widget;
}

static void
calf_keyboard_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_KEYBOARD(widget));

    cairo_pattern_t *pat;
    CalfKeyboard *self = CALF_KEYBOARD(widget);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    int sy = height - 1;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

    cairo_set_line_join(c, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_width(c, 1);

    for (int i = 0; i < self->nkeys; i++)
    {
        CalfKeyboard::KeyInfo ki = { 0.5 + 11 * i, 0.5, 11, (double)sy, 12 * (i / 7) + semitones_w[i % 7], false };
        cairo_new_path(c);
        if (!self->sink->pre_draw(c, ki))
        {
            cairo_rectangle(c, ki.x, ki.y, ki.width, ki.y + ki.height);

            pat = cairo_pattern_create_linear (ki.x, ki.y, ki.x, ki.y + ki.height);
            cairo_pattern_add_color_stop_rgb (pat, 0.0, 0.25, 0.25, 0.2);
            cairo_pattern_add_color_stop_rgb (pat, 0.1, 0.957, 0.914, 0.925);
            cairo_pattern_add_color_stop_rgb (pat, 1.0, 0.796, 0.787, 0.662);
            cairo_set_source(c, pat);
            cairo_fill(c);

            cairo_set_source_rgba(c, 0, 0, 0, 0.5);

            if (!self->sink->pre_draw_outline(c, ki))
                cairo_stroke(c);
            else
                cairo_new_path(c);
            self->sink->post_draw(c, ki);
        }
    }

    for (int i = 0; i < self->nkeys - 1; i++)
    {
        if ((1 << (i % 7)) & 59)
        {
            CalfKeyboard::KeyInfo ki = { 8.5 + 11 * i, 0.5, 6, (double)sy * 3 / 5, 12 * (i / 7) + semitones_b[i % 7], true };
            cairo_new_path(c);
            cairo_rectangle(c, ki.x, ki.y, ki.width, ki.height);
            if (!self->sink->pre_draw(c, ki))
            {
                pat = cairo_pattern_create_linear (ki.x, ki.y, ki.x, ki.height + ki.y);
                cairo_pattern_add_color_stop_rgb (pat, 0.0, 0, 0, 0);
                cairo_pattern_add_color_stop_rgb (pat, 0.1, 0.27, 0.27, 0.27);
                cairo_pattern_add_color_stop_rgb (pat, 1.0, 0, 0, 0);
                cairo_set_source(c, pat);
                cairo_fill(c);

                pat = cairo_pattern_create_linear (ki.x + 1, ki.y, ki.x + 1, (int)(ki.height * 0.8 + ki.y));
                cairo_pattern_add_color_stop_rgb (pat, 0.0, 0, 0, 0);
                cairo_pattern_add_color_stop_rgb (pat, 0.1, 0.55, 0.55, 0.55);
                cairo_pattern_add_color_stop_rgb (pat, 0.5, 0.45, 0.45, 0.45);
                cairo_pattern_add_color_stop_rgb (pat, 0.5001, 0.35, 0.35, 0.35);
                cairo_pattern_add_color_stop_rgb (pat, 1.0, 0.25, 0.25, 0.25);
                cairo_set_source(c, pat);
                cairo_rectangle(c, ki.x + 1, ki.y, ki.width - 2, (int)(ki.height * 0.8 + ki.y));
                cairo_fill(c);

                self->sink->post_draw(c, ki);
            }
        }
    }

    /* top shadow overlay — widget-local coordinates (no allocation offset) */
    pat = cairo_pattern_create_linear (0, 0, 0, (int)(height * 0.2));
    cairo_pattern_add_color_stop_rgba (pat, 0.0, 0, 0, 0, 0.4);
    cairo_pattern_add_color_stop_rgba (pat, 1.0, 0, 0, 0, 0);
    cairo_rectangle(c, 0, 0, width, (int)(height * 0.2));
    cairo_set_source(c, pat);
    cairo_fill(c);

    self->sink->post_all(c);

    cairo_destroy(c);
}

static void
calf_keyboard_measure (GtkWidget *widget,
                       GtkOrientation orientation,
                       int for_size,
                       int *minimum, int *natural,
                       int *minimum_baseline, int *natural_baseline)
{
    CalfKeyboard *self = CALF_KEYBOARD(widget);
    g_assert(CALF_IS_KEYBOARD(widget));

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = *natural = 11 * self->nkeys + 1;
    } else {
        *minimum = *natural = 40;
    }
    *minimum_baseline = *natural_baseline = -1;
}

static void
calf_keyboard_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    g_assert(CALF_IS_KEYBOARD(widget));
    /* GTK4 handles allocation internally */
}

int
calf_keyboard_pos_to_note (CalfKeyboard *kb, int x, int y, int *vel = NULL)
{
    int height = gtk_widget_get_height(GTK_WIDGET(kb));
    // first try black keys
    if (y <= height * 3 / 5 && x >= 0 && (x - 8) % 12 < 8)
    {
        int blackkey = (x - 8) / 12;
        if (blackkey < kb->nkeys && (59 & (1 << (blackkey % 7))))
        {
            return semitones_b[blackkey % 7] + 12 * (blackkey / 7);
        }
    }
    // if not a black key, then which white one?
    int whitekey = x / 12;
    // semitones within octave + 12 semitones per octave
    return semitones_w[whitekey % 7] + 12 * (whitekey / 7);
}

static void
calf_keyboard_key_press (GtkEventControllerKey *controller,
                         guint keyval, guint keycode,
                         GdkModifierType state, gpointer data)
{
    /* placeholder — no logic in original either */
    (void)controller; (void)keyval; (void)keycode; (void)state; (void)data;
}

static void
calf_keyboard_gesture_pressed (GtkGestureClick *gesture, int n_press,
                                double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_KEYBOARD(widget));
    CalfKeyboard *self = CALF_KEYBOARD(widget);
    if (!self->interactive)
        return;
    gtk_widget_grab_focus(widget);
    int vel = 127;
    self->last_key = calf_keyboard_pos_to_note(self, (int)ex, (int)ey, &vel);
    if (self->last_key != -1)
        self->sink->note_on(self->last_key, vel);
}

static void
calf_keyboard_gesture_released (GtkGestureClick *gesture, int n_press,
                                 double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_KEYBOARD(widget));
    CalfKeyboard *self = CALF_KEYBOARD(widget);
    if (!self->interactive)
        return;
    if (self->last_key != -1)
        self->sink->note_off(self->last_key);
}

static void
calf_keyboard_motion (GtkEventControllerMotion *controller,
                      double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_KEYBOARD(widget));
    CalfKeyboard *self = CALF_KEYBOARD(widget);
    if (!self->interactive)
        return;
    int vel = 127;
    int key = calf_keyboard_pos_to_note(self, (int)ex, (int)ey, &vel);
    if (key != self->last_key)
    {
        if (self->last_key != -1)
            self->sink->note_off(self->last_key);
        self->last_key = key;
        if (self->last_key != -1)
            self->sink->note_on(self->last_key, vel);
    }
}

static void
calf_keyboard_class_init (CalfKeyboardClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot      = calf_keyboard_snapshot;
    widget_class->measure       = calf_keyboard_measure;
    widget_class->size_allocate = calf_keyboard_size_allocate;
}

static void
calf_keyboard_init (CalfKeyboard *self)
{
    static CalfKeyboard::EventAdapter default_sink;
    GtkWidget *widget = GTK_WIDGET(self);
    g_assert(CALF_IS_KEYBOARD(widget));
    gtk_widget_set_focusable(widget, TRUE);
    self->nkeys    = 7 * 3 + 1;
    self->sink     = &default_sink;
    self->last_key = -1;

    /* Click gesture (press + release) */
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click, "pressed",  G_CALLBACK(calf_keyboard_gesture_pressed),  widget);
    g_signal_connect(click, "released", G_CALLBACK(calf_keyboard_gesture_released), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));

    /* Motion controller */
    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion", G_CALLBACK(calf_keyboard_motion), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(motion));

    /* Key controller */
    GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY(gtk_event_controller_key_new());
    g_signal_connect(key, "key-pressed", G_CALLBACK(calf_keyboard_key_press), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(key));
}

GType
calf_keyboard_get_type (void)
{
    static GType type = 0;
    if (!type) {

        static const GTypeInfo type_info = {
            sizeof(CalfKeyboardClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_keyboard_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfKeyboard),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_keyboard_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfKeyboard";
            //char *name = g_strdup_printf("CalfKeyboard%u%d",
                //((unsigned int)(intptr_t)calf_keyboard_class_init) >> 16, i);
            if (g_type_from_name(name)) {
                //free(name);
                continue;
            }
            type = g_type_register_static(GTK_TYPE_WIDGET,
                                          name,
                                          &type_info,
                                          (GTypeFlags)0);
            //free(name);
            break;
        }
    }
    return type;
}
