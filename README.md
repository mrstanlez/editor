# Editor
Simple editor for programmers
<br>
<br>

## Description
This a simple editor for programmers. Now for C, C++ only. Included drawing area, notes, 2 terminal windows on bottom,
themes, fonts, position, size. All is saved. Special buttons: Build, Run, Debug.
<br>

## Requirements:
### Arch Linux
sudo pacman -S base-devel gtk3 gtksourceview3 vte3 gcc gdb
<br>

### Debian/Ubuntu
sudo apt install build-essential pkg-config libgtk-3-dev libgtksourceview-3.0-dev libvte-2.91-dev
<br>

### Fedora / RHEL
sudo dnf groupinstall "Development Tools"
sudo dnf install pkgconf-pkg-config gtk3-devel gtksourceview3-devel vte291-devel
<br>

## Build:
gcc main.c actions.c -o editor `pkg-config --cflags --libs gtk+-3.0 gtksourceview-3.0 vte-2.91` -w
<br>

## Run:
./editor

<br>
<br>

Author: stanislav Petrek
Thank you.
