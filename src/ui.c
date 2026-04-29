#include "ui.h"

#include "date_utils.h"
#include "paths.h"

typedef struct {
    int year;
    int month;
    int day;
    gboolean sunday;
    gboolean holiday;
    gboolean regional;
    gboolean today;
    const char *holiday_name;
    const char *holiday_region;
} DayEntry;

typedef struct {
    AppState *state;
    int year;
    int month;
    int day;
    GtkWidget *label;
} DayWidget;

typedef struct {
    AppState *state;
    int display_index;
    GtkWidget *frame;
    GtkWidget *grid;
    GtkWidget *legend;
    GtkWidget *year_label;
    GtkWidget *month_label;
    GtkWidget *prev_button;
    GtkWidget *next_button;
} CalendarView;

typedef struct {
    DayWidget *day_widget;
    GtkWidget *window;
    GtkWidget *entry;
} EditDialogData;

static void render_month(CalendarView *view);

static void update_user_note_label(DayWidget *day_widget) {
    const char *note = g_hash_table_lookup(
        day_widget->state->notes,
        GINT_TO_POINTER(calendario_date_key(day_widget->year, day_widget->month, day_widget->day))
    );

    gtk_label_set_text(GTK_LABEL(day_widget->label), note != NULL ? note : "");
    gtk_widget_set_visible(day_widget->label, note != NULL && note[0] != '\0');
}

static void save_note(EditDialogData *data) {
    const char *note = gtk_editable_get_text(GTK_EDITABLE(data->entry));
    char *trimmed = g_strdup(note);

    g_strstrip(trimmed);
    if (g_utf8_strlen(trimmed, -1) > CALENDARIO_MAX_NOTE_LENGTH) {
        char *end = g_utf8_offset_to_pointer(trimmed, CALENDARIO_MAX_NOTE_LENGTH);
        *end = '\0';
    }

    if (trimmed[0] == '\0') {
        g_hash_table_remove(
            data->day_widget->state->notes,
            GINT_TO_POINTER(calendario_date_key(data->day_widget->year, data->day_widget->month, data->day_widget->day))
        );
        g_free(trimmed);
    } else {
        g_hash_table_replace(
            data->day_widget->state->notes,
            GINT_TO_POINTER(calendario_date_key(data->day_widget->year, data->day_widget->month, data->day_widget->day)),
            trimmed
        );
    }

    update_user_note_label(data->day_widget);
    app_state_save_notes(data->day_widget->state);
}

static void on_note_window_destroy(GtkWidget *window, gpointer user_data) {
    (void)window;
    g_free(user_data);
}

static void on_note_cancel_clicked(GtkButton *button, gpointer user_data) {
    EditDialogData *data = user_data;

    (void)button;
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_note_save_clicked(GtkButton *button, gpointer user_data) {
    EditDialogData *data = user_data;

    (void)button;
    save_note(data);
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_note_entry_activate(GtkEntry *entry, gpointer user_data) {
    EditDialogData *data = user_data;

    (void)entry;
    save_note(data);
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_day_clicked(GtkButton *button, gpointer user_data) {
    DayWidget *day_widget = user_data;
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    GtkWidget *window = gtk_window_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *title = gtk_label_new(NULL);
    GtkWidget *entry = gtk_entry_new();
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *cancel = gtk_button_new_with_label("Cancelar");
    GtkWidget *save = gtk_button_new_with_label("Guardar");
    EditDialogData *data = g_new0(EditDialogData, 1);
    const char *current_note = g_hash_table_lookup(
        day_widget->state->notes,
        GINT_TO_POINTER(calendario_date_key(day_widget->year, day_widget->month, day_widget->day))
    );
    char title_text[64];

    gtk_window_set_title(GTK_WINDOW(window), "Nota del dia");
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), parent);

    g_snprintf(
        title_text,
        sizeof(title_text),
        "%d de %s de %d",
        day_widget->day,
        CALENDARIO_MONTHS[day_widget->month - 1],
        day_widget->year
    );
    gtk_label_set_text(GTK_LABEL(title), title_text);
    gtk_widget_add_css_class(title, "dialog-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);

    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Escribe una nota");
    gtk_entry_set_max_length(GTK_ENTRY(entry), CALENDARIO_MAX_NOTE_LENGTH);
    gtk_editable_set_text(GTK_EDITABLE(entry), current_note != NULL ? current_note : "");
    gtk_widget_set_hexpand(entry, TRUE);

    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(actions), cancel);
    gtk_box_append(GTK_BOX(actions), save);

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), actions);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);

    data->day_widget = day_widget;
    data->window = window;
    data->entry = entry;
    g_signal_connect(window, "destroy", G_CALLBACK(on_note_window_destroy), data);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_note_cancel_clicked), data);
    g_signal_connect(save, "clicked", G_CALLBACK(on_note_save_clicked), data);
    g_signal_connect(entry, "activate", G_CALLBACK(on_note_entry_activate), data);

    gtk_window_set_default_size(GTK_WINDOW(window), 360, 140);
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

