# Calendario GTK

Calendar with a traditional spanish style showing regional holidays.

## Files

- `src/main.c`: GTK application and calendar layout.
- `styles/calendario.css`: visual theme with large numbers, black/red palette, and poster-style spacing.
- `Makefile`: simple build and run targets.

## Requirements

You need GTK 4 development libraries, JSON-GLib development libraries, `pkg-config`, and `curl`.

Ubuntu:

```sh
sudo apt install build-essential pkg-config libgtk-4-dev libjson-glib-dev curl
```

## Build

```sh
make
```

## Run

```sh
make run
```

## Snap

Build and install locally:

```sh
snapcraft
sudo snap install calendario-es_*.snap --dangerous
calendario-es
```

Click a day to add or edit a note. Notes are restored the next time the app opens. When running as a snap, notes and cached holidays are stored under the snap sandbox user area, usually `~/snap/calendario-es/common/calendario/`. Local development builds still use your normal user config directory.

The calendar shows 14 months: December from the previous year, all 12 months of the selected/current year, and January from the next year.

Spanish public holidays for those three years are fetched from Nager Date when the app starts and cached locally. National holidays are shown in red; regional holidays are shown in orange with a legend for the current month.
