#include "app_state.h"

#include "date_utils.h"
#include "paths.h"

#include <stdio.h>

static const char *region_name_from_code(const char *code) {
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

static void load_key_file_table(GHashTable *table, const char *path, const char *group, int year) {
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

            if (calendario_parse_date_key(keys[i], year, &item_year, &month, &day) &&
                item_year >= year - 1 &&
                item_year <= year + 1 &&
                month >= 1 &&
                month <= 12 &&
                day >= 1 &&
                day <= calendario_days_in_month(item_year, month) &&
                value != NULL &&
                value[0] != '\0') {
                g_hash_table_insert(table, GINT_TO_POINTER(calendario_date_key(item_year, month, day)), value);
            } else {
                g_free(value);
            }
        }
        g_strfreev(keys);
    }

    g_key_file_unref(key_file);
}

static void save_key_file_table(GHashTable *table, const char *path, const char *group) {
    GKeyFile *key_file = g_key_file_new();
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    gsize data_length = 0;
    char *data;

    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        calendario_format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, group, item_key, value);
    }

    data = g_key_file_to_data(key_file, &data_length, NULL);
    if (data != NULL) {
        g_file_set_contents(path, data, (gssize)data_length, NULL);
    }

    g_free(data);
    g_key_file_unref(key_file);
}

static void save_holidays(AppState *state) {
    GKeyFile *key_file = g_key_file_new();
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    gsize data_length = 0;
    char *data;

    g_hash_table_iter_init(&iter, state->holidays);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        calendario_format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, "holidays", item_key, value);
    }

    g_hash_table_iter_init(&iter, state->holiday_regions);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        char item_key[16];

        calendario_format_date_key(GPOINTER_TO_INT(key), item_key, sizeof(item_key));
        g_key_file_set_string(key_file, "regions", item_key, value);
    }

    data = g_key_file_to_data(key_file, &data_length, NULL);
    if (data != NULL) {
        g_file_set_contents(state->holidays_path, data, (gssize)data_length, NULL);
    }

    g_free(data);
    g_key_file_unref(key_file);
}

static char *extract_json_string(const char *object, const char *field) {
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

static char *extract_json_regions(const char *object) {
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

static void parse_holidays_json(AppState *state, const char *json) {
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
                day <= calendario_days_in_month(year, month)) {
                int key = calendario_date_key(year, month, day);

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

static gboolean fetch_holidays_for_year(AppState *state, int year, gboolean clear_before_parse) {
    char *url = g_strdup_printf("https://date.nager.at/api/v3/publicholidays/%d/ES", year);
    char **env = g_get_environ();
    char *argv[] = {"curl", "-fsSL", "--max-time", "5", url, NULL};
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

static void fetch_holidays(AppState *state) {
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

AppState *app_state_new(void) {
    AppState *state = g_new0(AppState, 1);
    char notes_file[32];
    char holidays_file[32];

    state->year = calendario_current_year();
    state->notes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    state->holidays = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    state->holiday_regions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    g_snprintf(notes_file, sizeof(notes_file), "notas-%d.ini", state->year);
    g_snprintf(holidays_file, sizeof(holidays_file), "festivos-%d.ini", state->year);
    state->notes_path = calendario_build_config_path(notes_file);
    state->holidays_path = calendario_build_config_path(holidays_file);

    load_key_file_table(state->notes, state->notes_path, "notes", state->year);
    load_key_file_table(state->holidays, state->holidays_path, "holidays", state->year);
    load_key_file_table(state->holiday_regions, state->holidays_path, "regions", state->year);
    fetch_holidays(state);

    return state;
}

void app_state_free(AppState *state) {
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

void app_state_save_notes(AppState *state) {
    save_key_file_table(state->notes, state->notes_path, "notes");
}
