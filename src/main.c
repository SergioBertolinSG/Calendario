#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#define MAX_NOTE_LENGTH 48

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
    int year;
    GHashTable *notes;
    GHashTable *holidays;
    GHashTable *holiday_regions;
    char *notes_path;
    char *holidays_path;
} AppState;

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

static const char *WEEKDAYS[] = {
    "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO", "DOMINGO"
};

static const char *MONTHS[] = {
    "ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO",
    "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"
};

static int date_key(int year, int month, int day)
{
    return (year * 10000) + (month * 100) + day;
}

static gboolean parse_date_key(const char *text, int base_year, int *year, int *month, int *day)
{
    if (sscanf(text, "%d-%d-%d", year, month, day) == 3) {
        return TRUE;
    }

    if (sscanf(text, "%d-%d", month, day) == 2) {
        *year = base_year;
        return TRUE;
    }

    *year = base_year;
    *month = 1;
    *day = (int)g_ascii_strtoll(text, NULL, 10);
    return TRUE;
}

static void format_date_key(int key, char *buffer, gsize size)
{
    int year = key / 10000;
    int month = (key / 100) % 100;
    int day = key % 100;

    g_snprintf(buffer, size, "%04d-%02d-%02d", year, month, day);
}

static int current_year(void)
{
    const char *year_override = g_getenv("CALENDARIO_YEAR");
    GDateTime *now = g_date_time_new_now_local();
    int year = 1;

    if (year_override != NULL && year_override[0] != '\0') {
        int parsed_year = (int)g_ascii_strtoll(year_override, NULL, 10);

        if (parsed_year >= 1 && parsed_year <= 9999) {
            if (now != NULL) {
                g_date_time_unref(now);
            }
            return parsed_year;
        }
    }

    if (now != NULL) {
        year = g_date_time_get_year(now);
        g_date_time_unref(now);
    }

    return year;
}

static const char *region_name_from_code(const char *code)
{
    if (g_strcmp0(code, "ES-AN") == 0) return "Andalucia";
    if (g_strcmp0(code, "ES-AR") == 0) return "Aragon";
    if (g_strcmp0(code, "ES-AS") == 0) return "Asturias";
    if (g_strcmp0(code, "ES-CB") == 0) return "Cantabria";
    if (g_strcmp0(code, "ES-CE") == 0) return "Ceuta";
    if (g_strcmp0(code, "ES-CL") == 0) return "Castilla y Leon";
    if (g_strcmp0(code, "ES-CM") == 0) return "Castilla-La Mancha";
    if (g_strcmp0(code, "ES-CN") == 0) return "Canarias";
    if (g_strcmp0(code, "ES-CT") == 0) return "Cataluna";
    if (g_strcmp0(code, "ES-EX") == 0) return "Extremadura";
    if (g_strcmp0(code, "ES-GA") == 0) return "Galicia";
    if (g_strcmp0(code, "ES-IB") == 0) return "Islas Baleares";
    if (g_strcmp0(code, "ES-MC") == 0) return "Murcia";
    if (g_strcmp0(code, "ES-MD") == 0) return "Madrid";
    if (g_strcmp0(code, "ES-ML") == 0) return "Melilla";
    if (g_strcmp0(code, "ES-NC") == 0) return "Navarra";
    if (g_strcmp0(code, "ES-PV") == 0) return "Pais Vasco";
    if (g_strcmp0(code, "ES-RI") == 0) return "La Rioja";
    if (g_strcmp0(code, "ES-VC") == 0) return "Comunitat Valenciana";

    return code;
}

static int days_in_month(int year, int month)
{
    return g_date_get_days_in_month((GDateMonth)month, year);
}

static int first_weekday_column(int year, int month)
{
    GDate date;

    g_date_clear(&date, 1);
    g_date_set_dmy(&date, 1, (GDateMonth)month, year);

    return (int)g_date_get_weekday(&date) - 1;
}

static int calendar_cells_for_month(int year, int month)
{
    int occupied_cells = first_weekday_column(year, month) + days_in_month(year, month);

    return ((occupied_cells + 6) / 7) * 7;
}

static gboolean is_current_date(int year, int month, int day)
{
    GDateTime *now = g_date_time_new_now_local();
    gboolean matches = FALSE;

    if (now != NULL) {
        matches = g_date_time_get_year(now) == year &&
            g_date_time_get_month(now) == month &&
            g_date_time_get_day_of_month(now) == day;
        g_date_time_unref(now);
    }

    return matches;
}

