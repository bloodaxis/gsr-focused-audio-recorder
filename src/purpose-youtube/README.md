# Purpose YouTube upload integration source

The release's `youtubeplugin.so` is built from KDE Purpose commit
`8473417a25a9a7c7ba137d34289063b8053c403e` plus `purpose.patch`.
The release's `purpose-youtube-tray` is built from `youtubetray.cpp` in the
same patched Purpose build. KDE Purpose is available at
<https://invent.kde.org/frameworks/purpose> and is licensed under
LGPL-2.1-or-later. The added tray source carries the same license.

To reproduce the source tree:

```bash
git clone https://invent.kde.org/frameworks/purpose.git
cd purpose
git checkout 8473417a25a9a7c7ba137d34289063b8053c403e
git apply /path/to/gsr-focused-audio-recorder/src/purpose-youtube/purpose.patch
cp /path/to/gsr-focused-audio-recorder/src/purpose-youtube/youtubetray.cpp \
  src/plugins/youtube/youtubetray.cpp
```

Configure Purpose with Qt 6 and KDE Frameworks 6 development packages, then
build the `youtubeplugin` and `purpose-youtube-tray` targets. The release
payload installs the plugin under the user's Qt 6 Purpose plugin directory and
the tray helper in `~/.local/libexec/`.
