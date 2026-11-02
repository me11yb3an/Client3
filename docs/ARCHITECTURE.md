# Architecture

Build 244 is one Vita process. A C client renders the world. A QuickJS runtime, in the same process, advances the revision 225 world. They pass packets through memory.

```text
front touch / ctrl
        |
        v
platform/vita.c  ---- pointer, camera, buttons
        |
        v
entry/client.c   ---- login, scene, interface, audio
        |
        +---- gl11.c and ba_hw_r*   vitaGL upload and draw
        |
        +---- packet_queue.h        bounded byte queues
                    |
                    v
        embedded_server.c + world_worker.h
                    |
                    v
        server.bundle.npc-unload.js    QuickJS world
                    |
                    v
        save/                          CRC image on ux0:
```

## Client

The C sources under `src/` follow the layout of the revision 225 C client: `entry/client.c`, `gameshell.c`, `world.c`, `world3d.c`, `model.c`, the `pix*` software rasterizer, and `platform/vita.c`. Build 244 adds the Vita UI, camera, dialogue, and hardware-render files (`vita_*`, `ui474.c`, `r10_` through `r113_`, `ba_*`).

`ui474.c` is a gameplay interface adapter in this client. The number in the filename is not a claim that the protocol is revision 474.

`src/offline_vita.c` is the Vita entry wrapper. It sets a 224 MiB user heap and a 2 MiB main-thread stack, creates `ux0:/data/RS2Vita/`, and refuses to continue when the resource index is missing or invalid. Profiling logs under that directory are written only when diagnostics are enabled.

## Native rendering

`units.mk` defines `GL11` for every client object. `src/gl11.c` is the vitaGL drawing path. The hardware stages included by `src/offline_vita.c` are:

| File | Role in the snapshot |
| --- | --- |
| `src/ba_hw_r2.inc` | Hardware summary counters |
| `src/ba_hw_r3.inc` | Included with the Vita frame timing |
| `src/ba_hw_r4.inc` | Vita init declares `ba_hw_r4.h` before the frame loop |
| `src/ba_hw_r5.inc` | Frame-stage include |
| `src/ba_hw_r7.inc` | Frame-stage include |
| `src/ba_hw_r6.c` | Compiled as its own object |

The export also stored a second copy of `ba_hw_r2.inc` through `r7.inc`. After newline normalization the text matches, so this fork keeps one copy under `src/`. There is no `ba_hw_r6.inc`.

The software rasterizer objects are still linked. `GL11` selects the accelerated draw path at compile time. CDRAM for the framebuffers is allocated in the platform layer and is not part of the 224 MiB user heap.

## QuickJS world

`src/embedded_server.c` creates one QuickJS runtime and context. Comments in that file call it a build 225 runner. Native helpers on the JS side use the build 225 buffer and pathfinder entry points, and those calls stay on the single JS thread.

`src/world_worker.h` runs that pump on a pthread with a 2 MiB stack. The render thread reads the client-bound queue without taking the JS lock. The queue lock covers the byte copy only.

There is no listen socket in this embedding. The first packet carried across the queue is the 8-byte build 225 seed. Later bytes are world traffic.

The active script is the esbuild bundle `server/server.bundle.npc-unload.js`. Inside it, `ENGINE_REVISION` defaults to 225. A second bundle, `server.bundle.tickparts.js`, is retained as the reference copy from the same checkpoint.

QuickJS itself is `libquickjs.a`, linked from outside this repository. The header expected by the embed is `vendor/quickjs-vita/quickjs.h`, also external. The startup log line in `offline_vita.c` records `js_limit=96MiB`.

## Saves

The world asks the save module for a character image at login and writes one back through `save_commit`. The format and the filenames are documented in `save/save_format.h` and `save/save_login.h`. The Vita reader used by the embedded server is `src/r103_save_native_read.h`.

Player files stay on `ux0:`. The `save/` directory in git is only the implementation.

## What is outside the process

A resource pack must already be on the console. Caches, models, and maps are not compiled into this source tree. The experimental revision 377 client is a different body of work and is not linked into this ELF.