static GtkWidget *build_day_card(AppState *state, const DayEntry *entry) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *number = gtk_label_new(NULL);
    GtkWidget *fixed_note = gtk_label_new(NULL);
    GtkWidget *user_note = gtk_label_new(NULL);
    char text[8];

    gtk_widget_add_css_class(card, "day-card");

    if (entry->day == 0) {
        gtk_widget_add_css_class(card, "day-card-empty");
        gtk_widget_set_hexpand(card, TRUE);
        gtk_widget_set_vexpand(card, TRUE);
        return card;
    }

    GtkWidget *button = gtk_button_new();
    DayWidget *day_widget = g_new0(DayWidget, 1);

    gtk_widget_add_css_class(button, "day-button");
    gtk_button_set_child(GTK_BUTTON(button), card);
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);

    if (entry->regional) {
        gtk_widget_add_css_class(card, "regional");
    } else if (entry->holiday) {
        gtk_widget_add_css_class(card, "accent");
    } else if (entry->sunday) {
        gtk_widget_add_css_class(card, "sunday");
    }
    if (entry->today) {
        gtk_widget_add_css_class(card, "today");
    }

    g_snprintf(text, sizeof(text), "%d", entry->day);
    gtk_label_set_markup(GTK_LABEL(number), text);
    gtk_widget_add_css_class(number, "day-number");
    gtk_label_set_xalign(GTK_LABEL(number), 0.0f);

    gtk_label_set_text(GTK_LABEL(fixed_note), entry->holiday_name != NULL ? entry->holiday_name : "");
    gtk_widget_add_css_class(fixed_note, "day-note");
    gtk_label_set_xalign(GTK_LABEL(fixed_note), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(fixed_note), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(fixed_note), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(fixed_note), 14);
    gtk_label_set_lines(GTK_LABEL(fixed_note), 2);
    gtk_label_set_ellipsize(GTK_LABEL(fixed_note), PANGO_ELLIPSIZE_END);
    gtk_widget_set_visible(fixed_note, entry->holiday_name != NULL && entry->holiday_name[0] != '\0');

    gtk_widget_add_css_class(user_note, "user-note");
    gtk_label_set_xalign(GTK_LABEL(user_note), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(user_note), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(user_note), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(user_note), 14);
    gtk_label_set_lines(GTK_LABEL(user_note), 2);
    gtk_label_set_ellipsize(GTK_LABEL(user_note), PANGO_ELLIPSIZE_END);

    gtk_box_append(GTK_BOX(card), number);
    gtk_box_append(GTK_BOX(card), fixed_note);
    gtk_box_append(GTK_BOX(card), user_note);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_set_vexpand(card, TRUE);

    day_widget->state = state;
    day_widget->year = entry->year;
    day_widget->month = entry->month;
    day_widget->day = entry->day;
    day_widget->label = user_note;
    g_object_set_data_full(G_OBJECT(button), "day-widget", day_widget, g_free);
    update_user_note_label(day_widget);
    g_signal_connect(button, "clicked", G_CALLBACK(on_day_clicked), day_widget);

    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_vexpand(button, TRUE);

    return button;
}