static int initial_month(int year)
{
    GDateTime *now = g_date_time_new_now_local();
    int month = 1;

    if (now != NULL) {
        if (g_date_time_get_year(now) == year) {
            month = g_date_time_get_month(now);
        }
        g_date_time_unref(now);
    }

    return month;
}

static void display_date_from_index(int base_year, int display_index, int *year, int *month)
{
    if (display_index == 0) {
        *year = base_year - 1;
        *month = 12;
    } else if (display_index == 13) {
        *year = base_year + 1;
        *month = 1;
    } else {
        *year = base_year;
        *month = display_index;
    }
}

static char *build_config_path(const char *filename)
{
    const char *snap_user_common = g_getenv("SNAP_USER_COMMON");
    const char *config_dir = snap_user_common != NULL && snap_user_common[0] != '\0'
        ? snap_user_common
        : g_get_user_config_dir();
    char *app_dir = g_build_filename(config_dir, "calendario", NULL);
    char *path = g_build_filename(app_dir, filename, NULL);

    g_mkdir_with_parents(app_dir, 0700);
    g_free(app_dir);

    return path;
}

static char *build_css_path(void)
{
    const char *snap = g_getenv("SNAP");

    if (snap != NULL && snap[0] != '\0') {
        return g_build_filename(snap, "usr", "share", "calendario", "styles", "calendario.css", NULL);
    }

    return g_build_filename("styles", "calendario.css", NULL);
}

static void load_key_file_table(GHashTable *table, const char *path, const char *group, int year)
{
    GKeyFile *key_file = g_key_file_new();
    gsize length = 0;
    char **keys;

    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
        g_key_file_unref(key_file);
        return;
    }

    keys = g_key_file_get_keys(key_file, group, &length, NULL);
    if (keys != NULL) {
        for (gsize i = 0; i < length; i++) {
            int item_year = 0;
            int month = 0;
            int day = 0;
            char *value = g_key_file_get_string(key_file, group, keys[i], NULL);

            if (parse_date_key(keys[i], year, &item_year, &month, &day) &&
                item_year >= year - 1 &&
                item_year <= year + 1 &&
                month >= 1 &&
                month <= 12 &&
                day >= 1 &&
                day <= days_in_month(item_year, month) &&
                value != NULL &&
                value[0] != '\0') {
                g_hash_table_insert(table, GINT_TO_POINTER(date_key(item_year, month, day)), value);
            } else {
                g_free(value);
            }
        }
        g_strfreev(keys);
    }

    g_key_file_unref(key_file);
}

static void save_key_file_table(GHashTable *table, const char *path, const char *group)
{
    GKeyFile *key_file = g_key_file_new();
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    gsize data_length = 0;
    char *data;

    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, group, item_key, value);
    }

    data = g_key_file_to_data(key_file, &data_length, NULL);
    if (data != NULL) {
        g_file_set_contents(path, data, (gssize)data_length, NULL);
    }

    g_free(data);
    g_key_file_unref(key_file);
}

static void save_holidays(AppState *state)
{
    GKeyFile *key_file = g_key_file_new();
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    gsize data_length = 0;
    char *data;

    g_hash_table_iter_init(&iter, state->holidays);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, "holidays", item_key, value);
    }

    g_hash_table_iter_init(&iter, state->holiday_regions);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, "regions", item_key, value);
    }

    data = g_key_file_to_data(key_file, &data_length, NULL);
    if (data != NULL) {
        g_file_set_contents(state->holidays_path, data, (gssize)data_length, NULL);
    }

    g_free(data);
    g_key_file_unref(key_file);
}

static char *extract_json_string(const char *object, const char *field)
{
    char *pattern = g_strdup_printf("\"%s\"[[:space:]]*:[[:space:]]*\"([^\"]*)\"", field);
    GRegex *regex = g_regex_new(pattern, 0, 0, NULL);
    GMatchInfo *match_info = NULL;
    char *value = NULL;

    if (g_regex_match(regex, object, 0, &match_info)) {
        value = g_match_info_fetch(match_info, 1);
    }

    g_match_info_free(match_info);
    g_regex_unref(regex);
    g_free(pattern);

    return value;
}

