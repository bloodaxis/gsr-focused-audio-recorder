function unitValue(value) {
    return String(value || "unknown")
        .replace(/[^A-Za-z0-9_.-]/g, "_")
        .substring(0, 80);
}

function toggleFocusedRecording() {
    const window = workspace.activeWindow;
    if (!window) {
        return;
    }

    const unit = "gsr-focused-toggle@" + unitValue(window.resourceClass) + "___" +
        unitValue(window.output ? window.output.name : "") + "___" +
        unitValue(window.caption) + ".service";

    callDBus(
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "StartUnit",
        unit,
        "replace"
    );
}

registerShortcut(
    "gsr-focused-recording-toggle",
    "Toggle focused application recording",
    "Meta+Ctrl+Alt+E",
    toggleFocusedRecording
);
