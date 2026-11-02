# Development status

Date of this note: 2 November 2026. The status below describes the public source export, not a newly tested console run.

## Approved snapshot

Build 244 is the working Vita source. `SOURCE-MANIFEST.json` records 361 source files, the active server bundle, and the files left out. This branch extracts that snapshot so it can be read. It does not retune gameplay, the renderer, the world, or saves.

The original README stated that the approved checkpoint, the installed game, and player saves were not modified. This reorganization follows the same boundary. It was done in a separate clone and branch.

## In this tree

- The revision 225 C client is browsable under `src/`, overlaid on Client3.
- The Vita embedding, QuickJS host, and save implementation are under `src/` and `save/`.
- The active and reference JavaScript bundles are under `server/`.
- The original `src-tree.rar` is under `provenance/` for a byte-level re-check.
- Root `units.mk` and `link.sh` point at this checkout. The unmodified scripts are `provenance/checkpoint-units.mk` and `provenance/checkpoint-link.sh`.

## Not in this tree

- VPK, ELF, and the console install of build 244. `RS2_Ver_0.92.vpk` was not added.
- The resource pack required at runtime. Client3's own `rom/` tree is already in the fork because the branch is based on upstream. This migration did not add another cache.
- The five generated image headers.
- QuickJS, the native bridge library, and the linker script.
- Player saves and runtime logs.
- Revision 377 client sources and the Slayer research notes.

## Revision 377

377 work is experimental and separate. It is not a mode, flag, or directory in build 244. Shipping it would mean adding different sources and different content, which this branch does not do.

## Known limitations

- A fresh clone cannot produce `eboot.bin` until the external toolchain paths, libraries, omitted headers, and resource pack are restored locally.
- `link.sh` wraps a large set of Vita and GL calls. Those wraps belong to the approved link line and were not simplified.
- Circle and Start have empty handlers.
- The server bundles are third-party build products. Their upstream commit is not pinned in this repository.
- No license file covers the whole project. Reuse of the client sources is not granted by the upstream C client, because that project has no license.

## Suggested next steps, after review

These are recommendations. They are not done on this branch.

1. Re-run `make -f units.mk eboot.bin` on a machine with VitaSDK. The in-repo paths were pointed at this checkout without a compile.
2. Record the esbuild recipe that produced each bundle. `server/NOTICE.md` already carries the upstream notices, and only `Environment.ts` is pinned to `e1dea19f`.
3. Ask the Client3 author before describing these client sources as reusable.
4. Confirm whether any file was copied from the AGPL-3.0 RSC Vita client. A copied file stays under that license.
5. Leave caches, VPKs, and saves out of the public repository.
