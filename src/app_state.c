#include "app_state.h"

#include "date_utils.h"
#include "paths.h"

#include <json-glib/json-glib.h>
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

static char *build_regions_text(JsonObject *holiday) {
    JsonNode *counties_node;
    JsonArray *counties;
    GString *builder;
    guint length;

    counties_node = json_object_get_member(holiday, "counties");
    if (counties_node == NULL || !JSON_NODE_HOLDS_ARRAY(counties_node)) {
        return NULL;
    }

    counties = json_node_get_array(counties_node);
    length = json_array_get_length(counties);
    if (length == 0) {
        return NULL;
    }

    builder = g_string_new(NULL);
    for (guint i = 0; i < length; i++) {
        JsonNode *code_node = json_array_get_element(counties, i);
        const char *code;

        if (code_node == NULL || !JSON_NODE_HOLDS_VALUE(code_node)) {
            continue;
        }

        code = json_node_get_string(code_node);
        if (code == NULL || code[0] == '\0') {
            continue;
        }

        if (builder->len > 0) {
            g_string_append(builder, ", ");
        }
        g_string_append(builder, region_name_from_code(code));
    }

    if (builder->len == 0) {
        g_string_free(builder, TRUE);
        return NULL;
    }

    return g_string_free(builder, FALSE);
}

static void parse_holiday_object(AppState *state, JsonObject *holiday) {
    JsonNode *date_node = json_object_get_member(holiday, "date");
    JsonNode *local_name_node = json_object_get_member(holiday, "localName");
    const char *date;
    const char *local_name;
    char *regions;
    int year = 0;
    int month = 0;
    int day = 0;

    if (date_node == NULL ||
        local_name_node == NULL ||
        !JSON_NODE_HOLDS_VALUE(date_node) ||
        !JSON_NODE_HOLDS_VALUE(local_name_node)) {
        return;
    }

    date = json_node_get_string(date_node);
    local_name = json_node_get_string(local_name_node);
    if (date == NULL || local_name == NULL || local_name[0] == '\0') {
        return;
    }

    if (sscanf(date, "%d-%d-%d", &year, &month, &day) == 3 &&
        year >= state->year - 1 &&
        year <= state->year + 1 &&
        month >= 1 &&
        month <= 12 &&
        day >= 1 &&
        day <= calendario_days_in_month(year, month)) {
        int key = calendario_date_key(year, month, day);

        g_hash_table_replace(state->holidays, GINT_TO_POINTER(key), g_strdup(local_name));
        regions = build_regions_text(holiday);
        if (regions != NULL) {
            g_hash_table_replace(state->holiday_regions, GINT_TO_POINTER(key), regions);
        }
    }
}

static void parse_holidays_json(AppState *state, const char *json) {
    JsonParser *parser = json_parser_new();
    JsonNode *root;
    JsonArray *holidays;
    GError *error = NULL;

    if (!json_parser_load_from_data(parser, json, -1, &error)) {
        g_printerr("No se pudieron leer los festivos descargados: %s\n", error->message);
        g_clear_error(&error);
        g_object_unref(parser);
        return;
    }

    root = json_parser_get_root(parser);
    if (root == NULL || !JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(parser);
        return;
    }

    holidays = json_node_get_array(root);
    for (guint i = 0; i < json_array_get_length(holidays); i++) {
        JsonNode *holiday_node = json_array_get_element(holidays, i);

        if (holiday_node != NULL && JSON_NODE_HOLDS_OBJECT(holiday_node)) {
            parse_holiday_object(state, json_node_get_object(holiday_node));
        }
    }

    g_object_unref(parser);
}

static char *resolve_curl_path(void) {
    const char *snap = g_getenv("SNAP");
    const char *candidates[] = {
        "/usr/bin/curl",
        "/bin/curl",
        NULL
    };

    if (snap != NULL && snap[0] != '\0') {
        char *snap_curl = g_build_filename(snap, "usr", "bin", "curl", NULL);

        if (g_file_test(snap_curl, G_FILE_TEST_IS_EXECUTABLE)) {
            return snap_curl;
        }
        g_free(snap_curl);
    }

    for (int i = 0; candidates[i] != NULL; i++) {
        if (g_file_test(candidates[i], G_FILE_TEST_IS_EXECUTABLE)) {
            return g_strdup(candidates[i]);
        }
    }

    return NULL;
}

static gboolean fetch_holidays_for_year(AppState *state, int year, gboolean clear_before_parse) {
    char *url = g_strdup_printf("https://date.nager.at/api/v3/publicholidays/%d/ES", year);
    char *curl_path = resolve_curl_path();
    char *argv[] = {curl_path, "-fsSL", "--max-time", "5", url, NULL};
    char *stdout_data = NULL;
    char *stderr_data = NULL;
    int exit_status = 0;
    GError *error = NULL;

    if (curl_path == NULL) {
        g_printerr("No se pudieron descargar los festivos: no se encontro curl\n");
        g_free(url);
        return FALSE;
    }

    if (!g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, &stdout_data, &stderr_data, &exit_status, &error)) {
        g_printerr("No se pudieron descargar los festivos: %s\n", error != NULL ? error->message : "error desconocido");
        g_clear_error(&error);
        g_free(stderr_data);
        g_free(curl_path);
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
    g_free(curl_path);
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