static char *extract_json_regions(const char *object)
{
    char *pattern = g_strdup("\"counties\"[[:space:]]*:[[:space:]]*\\[([^]]*)\\]");
    GRegex *regex = g_regex_new(pattern, 0, 0, NULL);
    GMatchInfo *match_info = NULL;
    char *regions = NULL;

    if (g_regex_match(regex, object, 0, &match_info)) {
        char *raw = g_match_info_fetch(match_info, 1);
        GRegex *code_regex = g_regex_new("\"([^\"]+)\"", 0, 0, NULL);
        GMatchInfo *code_info = NULL;
        GString *builder = g_string_new(NULL);

        g_regex_match(code_regex, raw, 0, &code_info);
        while (g_match_info_matches(code_info)) {
            char *code = g_match_info_fetch(code_info, 1);

            if (builder->len > 0) {
                g_string_append(builder, ", ");
            }
            g_string_append(builder, region_name_from_code(code));

            g_free(code);
            g_match_info_next(code_info, NULL);
        }

        if (builder->len > 0) {
            regions = g_string_free(builder, FALSE);
        } else {
            g_string_free(builder, TRUE);
        }

        g_match_info_free(code_info);
        g_regex_unref(code_regex);
        g_free(raw);
    }

    g_match_info_free(match_info);
    g_regex_unref(regex);
    g_free(pattern);

    return regions;
}

static void parse_holidays_json(AppState *state, const char *json)
{
    GRegex *object_regex = g_regex_new("\\{[^}]*\\}", 0, 0, NULL);
    GMatchInfo *match_info = NULL;

    g_regex_match(object_regex, json, 0, &match_info);
    while (g_match_info_matches(match_info)) {
        char *object = g_match_info_fetch(match_info, 0);
        char *date = extract_json_string(object, "date");
        char *local_name = extract_json_string(object, "localName");
        char *regions = extract_json_regions(object);

        if (date != NULL && local_name != NULL) {
            int year = 0;
            int month = 0;
            int day = 0;

            if (sscanf(date, "%d-%d-%d", &year, &month, &day) == 3 &&
                year >= state->year - 1 &&
                year <= state->year + 1 &&
                month >= 1 &&
                month <= 12 &&
                day >= 1 &&
                day <= days_in_month(year, month)) {
                int key = date_key(year, month, day);

                g_hash_table_replace(state->holidays, GINT_TO_POINTER(key), g_strdup(local_name));
                if (regions != NULL && regions[0] != '\0') {
                    g_hash_table_replace(state->holiday_regions, GINT_TO_POINTER(key), g_strdup(regions));
                }
            }
        }

        g_free(regions);
        g_free(local_name);
        g_free(date);
        g_free(object);
        g_match_info_next(match_info, NULL);
    }

    g_match_info_free(match_info);
    g_regex_unref(object_regex);
}

static gboolean fetch_holidays_for_year(AppState *state, int year, gboolean clear_before_parse)
{
    char *url = g_strdup_printf("https://date.nager.at/api/v3/publicholidays/%d/ES", year);
    char **env = g_get_environ();
    char *argv[] = {
        "curl",
        "-fsSL",
        "--max-time",
        "5",
        url,
        NULL
    };
    char *stdout_data = NULL;
    char *stderr_data = NULL;
    int exit_status = 0;
    GError *error = NULL;

    env = g_environ_unsetenv(env, "LD_PRELOAD");
    env = g_environ_unsetenv(env, "FAKETIME");
    env = g_environ_unsetenv(env, "FAKETIME_TIMESTAMP_FILE");
    env = g_environ_unsetenv(env, "FAKETIME_NO_CACHE");

    if (!g_spawn_sync(NULL, argv, env, G_SPAWN_SEARCH_PATH, NULL, NULL, &stdout_data, &stderr_data, &exit_status, &error)) {
        g_printerr("No se pudieron descargar los festivos: %s\n", error != NULL ? error->message : "error desconocido");
        g_clear_error(&error);
        g_free(stderr_data);
        g_strfreev(env);
        g_free(url);
        return FALSE;
    }

    if (g_spawn_check_wait_status(exit_status, NULL) && stdout_data != NULL && stdout_data[0] != '\0') {
        if (clear_before_parse) {
            g_hash_table_remove_all(state->holidays);
            g_hash_table_remove_all(state->holiday_regions);
        }
        parse_holidays_json(state, stdout_data);
    } else if (stderr_data != NULL && stderr_data[0] != '\0') {
        g_printerr("No se pudieron descargar los festivos: %s\n", stderr_data);
    }

    gboolean success = g_spawn_check_wait_status(exit_status, NULL) && stdout_data != NULL && stdout_data[0] != '\0';

    g_free(stdout_data);
    g_free(stderr_data);
    g_strfreev(env);
    g_free(url);

    return success;
}

