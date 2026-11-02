# RuneScape 2 for PlayStation Vita

RuneScape Vita is a PlayStation Vita port of the revision 225 RuneScape 2 client, with an in-process offline world and on-console saves. This repository is a GitHub fork of [lesleyrs/Client3](https://github.com/lesleyrs/Client3). The Vita work published here is build 244 of revision 225, based on Client3 commit `53c4cf8dbc20cedf588ee07684709f3cd3aee2b7`.

This is source code. It is not a downloadable game package, and it is not affiliated with Jagex. Revision 377 is a separate project and is not in this tree.

The earlier export remains at [me11yb3an/rs2vita377clientsourcecode](https://github.com/me11yb3an/rs2vita377clientsourcecode). That repository was not renamed, overwritten, or deleted.

## Features

- Native Vita rendering through vitaGL (`-DGL11`, `-DVITA_NATIVE_WIDESCREEN`), with retained geometry and batched uploads in the `ba_hw_r*` sources.
- Controller and front-touch input in `src/platform/vita.c`.
- An in-process QuickJS world. The client and the world pass bytes through a queue. The embedded server does not open a listening socket.
- Character saves with a CRC, a backup file, and quarantine of a damaged live file.
- Dialogue and option-menu navigation on the controller, and a performance readout.

A checkout still needs a resource pack and the libraries in [docs/BUILDING.md](docs/BUILDING.md). Those are not in this branch.

## Layout

```text
src/           Client3 sources, with the build 244 changes overlaid
save/          Save format and commit code. Not a folder of player saves
server/        QuickJS bundles and their copyright notices
provenance/    Export archive, byte manifest notes, and the unmodified build scripts
docs/          Build, controls, architecture, status, and compliance
units.mk       Build 244 VitaSDK makefile. In-repo paths point at this tree
link.sh        Build 244 link line. In-repo object paths point at this tree
vita.mk        Upstream Vita package rule. It does not compile the QuickJS host
```

`Makefile` and `vita.mk` still build the original Client3 source list. The new translation units are named in `build244.mk` and are compiled only by `units.mk`. Upstream files that match build 244 after newline normalization were left as the Client3 blobs. The file-by-file result is [docs/MIGRATION.md](docs/MIGRATION.md).

The upstream readme is preserved at [docs/CLIENT3-README.md](docs/CLIENT3-README.md).

## Architecture

One Vita process runs the C client and a QuickJS runtime. `src/platform/vita.c` reads the controller. `src/gl11.c` and the `ba_hw_r*` sources upload and draw. `packet_queue.h` carries packets to `src/embedded_server.c`, which runs `server/server.bundle.npc-unload.js` on one worker thread. Saves are written by `save/`.

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) is the longer description. The framebuffer is 960×544. The user heap budget in `src/offline_vita.c` is 224 MiB, separate from the CDRAM framebuffers.

## Build requirements

A fresh VitaSDK compilation of this tree has not succeeded. `make -f units.mk -n` parses the makefile and stops because `src/r63_search_icon.h` is missing. Do not treat a checkout as a finished, reproducible build.

The build 244 commands, once the missing inputs below are supplied locally, are:

```sh
make -f units.mk
make -f units.mk client.elf
make -f units.mk eboot.bin
```

Dependencies that are not in this repository:

- VitaSDK: `arm-vita-eabi-gcc`, `arm-vita-eabi-strip`, `vita-elf-create`, `vita-make-fself`. `units.mk` still calls `/usr/local/vitasdk/bin/arm-vita-eabi-gcc`.
- vitaGL and vitaShaCC. A console GL build also needs `libshacccg.suprx` installed on the Vita. That file is not in git.
- `libquickjs.a`, `vendor/quickjs-vita/quickjs.h`, `libvita225_native_bridge.a`, `libbz2.a`, `vita_crypto.o`, and `vita-segment-gap.ld`.
- Five generated image headers listed in [docs/BUILD-DEPS.md](docs/BUILD-DEPS.md). They were left out because they embed artwork.
- A local resource pack. `src/offline_vita.c` stops unless `ba_resource_init()` succeeds. `src/vita_enhanced.c` also opens `rom/vita-enhanced.bin`.

`provenance/checkpoint-units.mk` and `provenance/checkpoint-link.sh` are the unmodified checkpoint scripts. `make -f vita.mk` is the upstream Client3 package rule. It still names `rom/cache` and `rom/SCC1_Florestan.sf2`, which this tree does not contain, and it does not link the QuickJS world. Use `units.mk` for the build 244 source build. Do not commit the package it would produce.

## Installation prerequisites

This repository does not contain a VPK. Installing a test build on a Vita is a separate step you do yourself.

- A Vita prepared for homebrew, with VitaSDK only required if you are compiling.
- `libshacccg.suprx` on the console for the vitaGL path.
- Your own copy of the runtime resource pack and `rom/vita-enhanced.bin`.
- The Roboto fonts in `rom/Roboto/` are included. Their notice is `rom/Roboto/NOTICE`.
- `rom/config.ini` has an empty password. Put a password only in a local file that you do not commit.

## Controller

Mapped from `src/platform/vita.c`:

| Input | Action |
| --- | --- |
| Left stick | Move the pointer |
| Right stick | Rotate the camera, past a deadzone of 60 |
| Cross, or R | Primary pointer action. Cross also confirms a menu or dialogue choice |
| L | Secondary pointer action. L cancels an open option menu |
| Triangle | Hold for the run modifier |
| D-pad up / down | Move through an open menu or dialogue |
| D-pad left / right | Change the selected side panel while no menu is open |
| Square, or Select | Toggle the performance readout |
| Front touch | Pointer. A tap can open the on-screen keyboard |
| Circle, Start | Read by the poll and currently unused |

[docs/CONTROLS.md](docs/CONTROLS.md) records the same mapping with the conditions from the input poll.

## Offline server

`src/embedded_server.c` hosts QuickJS. `src/world_worker.h` is the worker thread. `packet_queue.h` is the in-process queue. The active bundle is [server/server.bundle.npc-unload.js](server/server.bundle.npc-unload.js). [server/server.bundle.tickparts.js](server/server.bundle.tickparts.js) is the reference bundle from the same snapshot.

Both bundles are a revision 225 world. `src/util/Environment.ts` inside them matches LostCityRS/Engine-TS commit `e1dea19f256c7ff1a89d47024c811c755ad2184d`. The notices for that engine, `@2004scape/rsbuf`, `@2004scape/rsmod-pathfinder`, and node-forge are in [server/NOTICE.md](server/NOTICE.md). There is no repository-wide license.

## Saves

`save/` implements the format and the commit rules. It does not contain player saves.

- Magic `8196` (`0x2004`), versions 0 through 6, CRC32 over the image.
- Login selects `name.sav` when that image validates, otherwise `name.sav.bak`.
- `name.sav.tmp` is never treated as a committed save.
- A damaged live file is quarantined. The previous good file stays as `.bak`.
- On Vita, profiles are stored under `ux0:/data/RS2Vita/players/`. That directory is created at runtime and is not part of this repository.

## Current status

Build 244 / revision 225 is the working Vita foundation in this repository. It has not passed a fresh VitaSDK link. [docs/STATUS.md](docs/STATUS.md) and [docs/BUILD-DEPS.md](docs/BUILD-DEPS.md) list the missing inputs.

## Known limitations

- The five image headers, the native bridge, `vita_crypto.o`, the segment linker script, and QuickJS are not in the tree.
- Circle and Start are read and then ignored.
- `make -f vita.mk` still expects cache and soundfont paths that were removed from this branch.
- There is no repository-wide license. Client3 publishes none, and a GitHub fork does not grant one. The files that still need a grant are listed in [docs/CLIENT3-REDISTRIBUTION.md](docs/CLIENT3-REDISTRIBUTION.md). The practical channel is the author of [lesleyrs/Client3](https://github.com/lesleyrs/Client3).
- Older commits on this fork, and upstream Client3, still contain `rom/cache` and `rom/SCC1_Florestan.sf2`. This branch does not. History was not rewritten, so those blobs remain reachable from the parent history.

## Revision 377

Revision 377 stays outside this fork until it is reviewed on its own. The roadmap, and the boundary, is:

1. Keep 377 on a separate branch. Do not merge it into build 244 or into `main`.
2. Publish it only after the same attribution check used for build 244: which files are original, which are derived, and which licenses travel with them.
3. Leave 377 caches, diagnostics, and experimental sources out until that review is approved.

No 377 client source was added to this branch.

The two tracks stay separate:

| Track | Revision | Where it is |
| --- | --- | --- |
| Working Vita client | Revision 225, build 244 | This branch, `migration/vita-build-244` |
| Experimental client | Revision 377 | Outside this fork. Not a build flag and not part of the publication decision for build 244 |

## Compliance

The review notes are [docs/PUBLICATION.md](docs/PUBLICATION.md), [docs/COMPLIANCE.md](docs/COMPLIANCE.md), and [docs/PROVENANCE.md](docs/PROVENANCE.md). Publishing this branch does not close the Client3 permission question.

## Credits

- **[lesleyrs/Client3](https://github.com/lesleyrs/Client3)** — the C client this fork is based on. [RustCityRS/client-c](https://github.com/RustCityRS/client-c) is a fork of that project. Neither publishes a license. The upstream readme is [docs/CLIENT3-README.md](docs/CLIENT3-README.md).
- **me11yb3an** — the build 244 Vita renderer integration, controller work, QuickJS embedding, and save system.
- **[LostCityRS/Engine-TS](https://github.com/LostCityRS/Engine-TS)** — revision 225 world engine used by the bundles. MIT notice: [server/NOTICE.md](server/NOTICE.md).
- **[Brendonm17/Client3-Vita](https://github.com/Brendonm17/Client3-Vita)** — a documentation reference for a Vita port of Client3. None of its source was copied.
- **[Brendonm17/rsc-c-vita](https://github.com/Brendonm17/rsc-c-vita)** — AGPL-3.0. Its QuickJS-on-Vita approach informed the embedding. No source line from that tree was found in the build 244 export.
- Vendored notices in `src/thirdparty/` cover ini, ISAAC, LibTomMath, stb, TinySoundFont, and micro-bunzip. QuickJS and vitaGL are linked from outside the tree.

RuneScape is a trademark of Jagex.

## License and compliance

There is no root `LICENSE`. Forking a public repository on GitHub does not grant a license to modify or redistribute Client3. Client3 publishes no license, and no written grant is in this tree. [docs/LICENSING.md](docs/LICENSING.md) is the longer audit.

## What this tree does not contain

No VPK, `eboot.bin`, `client.pak`, `server.pak`, player save, or `libshacccg.suprx` is tracked. `rom/cache/` and `rom/SCC1_Florestan.sf2` are not in this branch. The Roboto fonts remain, with `rom/Roboto/NOTICE`. A draft description for a future VPK release that you upload yourself is [docs/VPK-RELEASE-DRAFT.md](docs/VPK-RELEASE-DRAFT.md). That file is not a published GitHub Release.
