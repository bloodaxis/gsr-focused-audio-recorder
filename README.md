# GPU Screen Recorder Focused Audio Recorder

GPU Screen Recorder scripts and service that lets you automate recording only
the audio stream of the currently focused window. Press the same global shortcut
again to stop and finalize the recording.

> **Audio isolation is the purpose of this project:** the recording contains
> **only the audio belonging to the window/application that was focused when
> recording started**. It does not record general desktop audio, audio from
> other applications, or microphone input. Video may cover the entire monitor;
> “focused” refers specifically to which application's audio is captured.

If GPU Screen Recorder cannot match the focused window to an application-audio
source, recording is refused instead of silently capturing the wrong audio.

## Features

- Queries the focused window only when the shortcut is pressed; it does not poll.
- Records the focused window's current monitor.
- Matches GPU Screen Recorder's application-audio list against the window class
  and title, including Proton/Wine executable names.
- Uses one shortcut for start and stop.
- A second shortcut stops, finalizes, and opens KDE/Dolphin's YouTube share
  dialog for the completed recording. When nothing is recording, it shares the
  last successfully finalized recording instead.
- Shows recording state through a native KDE StatusNotifierItem tray helper.
- Watches the recorder with `pidfd_open()` instead of polling.
- Saves recordings to `~/Videos/GPUScreenRecorder` by default.

## Requirements

- KDE Plasma 6 on Wayland
- The `com.dec05eba.gpu_screen_recorder` Flatpak
- `systemd --user`, `qdbus-qt6`, `kwriteconfig6`, and `notify-send`
- KDE Purpose with its YouTube plugin and an Online Accounts YouTube account
- x86-64 for the included prebuilt tray helper

## Install from a source download

Download and extract the repository archive, then run:

```bash
chmod +x install.sh
./install.sh
```

Or clone it:

```bash
git clone https://github.com/bloodaxis/gsr-focused-audio-recorder.git
cd gsr-focused-audio-recorder
./install.sh
```

The shortcuts are exposed in **System Settings → Keyboard → Shortcuts → KWin**:

- `Meta+Ctrl+Alt+E`: start or stop normally
- `Meta+Ctrl+Alt+Y`: stop and share the current recording, or share the last
  finalized recording when the recorder is idle

## Repository layout

The installable files mirror their destinations beneath `payload/`:

```text
payload/.local/bin/                                  recorder scripts and tray binary
payload/.local/share/systemd/user/                   user services
payload/.local/share/kwin/scripts/gsr-focus-publisher/  KWin script package
src/gsr-focused-tray/                                Qt 6/KDE tray-helper source
```

## Recording options

The recorder runner accepts these environment variables:

- `GSR_OUTPUT_DIR` (default: `~/Videos/GPUScreenRecorder`)
- `GSR_FPS` (default: `60`)
- `GSR_CODEC` (default: `hevc`)
- `GSR_QUALITY` (default: `ultra`)
- `GSR_COLOR_RANGE` (default: `full`)
- `GSR_AUDIO_CODEC` (default: `opus`)
- `GSR_CURSOR` (default: `yes`)

They can be added as `Environment=` entries in
`~/.local/share/systemd/user/gsr-focused-recorder.service`.

## Build the tray helper on immutable Fedora/Bazzite

```bash
src="$PWD/src/gsr-focused-tray"
out="$(mktemp -d)"

podman run --rm \
  -v "$src:/src:ro,Z" \
  -v "$out:/out:Z" \
  fedora:44 bash -lc '
    dnf install -y --setopt=install_weak_deps=False \
      cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel \
      kf6-kstatusnotifieritem-devel kf6-purpose-devel &&
    cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release &&
    cmake --build /tmp/build &&
    cp /tmp/build/gsr-focused-tray /out/
  '

install -Dm755 "$out/gsr-focused-tray" \
  "$PWD/payload/.local/bin/gsr-focused-tray"
install -Dm755 "$out/gsr-share-youtube" \
  "$PWD/payload/.local/bin/gsr-share-youtube"
```

The binary is dynamically linked against Qt 6 and KDE Frameworks 6.
