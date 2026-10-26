#!/bin/sh
set -eu
FLOWTONE_SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FLOWTONE_VOLUME=${3:-/}
FLOWTONE_ROOT=${FLOWTONE_VOLUME%/}
FLOWTONE_COMPONENT="$FLOWTONE_ROOT/Library/Audio/Plug-Ins/Components/FLOWTONE.component"
FLOWTONE_BACKUPS="$FLOWTONE_ROOT/Library/Application Support/FLOWMINNERS/FLOWTONE/Backups"

backup_directory() {
    /bin/mkdir -p "$FLOWTONE_BACKUPS"
    /usr/bin/mktemp -d "$FLOWTONE_BACKUPS/$(/bin/date +%Y%m%d-%H%M%S)-XXXXXX"
}

console_home() {
    FLOWTONE_CONSOLE_USER=$(/usr/bin/stat -f '%Su' /dev/console)
    case "$FLOWTONE_CONSOLE_USER" in root|loginwindow|_mbsetupuser|'') return 1 ;; esac
    FLOWTONE_USER_HOME=$(/usr/bin/dscl . -read "/Users/$FLOWTONE_CONSOLE_USER" NFSHomeDirectory | /usr/bin/sed 's/^NFSHomeDirectory: //')
    case "$FLOWTONE_USER_HOME" in /*) ;; *) return 1 ;; esac
}