static DayEntry build_entry_for_cell(AppState *state, int year, int month, int cell) {
    int first_column = calendario_first_weekday_column(year, month);
    int day = cell - first_column + 1;
    int column = cell % 7;
    const char *holiday_name = NULL;
    const char *holiday_region = NULL;
    int month_days = calendario_days_in_month(year, month);

    if (day < 1 || day > month_days) {
        return (DayEntry){year, month, 0, FALSE, FALSE, FALSE, FALSE, NULL, NULL};
    }

    holiday_name = g_hash_table_lookup(state->holidays, GINT_TO_POINTER(calendario_date_key(year, month, day)));
    holiday_region = g_hash_table_lookup(state->holiday_regions, GINT_TO_POINTER(calendario_date_key(year, month, day)));

    return (DayEntry){
        year,
        month,
        day,
        column == 6,
        holiday_name != NULL,
        holiday_region != NULL,
        calendario_is_current_date(year, month, day),
        holiday_name,
        holiday_region
    };
}

static GtkWidget *build_calendar_grid(AppState *state, int year, int month) {
    GtkWidget *grid = gtk_grid_new();
    int calendar_cells = calendario_cells_for_month(year, month);

    gtk_widget_add_css_class(grid, "calendar-grid");
    if (calendar_cells > 35) {
        gtk_widget_add_css_class(grid, "six-week-month");
    }
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    for (int column = 0; column < 7; column++) {
        GtkWidget *header = gtk_label_new(CALENDARIO_WEEKDAYS[column]);
        gtk_widget_add_css_class(header, "weekday-header");
        if (column == 6) {
            gtk_widget_add_css_class(header, "sunday-header");
        }
        gtk_grid_attach(GTK_GRID(grid), header, column, 0, 1, 1);
    }

    for (int i = 0; i < calendar_cells; i++) {
        DayEntry entry = build_entry_for_cell(state, year, month, i);
        GtkWidget *card = build_day_card(state, &entry);
        gtk_grid_attach(GTK_GRID(grid), card, i % 7, (i / 7) + 1, 1, 1);
    }

    return grid;
}

static GtkWidget *build_month_legend(AppState *state, int year, int month) {
    GtkWidget *legend = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *title = gtk_label_new("Festivos regionales");
    gboolean has_items = FALSE;
    int month_days = calendario_days_in_month(year, month);

    gtk_widget_add_css_class(legend, "holiday-legend");
    gtk_widget_add_css_class(title, "holiday-legend-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(legend), title);

    for (int day = 1; day <= month_days; day++) {
        int key = calendario_date_key(year, month, day);
        const char *name = g_hash_table_lookup(state->holidays, GINT_TO_POINTER(key));
        const char *region = g_hash_table_lookup(state->holiday_regions, GINT_TO_POINTER(key));

        if (name != NULL && region != NULL) {
            char text[512];
            GtkWidget *item;

            g_snprintf(text, sizeof(text), "%d %s %d: %s - %s", day, CALENDARIO_MONTHS[month - 1], year, name, region);
            item = gtk_label_new(text);
            gtk_widget_add_css_class(item, "holiday-legend-item");
            gtk_label_set_xalign(GTK_LABEL(item), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(item), TRUE);
            gtk_box_append(GTK_BOX(legend), item);
            has_items = TRUE;
        }
    }

    if (!has_items) {
        GtkWidget *empty = gtk_label_new("Sin festivos regionales este mes");

        gtk_widget_add_css_class(empty, "holiday-legend-empty");
        gtk_label_set_xalign(GTK_LABEL(empty), 0.0f);
        gtk_box_append(GTK_BOX(legend), empty);
    }

    return legend;
}

static void render_month(CalendarView *view) {
    int year = 0;
    int month = 0;
    char year_text[8];

    if (view->grid != NULL) {
        gtk_box_remove(GTK_BOX(view->frame), view->grid);
    }
    if (view->legend != NULL) {
        gtk_box_remove(GTK_BOX(view->frame), view->legend);
    }

    calendario_display_date_from_index(view->state->year, view->display_index, &year, &month);
    g_snprintf(year_text, sizeof(year_text), "%d", year);

    gtk_label_set_text(GTK_LABEL(view->year_label), year_text);
    gtk_label_set_text(GTK_LABEL(view->month_label), CALENDARIO_MONTHS[month - 1]);
    gtk_widget_set_sensitive(view->prev_button, view->display_index > 0);
    gtk_widget_set_sensitive(view->next_button, view->display_index < 13);

    view->grid = build_calendar_grid(view->state, year, month);
    view->legend = build_month_legend(view->state, year, month);
    gtk_box_append(GTK_BOX(view->frame), view->grid);
    gtk_box_append(GTK_BOX(view->frame), view->legend);
}

static void on_prev_month_clicked(GtkButton *button, gpointer user_data) {
    CalendarView *view = user_data;

    (void)button;
    if (view->display_index > 0) {
        view->display_index--;
        render_month(view);
    }
}

static void on_next_month_clicked(GtkButton *button, gpointer user_data) {
    CalendarView *view = user_data;

    (void)button;
    if (view->display_index < 13) {
        view->display_index++;
        render_month(view);
    }
}

static gboolean on_calendar_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
    CalendarView *view = user_data;

    (void)controller;
    (void)dx;

    if (dy < 0.0 && view->display_index > 0) {
        view->display_index--;
        render_month(view);
        return TRUE;
    }

    if (dy > 0.0 && view->display_index < 13) {
        view->display_index++;
        render_month(view);
        return TRUE;
    }

    return FALSE;
}

