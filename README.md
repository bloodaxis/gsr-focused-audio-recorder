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
- Shows a separate YouTube upload tray indicator. It stays after a successful
  upload until you open the video from the icon or dismiss it. During upload,
  clicking the icon opens YouTube Studio.
- Shows recording state through a native KDE StatusNotifierItem tray helper.
- Watches the recorder with `pidfd_open()` instead of polling.
- Saves recordings to `~/Videos/GPUScreenRecorder` by default.

## Requirements

- KDE Plasma 6 on Wayland
- The `com.dec05eba.gpu_screen_recorder` Flatpak
- `systemd --user`, `qdbus-qt6`, `kwriteconfig6`, and `notify-send`
- KDE Purpose with its YouTube plugin and an Online Accounts YouTube account
- x86-64 Fedora 44/Bazzite with compatible Qt 6 and KDE Frameworks 6 runtime
  libraries for the included release binaries

## Install a release package

Download `gsr-focused-audio-recorder-v1.0.0-linux-x86_64.tar.gz` and its
`.sha256` file from the GitHub release, then verify, extract, and install:

```bash
sha256sum -c gsr-focused-audio-recorder-v1.0.0-linux-x86_64.tar.gz.sha256
tar -xzf gsr-focused-audio-recorder-v1.0.0-linux-x86_64.tar.gz
cd gsr-focused-audio-recorder-v1.0.0
./install.sh
```

Restart Dolphin after installation so it loads the updated Purpose YouTube
plugin. An upload already in progress keeps using its existing helper process.

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
payload/.local/libexec/                              YouTube upload tray binary
payload/.local/lib64/qt6/plugins/kf6/purpose/        YouTube Purpose plugin binary
payload/.local/share/kf6/purpose/                    YouTube visibility configuration
payload/.local/share/systemd/user/                   user services
payload/.local/share/kwin/scripts/gsr-focus-publisher/  KWin script package
src/gsr-focused-tray/                                Qt 6/KDE tray-helper source
src/purpose-youtube/                                  Purpose source patch and tray source
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
./src/gsr-focused-tray/build-container.sh

install -Dm755 src/gsr-focused-tray/build-output/gsr-focused-tray \
  "$PWD/payload/.local/bin/gsr-focused-tray"
install -Dm755 src/gsr-focused-tray/build-output/gsr-share-youtube \
  "$PWD/payload/.local/bin/gsr-share-youtube"
```

The first build creates the persistent image
`localhost/gsr-focused-tray-builder:fedora44` and named container
`gsr-focused-tray-builder`. Later builds restart that stopped container and
reuse its installed toolchain. The `Containerfile` is stored beside the C++
source.

The binaries are dynamically linked against Qt 6 and KDE Frameworks 6. The
YouTube plugin changes are based on KDE Purpose commit
`8473417a25a9a7c7ba137d34289063b8053c403e`; see
`src/purpose-youtube/README.md` for source and build details.

To create the installable archive from the checked-in payload, run
`./build-release.sh v1.0.0`. It writes the archive and SHA-256 checksum to
`dist/`.
