#!/usr/bin/env bash
set -Eeuo pipefail

readonly REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PAYLOAD_DIR="$REPO_DIR/payload"

if [[ "$(uname -m)" != "x86_64" ]]; then
    printf 'The included tray binary is built for x86_64. Build src/gsr-focused-tray first on this architecture.\n' >&2
    exit 1
fi

for command in flatpak systemctl kwriteconfig6 qdbus-qt6 notify-send; do
    command -v "$command" >/dev/null 2>&1 || {
        printf 'Missing required command: %s\n' "$command" >&2
        exit 1
    }
done

flatpak info com.dec05eba.gpu_screen_recorder >/dev/null 2>&1 || {
    printf 'The GPU Screen Recorder Flatpak (com.dec05eba.gpu_screen_recorder) is required.\n' >&2
    exit 1
}

install -d \
    "$HOME/.local/bin" \
    "$HOME/.local/share/systemd/user" \
    "$HOME/.local/share/kwin/scripts/gsr-focus-publisher/contents/code"

for script in \
    gsr-focus-event \
    gsr-focused-recorder-run \
    gsr-focused-recorder-stop \
    gsr-focused-toggle \
    gsr-focused-tray; do
    install -m 0755 "$PAYLOAD_DIR/.local/bin/$script" "$HOME/.local/bin/$script"
done

install -m 0644 "$PAYLOAD_DIR/.local/share/systemd/user/gsr-focused-recorder.service" \
    "$HOME/.local/share/systemd/user/gsr-focused-recorder.service"
install -m 0644 "$PAYLOAD_DIR/.local/share/systemd/user/gsr-focused-toggle@.service" \
    "$HOME/.local/share/systemd/user/gsr-focused-toggle@.service"
install -m 0644 "$PAYLOAD_DIR/.local/share/systemd/user/gsr-focused-tray.service" \
    "$HOME/.local/share/systemd/user/gsr-focused-tray.service"

install -m 0644 \
    "$PAYLOAD_DIR/.local/share/kwin/scripts/gsr-focus-publisher/metadata.json" \
    "$HOME/.local/share/kwin/scripts/gsr-focus-publisher/metadata.json"
install -m 0644 \
    "$PAYLOAD_DIR/.local/share/kwin/scripts/gsr-focus-publisher/contents/code/main.js" \
    "$HOME/.local/share/kwin/scripts/gsr-focus-publisher/contents/code/main.js"

systemctl --user daemon-reload
systemctl --user enable --now gsr-focused-tray.service

kwriteconfig6 --file kwinrc --group Plugins --key gsr-focus-publisherEnabled true
qdbus-qt6 org.kde.KWin /KWin org.kde.KWin.reconfigure >/dev/null 2>&1 || true

printf '%s\n' \
    'Installed focused GPU Screen Recorder integration.' \
    'Default shortcut: Meta+Ctrl+Alt+E' \
    'The shortcut is configurable in KDE System Settings > Keyboard > Shortcuts > KWin.' \
    'If it is not immediately listed, log out and back in once.'