static GtkWidget *build_header(CalendarView *view) {
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    char year_text[8];
    GtkWidget *year = NULL;
    GtkWidget *month = gtk_label_new(NULL);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *summary = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *mini_title = gtk_label_new("MES");
    GtkWidget *nav = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *prev = gtk_button_new_with_label("<");
    GtkWidget *next = gtk_button_new_with_label(">");

    g_snprintf(year_text, sizeof(year_text), "%d", view->state->year);
    year = gtk_label_new(year_text);

    gtk_widget_add_css_class(header, "hero");
    gtk_widget_add_css_class(year, "year-label");
    gtk_widget_add_css_class(month, "month-label");
    gtk_widget_add_css_class(spacer, "hero-spacer");
    gtk_widget_add_css_class(summary, "hero-summary");
    gtk_widget_add_css_class(mini_title, "mini-title");
    gtk_widget_add_css_class(prev, "month-nav-button");
    gtk_widget_add_css_class(next, "month-nav-button");

    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_widget_set_halign(summary, GTK_ALIGN_END);

    view->month_label = month;
    view->year_label = year;
    view->prev_button = prev;
    view->next_button = next;

    gtk_box_append(GTK_BOX(nav), prev);
    gtk_box_append(GTK_BOX(nav), next);
    gtk_box_append(GTK_BOX(summary), mini_title);
    gtk_box_append(GTK_BOX(summary), nav);
    gtk_box_append(GTK_BOX(header), year);
    gtk_box_append(GTK_BOX(header), month);
    gtk_box_append(GTK_BOX(header), spacer);
    gtk_box_append(GTK_BOX(header), summary);

    g_signal_connect(prev, "clicked", G_CALLBACK(on_prev_month_clicked), view);
    g_signal_connect(next, "clicked", G_CALLBACK(on_next_month_clicked), view);

    return header;
}

static void free_calendar_view(GtkWidget *window, gpointer user_data) {
    (void)window;
    g_free(user_data);
}

void calendario_activate(GtkApplication *app, gpointer user_data) {
    AppState *state = user_data;
    CalendarView *view = g_new0(CalendarView, 1);
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css_path = calendario_build_css_path();

    gtk_window_set_title(GTK_WINDOW(window), "Calendario");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 820);

    gtk_css_provider_load_from_path(provider, css_path);
    g_free(css_path);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    gtk_widget_add_css_class(window, "app-window");
    gtk_widget_add_css_class(outer, "app-shell");
    gtk_widget_add_css_class(frame, "calendar-frame");
    gtk_widget_set_hexpand(outer, TRUE);
    gtk_widget_set_vexpand(outer, TRUE);
    gtk_widget_set_hexpand(frame, TRUE);
    gtk_widget_set_vexpand(frame, TRUE);

    view->state = state;
    view->display_index = calendario_initial_display_index(state->year);
    view->frame = frame;

    gtk_event_controller_set_propagation_phase(scroll_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(on_calendar_scroll), view);
    gtk_widget_add_controller(window, scroll_controller);

    gtk_box_append(GTK_BOX(frame), build_header(view));
    render_month(view);
    gtk_box_append(GTK_BOX(outer), frame);

    g_signal_connect(window, "destroy", G_CALLBACK(free_calendar_view), view);

    gtk_window_set_child(GTK_WINDOW(window), outer);
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_window_present(GTK_WINDOW(window));
}
