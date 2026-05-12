/* Calf DSP Library
 * Barely started curve editor widget. Standard GtkCurve is
 * unreliable and deprecated, so I need to make my own.
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
#include <calf/ctl_curve.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

static gpointer parent_class = NULL;

GtkWidget *
calf_curve_new(unsigned int point_limit)
{
    GtkWidget *widget = GTK_WIDGET( g_object_new (CALF_TYPE_CURVE, NULL ));
    g_assert(CALF_IS_CURVE(widget));

    CalfCurve *self = CALF_CURVE(widget);
    self->point_limit = point_limit;
    return widget;
}

static void
calf_curve_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
    g_assert(CALF_IS_CURVE(widget));

    CalfCurve *self = CALF_CURVE(widget);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, width, height);
    cairo_t *c = gtk_snapshot_append_cairo(snapshot, &bounds);

    if (self->points->size())
    {
        cairo_set_source_rgb(c, 0.5, 0.5, 0.5); /* scLine */
        for (size_t i = 0; i < self->points->size(); i++)
        {
            const CalfCurve::point &pt = (*self->points)[i];
            if (i == (size_t)self->cur_pt && self->hide_current)
                continue;
            float x = pt.first, y = pt.second;
            self->log2phys(x, y);
            if (!i)
                cairo_move_to(c, x, y);
            else
                cairo_line_to(c, x, y);
        }
        cairo_stroke(c);
    }
    for (size_t i = 0; i < self->points->size(); i++)
    {
        if (i == (size_t)self->cur_pt && self->hide_current)
            continue;
        const CalfCurve::point &pt = (*self->points)[i];
        float x = pt.first, y = pt.second;
        self->log2phys(x, y);
        if (i == (size_t)self->cur_pt)
            cairo_set_source_rgb(c, 1.0, 0.0, 0.0); /* scHot */
        else
            cairo_set_source_rgb(c, 1.0, 1.0, 1.0); /* scPoint */
        cairo_rectangle(c, x - 2, y - 2, 5, 5);
        cairo_fill(c);
    }
    cairo_destroy(c);
}

static void
calf_curve_measure (GtkWidget *widget,
                    GtkOrientation orientation,
                    int for_size,
                    int *minimum, int *natural,
                    int *minimum_baseline, int *natural_baseline)
{
    g_assert(CALF_IS_CURVE(widget));
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = *natural = 64;
    } else {
        *minimum = *natural = 32;
    }
    *minimum_baseline = *natural_baseline = -1;
}

static void
calf_curve_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    g_assert(CALF_IS_CURVE(widget));
    /* nothing extra needed — GTK4 tracks size internally */
}

static int
find_nearest(CalfCurve *self, int ex, int ey, int &insert_pt)
{
    float dist = 5;
    int found_pt = -1;
    for (int i = 0; i < (int)self->points->size(); i++)
    {
        float x = (*self->points)[i].first, y = (*self->points)[i].second;
        self->log2phys(x, y);
        float thisdist = std::max(fabs(ex - x), fabs(ey - y));
        if (thisdist < dist)
        {
            dist = thisdist;
            found_pt = i;
        }
        if (ex > x)
            insert_pt = i + 1;
    }
    return found_pt;
}

static void
calf_curve_gesture_pressed (GtkGestureClick *gesture, int n_press,
                             double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_CURVE(widget));
    CalfCurve *self = CALF_CURVE(widget);
    int found_pt, insert_pt = -1;
    found_pt = find_nearest(self, (int)ex, (int)ey, insert_pt);
    if (found_pt == -1 && insert_pt != -1)
    {
        // if at point limit, do not start anything
        if (self->points->size() >= self->point_limit)
            return;
        float x = ex, y = ey;
        bool hide = false;
        self->phys2log(x, y);
        self->points->insert(self->points->begin() + insert_pt, CalfCurve::point(x, y));
        self->clip(insert_pt, x, y, hide);
        if (hide)
        {
            // give up
            self->points->erase(self->points->begin() + insert_pt);
            return;
        }
        (*self->points)[insert_pt] = CalfCurve::point(x, y);
        found_pt = insert_pt;
    }
    gtk_widget_grab_focus(widget);
    self->cur_pt = found_pt;
    gtk_widget_queue_draw(widget);
    if (self->sink)
        self->sink->curve_changed(self, *self->points);
    gtk_widget_set_cursor(widget, self->hand_cursor);
}

static void
calf_curve_gesture_released (GtkGestureClick *gesture, int n_press,
                              double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_CURVE(widget));
    CalfCurve *self = CALF_CURVE(widget);
    if (self->cur_pt != -1 && self->hide_current)
        self->points->erase(self->points->begin() + self->cur_pt);
    self->cur_pt = -1;
    self->hide_current = false;
    if (self->sink)
        self->sink->curve_changed(self, *self->points);
    gtk_widget_queue_draw(widget);
    GdkCursor *cursor = self->points->size() >= self->point_limit
                        ? self->arrow_cursor : self->pencil_cursor;
    gtk_widget_set_cursor(widget, cursor);
}

