#include "app_state.h"
#include "ui.h"

#include <gtk/gtk.h>

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.cosas.calendario", G_APPLICATION_DEFAULT_FLAGS);
    AppState *state = app_state_new();
    int status;

    g_signal_connect(app, "activate", G_CALLBACK(calendario_activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    app_state_free(state);
    g_object_unref(app);

    return status;
}