static void fetch_holidays(AppState *state)
{
    gboolean refreshed = FALSE;

    for (int year = state->year - 1; year <= state->year + 1; year++) {
        if (fetch_holidays_for_year(state, year, !refreshed)) {
            refreshed = TRUE;
        }
    }

    if (refreshed) {
        save_holidays(state);
    }
}

static AppState *app_state_new(void)
{
    AppState *state = g_new0(AppState, 1);
    char notes_file[32];
    char holidays_file[32];

    state->year = current_year();
    state->notes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    state->holidays = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    state->holiday_regions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    g_snprintf(notes_file, sizeof(notes_file), "notas-%d.ini", state->year);
    g_snprintf(holidays_file, sizeof(holidays_file), "festivos-%d.ini", state->year);
    state->notes_path = build_config_path(notes_file);
    state->holidays_path = build_config_path(holidays_file);

    load_key_file_table(state->notes, state->notes_path, "notes", state->year);
    load_key_file_table(state->holidays, state->holidays_path, "holidays", state->year);
    load_key_file_table(state->holiday_regions, state->holidays_path, "regions", state->year);
    fetch_holidays(state);

    return state;
}

static void app_state_free(AppState *state)
{
    if (state == NULL) {
        return;
    }

    g_hash_table_destroy(state->notes);
    g_hash_table_destroy(state->holidays);
    g_hash_table_destroy(state->holiday_regions);
    g_free(state->notes_path);
    g_free(state->holidays_path);
    g_free(state);
}

static void save_notes(AppState *state)
{
    save_key_file_table(state->notes, state->notes_path, "notes");
}

static void update_user_note_label(DayWidget *day_widget)
{
    const char *note = g_hash_table_lookup(
        day_widget->state->notes,
        GINT_TO_POINTER(date_key(day_widget->year, day_widget->month, day_widget->day))
    );

    gtk_label_set_text(GTK_LABEL(day_widget->label), note != NULL ? note : "");
    gtk_widget_set_visible(day_widget->label, note != NULL && note[0] != '\0');
}

static void save_note(EditDialogData *data)
{
    const char *note = gtk_editable_get_text(GTK_EDITABLE(data->entry));
    char *trimmed = g_strdup(note);

    g_strstrip(trimmed);
    if (g_utf8_strlen(trimmed, -1) > MAX_NOTE_LENGTH) {
        char *end = g_utf8_offset_to_pointer(trimmed, MAX_NOTE_LENGTH);
        *end = '\0';
    }

    if (trimmed[0] == '\0') {
        g_hash_table_remove(
            data->day_widget->state->notes,
            GINT_TO_POINTER(date_key(data->day_widget->year, data->day_widget->month, data->day_widget->day))
        );
        g_free(trimmed);
    } else {
        g_hash_table_replace(
            data->day_widget->state->notes,
            GINT_TO_POINTER(date_key(data->day_widget->year, data->day_widget->month, data->day_widget->day)),
            trimmed
        );
    }

    update_user_note_label(data->day_widget);
    save_notes(data->day_widget->state);
}

static void on_note_window_destroy(GtkWidget *window, gpointer user_data)
{
    (void)window;
    g_free(user_data);
}