static void
calf_curve_motion (GtkEventControllerMotion *controller,
                   double ex, double ey, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    g_assert(CALF_IS_CURVE(widget));
    CalfCurve *self = CALF_CURVE(widget);
    if (self->cur_pt != -1)
    {
        float x = ex, y = ey;
        self->phys2log(x, y);
        self->clip(self->cur_pt, x, y, self->hide_current);
        (*self->points)[self->cur_pt] = CalfCurve::point(x, y);
        if (self->sink)
            self->sink->curve_changed(self, *self->points);
        gtk_widget_queue_draw(widget);
    }
    else
    {
        int insert_pt = -1;
        if (find_nearest(self, (int)ex, (int)ey, insert_pt) == -1)
        {
            GdkCursor *cursor = self->points->size() >= self->point_limit
                                ? self->arrow_cursor : self->pencil_cursor;
            gtk_widget_set_cursor(widget, cursor);
        }
        else
        {
            gtk_widget_set_cursor(widget, self->hand_cursor);
        }
    }
}

static void
calf_curve_finalize (GObject *obj)
{
    g_assert(CALF_IS_CURVE(obj));
    CalfCurve *self = CALF_CURVE(obj);

    delete self->points;
    self->points = NULL;

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
calf_curve_class_init (CalfCurveClass *klass)
{
    parent_class = g_type_class_peek_parent (klass);

    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot       = calf_curve_snapshot;
    widget_class->measure        = calf_curve_measure;
    widget_class->size_allocate  = calf_curve_size_allocate;

    G_OBJECT_CLASS(klass)->finalize = calf_curve_finalize;
}

static void
calf_curve_init (CalfCurve *self)
{
    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_focusable(widget, TRUE);
    self->points = new CalfCurve::point_vector;
    // XXXKF: destructor
    self->points->push_back(CalfCurve::point(0.f, 1.f));
    self->points->push_back(CalfCurve::point(1.f, 1.f));
    self->x0 = 0.f;
    self->x1 = 1.f;
    self->y0 = 1.f;
    self->y1 = 0.f;
    self->cur_pt = -1;
    self->hide_current = false;
    self->pencil_cursor = gdk_cursor_new_from_name("pencil", NULL);
    self->hand_cursor   = gdk_cursor_new_from_name("fleur", NULL);
    self->arrow_cursor  = gdk_cursor_new_from_name("default", NULL);

    /* Button (press + release) controller */
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click, "pressed",  G_CALLBACK(calf_curve_gesture_pressed),  widget);
    g_signal_connect(click, "released", G_CALLBACK(calf_curve_gesture_released), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));

    /* Motion controller */
    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion", G_CALLBACK(calf_curve_motion), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(motion));
}

void CalfCurve::log2phys(float &x, float &y) {
    int width  = gtk_widget_get_width(&parent);
    int height = gtk_widget_get_height(&parent);
    x = (x - x0) / (x1 - x0) * (width - 2) + 1;
    y = (y - y0) / (y1 - y0) * (height - 2) + 1;
}

void CalfCurve::phys2log(float &x, float &y) {
    int width  = gtk_widget_get_width(&parent);
    int height = gtk_widget_get_height(&parent);
    x = x0 + (x - 1) * (x1 - x0) / (width - 2);
    y = y0 + (y - 1) * (y1 - y0) / (height - 2);
}

void CalfCurve::clip(int pt, float &x, float &y, bool &hide)
{
    hide = false;
    sink->clip(this, pt, x, y, hide);

    float ymin = std::min(y0, y1), ymax = std::max(y0, y1);
    float yamp = ymax - ymin;
    if (pt != 0 && pt != (int)(points->size() - 1))
    {
        if (y < ymin - yamp || y > ymax + yamp)
            hide = true;
    }
    if (x < x0) x = x0;
    if (y < ymin) y = ymin;
    if (x > x1) x = x1;
    if (y > ymax) y = ymax;
    if (pt == 0) x = 0;
    if (pt == (int)(points->size() - 1))
        x = (*points)[pt].first;
    if (pt > 0 && x < (*points)[pt - 1].first)
        x = (*points)[pt - 1].first;
    if (pt < (int)(points->size() - 1) && x > (*points)[pt + 1].first)
        x = (*points)[pt + 1].first;
}

void calf_curve_set_points(GtkWidget *widget, const CalfCurve::point_vector &src)
{
    g_assert(CALF_IS_CURVE(widget));
    CalfCurve *self = CALF_CURVE(widget);
    if (self->points->size() != src.size())
        self->cur_pt = -1;
    *self->points = src;

    gtk_widget_queue_draw(widget);
}

GType
calf_curve_get_type (void)
{
    static GType type = 0;
    if (!type) {

        static const GTypeInfo type_info = {
            sizeof(CalfCurveClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)calf_curve_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(CalfCurve),
            0,    /* n_preallocs */
            (GInstanceInitFunc)calf_curve_init
        };

        for (int i = 0; ; i++) {
            const char *name = "CalfCurve";
            //char *name = g_strdup_printf("CalfCurve%u%d",
                //((unsigned int)(intptr_t)calf_curve_class_init) >> 16, i);
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
