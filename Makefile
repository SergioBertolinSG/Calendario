CC := gcc
PKGS := gtk4 json-glib-1.0
CFLAGS := -Wall -Wextra -std=c11
TARGET := calendario
SRC := src/main.c src/date_utils.c src/paths.c src/app_state.c src/ui.c

.PHONY: all check-deps run clean

all: check-deps $(TARGET)

check-deps:
	@command -v pkg-config >/dev/null 2>&1 || { echo "Missing dependency: pkg-config"; exit 1; }
	@pkg-config --exists $(PKGS) || { echo "Missing development packages for: $(PKGS)"; echo "Ubuntu: sudo apt install build-essential pkg-config libgtk-4-dev libjson-glib-dev curl"; exit 1; }

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $$(pkg-config --cflags $(PKGS)) -o $@ $(SRC) $$(pkg-config --libs $(PKGS))

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