static void on_note_cancel_clicked(GtkButton *button, gpointer user_data)
{
    EditDialogData *data = user_data;

    (void)button;
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_note_save_clicked(GtkButton *button, gpointer user_data)
{
    EditDialogData *data = user_data;

    (void)button;
    save_note(data);
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_note_entry_activate(GtkEntry *entry, gpointer user_data)
{
    EditDialogData *data = user_data;

    (void)entry;
    save_note(data);
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_day_clicked(GtkButton *button, gpointer user_data)
{
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
        GINT_TO_POINTER(date_key(day_widget->year, day_widget->month, day_widget->day))
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
        MONTHS[day_widget->month - 1],
        day_widget->year
    );
    gtk_label_set_text(GTK_LABEL(title), title_text);
    gtk_widget_add_css_class(title, "dialog-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);

    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Escribe una nota");
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_NOTE_LENGTH);
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

static GtkWidget *build_day_card(AppState *state, const DayEntry *entry)
{
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

static DayEntry build_entry_for_cell(AppState *state, int year, int month, int cell)
{
    int first_column = first_weekday_column(year, month);
    int day = cell - first_column + 1;
    int column = cell % 7;
    const char *holiday_name = NULL;
    const char *holiday_region = NULL;
    int month_days = days_in_month(year, month);

    if (day < 1 || day > month_days) {
        return (DayEntry){year, month, 0, FALSE, FALSE, FALSE, FALSE, NULL, NULL};
    }

    holiday_name = g_hash_table_lookup(state->holidays, GINT_TO_POINTER(date_key(year, month, day)));
    holiday_region = g_hash_table_lookup(state->holiday_regions, GINT_TO_POINTER(date_key(year, month, day)));

    return (DayEntry){
        year,
        month,
        day,
        column == 6,
        holiday_name != NULL,
        holiday_region != NULL,
        is_current_date(year, month, day),
        holiday_name,
        holiday_region
    };
}

static GtkWidget *build_calendar_grid(AppState *state, int year, int month)
{
    GtkWidget *grid = gtk_grid_new();
    int calendar_cells = calendar_cells_for_month(year, month);

    gtk_widget_add_css_class(grid, "calendar-grid");
    if (calendar_cells > 35) {
        gtk_widget_add_css_class(grid, "six-week-month");
    }
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    for (int column = 0; column < 7; column++) {
        GtkWidget *header = gtk_label_new(WEEKDAYS[column]);
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

static GtkWidget *build_month_legend(AppState *state, int year, int month)
{
    GtkWidget *legend = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *title = gtk_label_new("Festivos regionales");
    gboolean has_items = FALSE;
    int month_days = days_in_month(year, month);

    gtk_widget_add_css_class(legend, "holiday-legend");
    gtk_widget_add_css_class(title, "holiday-legend-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(legend), title);

    for (int day = 1; day <= month_days; day++) {
        int key = date_key(year, month, day);
        const char *name = g_hash_table_lookup(state->holidays, GINT_TO_POINTER(key));
        const char *region = g_hash_table_lookup(state->holiday_regions, GINT_TO_POINTER(key));

        if (name != NULL && region != NULL) {
            char text[512];
            GtkWidget *item;

            g_snprintf(text, sizeof(text), "%d %s %d: %s - %s", day, MONTHS[month - 1], year, name, region);
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

static void render_month(CalendarView *view)
{
    int year = 0;
    int month = 0;
    char year_text[8];

    if (view->grid != NULL) {
        gtk_box_remove(GTK_BOX(view->frame), view->grid);
    }
    if (view->legend != NULL) {
        gtk_box_remove(GTK_BOX(view->frame), view->legend);
    }

    display_date_from_index(view->state->year, view->display_index, &year, &month);
    g_snprintf(year_text, sizeof(year_text), "%d", year);

    gtk_label_set_text(GTK_LABEL(view->year_label), year_text);
    gtk_label_set_text(GTK_LABEL(view->month_label), MONTHS[month - 1]);
    gtk_widget_set_sensitive(view->prev_button, view->display_index > 0);
    gtk_widget_set_sensitive(view->next_button, view->display_index < 13);

    view->grid = build_calendar_grid(view->state, year, month);
    view->legend = build_month_legend(view->state, year, month);
    gtk_box_append(GTK_BOX(view->frame), view->grid);
    gtk_box_append(GTK_BOX(view->frame), view->legend);
}

static void on_prev_month_clicked(GtkButton *button, gpointer user_data)
{
    CalendarView *view = user_data;

    (void)button;
    if (view->display_index > 0) {
        view->display_index--;
        render_month(view);
    }
}

static void on_next_month_clicked(GtkButton *button, gpointer user_data)
{
    CalendarView *view = user_data;

    (void)button;
    if (view->display_index < 13) {
        view->display_index++;
        render_month(view);
    }
}

static gboolean on_calendar_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
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

static GtkWidget *build_header(CalendarView *view)
{
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

static void free_calendar_view(GtkWidget *window, gpointer user_data)
{
    (void)window;
    g_free(user_data);
}

static void activate(GtkApplication *app, gpointer user_data)
{
    AppState *state = user_data;
    CalendarView *view = g_new0(CalendarView, 1);
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css_path = build_css_path();

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
    view->display_index = initial_month(state->year);
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

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("com.cosas.calendario", G_APPLICATION_DEFAULT_FLAGS);
    AppState *state = app_state_new();
    int status;

    g_signal_connect(app, "activate", G_CALLBACK(activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    app_state_free(state);
    g_object_unref(app);

    return status;
}
