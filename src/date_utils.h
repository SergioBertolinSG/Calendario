#ifndef CALENDARIO_DATE_UTILS_H
#define CALENDARIO_DATE_UTILS_H

#include <glib.h>

extern const char *CALENDARIO_WEEKDAYS[];
extern const char *CALENDARIO_MONTHS[];

int calendario_date_key(int year, int month, int day);
gboolean calendario_parse_date_key(const char *text, int base_year, int *year, int *month, int *day);
void calendario_format_date_key(int key, char *buffer, gsize size);
int calendario_current_year(void);
int calendario_days_in_month(int year, int month);
int calendario_first_weekday_column(int year, int month);
int calendario_cells_for_month(int year, int month);
gboolean calendario_is_current_date(int year, int month, int day);
int calendario_initial_display_index(int year);
void calendario_display_date_from_index(int base_year, int display_index, int *year, int *month);

#endif
