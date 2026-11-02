# Draft release notes

This is a draft description only. It is not a GitHub Release, it has no tag, and it attaches no binary. Upload any VPK yourself.

## Title

RuneScape Vita build 244 test build

## Body

Test build of the revision 225 Vita client, build 244. The matching source is the `migration/vita-build-244` history in this repository. That source has not passed a fresh VitaSDK compilation in the publication check, because generated image headers and several link libraries are not in git.

### Features

- Native Vita rendering at 960×544 through vitaGL, with retained geometry and batched uploads.
- Controller and front-touch input.
- An in-process QuickJS world. The embedded server does not open a listening socket.
- Character saves with a CRC, a backup, and quarantine of a damaged live file.

### Install

- Install the VPK with your usual Vita homebrew installer.
- The console needs `libshacccg.suprx` for the vitaGL path. That file is not in this repository and is not attached here.
- The package expects its runtime resources beside the installed application. This repository does not include a RuneScape cache.
- Saves are created under `ux0:/data/RS2Vita/players/`. Do not commit them.

### Required outside this archive

- VitaSDK only if you are compiling rather than installing a VPK.
- `libquickjs.a`, `libvita225_native_bridge.a`, `libbz2.a`, `vita_crypto.o`, `vita-segment-gap.ld`, and `vendor/quickjs-vita/quickjs.h` for a source build.
- The five generated image headers named in `docs/BUILD-DEPS.md`.

### Known issues

- Circle and Start are read and currently unused.
- A source checkout alone does not link.
- Client3 has no license. This release does not add one. See `docs/CLIENT3-REDISTRIBUTION.md`.

### Credits

- lesleyrs/Client3 for the C client.
- Build 244 Vita renderer, controller, QuickJS embedding, and saves.
- Lost City engine, `@2004scape/rsbuf`, `@2004scape/rsmod-pathfinder`, and node-forge, with notices in `server/NOTICE.md`.
- Roboto by Christian Robertson, Copyright 2011 Google Inc., Apache-2.0.

RuneScape is a trademark of Jagex. This is not an official client.
