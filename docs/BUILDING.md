# Building the Vita client

This page describes the build recorded in the approved snapshot. The scripts were not executed as part of the repository reorganization.

## Tools

Install [VitaSDK](https://vitasdk.org/). The snapshot expects the toolchain at `/usr/local/vitasdk/bin`:

- `arm-vita-eabi-gcc`
- `arm-vita-eabi-strip`
- `vita-elf-create`
- `vita-make-fself`

The link line also uses vitaGL (`-lvitaGL`), vitaShaCC (`-lvitashark`, `-lSceShaccCg_stub`), math-neon, pthread, and the usual Sce stubs (`Ctrl`, `Touch`, `Display`, `Audio`, `Power`, `Net`, `Rtc`, `Sysmem`, `AppMgr`). OpenGL on a real Vita needs `libshacccg.suprx` installed for vitaGL.

The original environment was a WSL workspace. `link.sh` is a bash script.

## What the scripts build

From the repository root, the preserved targets are:

```sh
make -f units.mk
make -f units.mk client.elf
make -f units.mk eboot.bin
```

`all` compiles the client objects under `out/`. `client.elf` also builds the objects under `arm/` (embedded server and save system) and then runs `bash link.sh`. `eboot.bin` strips that ELF, creates a VELF, and packs an FSELF.

Compiler flags fixed in `units.mk` include `-std=c99`, `-DGL11`, `-DWITH_RSA_LIBTOM`, `-DBUILD_A_EMBEDDED`, and `-DVITA_NATIVE_WIDESCREEN`. The server objects are C11.

## Paths

On this fork the root `units.mk` and `link.sh` point in-repo paths at the checkout: `src-tree/` is now `src/`, and `/tmp/rs225-content244-root/` object paths are relative. The unmodified checkpoint bytes are `provenance/checkpoint-units.mk` and `provenance/checkpoint-link.sh`.

These paths are still outside the clone:

- `/usr/local/vitasdk/bin/...` when the snapshot names the toolchain that way
- `/tmp/rs225-codex-r15/` for QuickJS, the native bridge, bzip2, `vita_crypto.o`, a bridge header, and `vita-segment-gap.ld`
- `/tmp/rs225-codex-bridge/` as an include directory

The branch was prepared on a machine with no `arm-vita-eabi-gcc` and no `VITASDK`, so the scripts were not executed. A path check of `units.mk` found 162 in-repo inputs. Three of those are generated headers and are absent on purpose: `src/vita_map_aperture.generated.h`, `src/r27_cursor_images.h`, and `src/r63_search_icon.h`. `src/ui474.c` also includes `src/runner_icon.generated.h` and `src/custom_run_stone.generated.h`, which the makefile does not list as its own prerequisites. All five stay out because they embed artwork.

## Libraries and headers that are not in the clone

The link requires these objects, which were never part of the public export:

- `libquickjs.a`
- `libvita225_native_bridge.a`
- `libbz2.a`
- `vita_crypto.o`
- `vita-segment-gap.ld`

`src/embedded_server.c` includes `vendor/quickjs-vita/quickjs.h`. That header tree is external.

`units.mk` also lists generated headers that `SOURCE-MANIFEST.json` excludes because they embed image or alpha-mask artwork:

- `src/custom_run_stone.generated.h`
- `src/r27_cursor_images.h`
- `src/r63_search_icon.h`
- `src/runner_icon.generated.h`
- `src/vita_map_aperture.generated.h`

`src/offline_vita.c` aborts at startup unless `ba_resource_init()` returns 2. The resource pack that satisfies that check is a local runtime file. It is not in this repository and should not be added if it contains Jagex cache data.

## Runtime files the console creates

On device the client creates `ux0:/data/RS2Vita/` and writes logs and player saves there. Those files are local to the console. Do not commit them.

## JavaScript bundle

`link.sh` does not compile the bundle. The package loads `server/server.bundle.npc-unload.js` at runtime. How the VPK install copies that file into `app0:` is part of the packaging step outside `units.mk`. The manifest names this file as the active bundle.
