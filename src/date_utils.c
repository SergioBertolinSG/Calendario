#include "date_utils.h"

#include <stdio.h>

const char *CALENDARIO_WEEKDAYS[] = {
    "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO", "DOMINGO"
};

const char *CALENDARIO_MONTHS[] = {
    "ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO",
    "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"
};

int calendario_date_key(int year, int month, int day) {
    return (year * 10000) + (month * 100) + day;
}

gboolean calendario_parse_date_key(const char *text, int base_year, int *year, int *month, int *day) {
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

void calendario_format_date_key(int key, char *buffer, gsize size) {
    int year = key / 10000;
    int month = (key / 100) % 100;
    int day = key % 100;

    g_snprintf(buffer, size, "%04d-%02d-%02d", year, month, day);
}

int calendario_current_year(void) {
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

int calendario_days_in_month(int year, int month) {
    return g_date_get_days_in_month((GDateMonth)month, year);
}

int calendario_first_weekday_column(int year, int month) {
    GDate date;

    g_date_clear(&date, 1);
    g_date_set_dmy(&date, 1, (GDateMonth)month, year);

    return (int)g_date_get_weekday(&date) - 1;
}

int calendario_cells_for_month(int year, int month) {
    int occupied_cells = calendario_first_weekday_column(year, month) + calendario_days_in_month(year, month);

    return ((occupied_cells + 6) / 7) * 7;
}

gboolean calendario_is_current_date(int year, int month, int day) {
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

int calendario_initial_display_index(int year) {
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

void calendario_display_date_from_index(int base_year, int display_index, int *year, int *month) {
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
