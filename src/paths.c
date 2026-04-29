#include "paths.h"

#include <glib.h>

char *calendario_build_config_path(const char *filename) {
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

char *calendario_build_css_path(void) {
    const char *snap = g_getenv("SNAP");

    if (snap != NULL && snap[0] != '\0') {
        return g_build_filename(snap, "usr", "share", "calendario", "styles", "calendario.css", NULL);
    }

    return g_build_filename("styles", "calendario.css", NULL);
}
