#ifndef CALENDARIO_APP_STATE_H
#define CALENDARIO_APP_STATE_H

#include <glib.h>

#define CALENDARIO_MAX_NOTE_LENGTH 48

typedef struct {
    int year;
    GHashTable *notes;
    GHashTable *holidays;
    GHashTable *holiday_regions;
    char *notes_path;
    char *holidays_path;
} AppState;

AppState *app_state_new(void);
void app_state_free(AppState *state);
void app_state_save_notes(AppState *state);

#endif
