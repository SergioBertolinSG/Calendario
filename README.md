# Calendario GTK

Scaffold for a small C app using GTK 4 that renders a bold wall-calendar UI inspired by `../taco.jpg`.

## Files

- `src/main.c`: GTK application and calendar layout.
- `styles/calendario.css`: visual theme with large numbers, black/red palette, and poster-style spacing.
- `Makefile`: simple build and run targets.

## Requirements

You need GTK 4 development libraries, `pkg-config`, and `curl`.

Examples:

- Debian/Ubuntu: `sudo apt install build-essential pkg-config libgtk-4-dev curl`
- Fedora: `sudo dnf install gcc make pkgconf-pkg-config gtk4-devel curl`

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
sudo snap install calendario_*.snap --dangerous
calendario
```

To test another year without changing the system clock:

```sh
CALENDARIO_YEAR=2024 make run
```

Click a day to add or edit a note. Notes are restored the next time the app opens. When running as a snap, notes and cached holidays are stored under the snap sandbox user area, usually `~/snap/calendario/common/calendario/`. Local development builds still use your normal user config directory.

The calendar shows 14 months: December from the previous year, all 12 months of the selected/current year, and January from the next year.

Spanish public holidays for those three years are fetched from Nager Date and cached locally. National holidays are shown in red; regional holidays are shown in orange with a legend for the current month.
