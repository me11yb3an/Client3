# Provenance

The approved export is identified by `SOURCE-MANIFEST.json` at the repository root. That file is unchanged on this branch.

## Checkpoint

- Build number recorded in the manifest: 244
- Origin string in the manifest: hash-verified `source244.zip` from the approved build 244 checkpoint
- Archive SHA-256 recorded for that original source archive: `f56ff76383184aba4f11ac144e5d44812f909c69af278e3ab79f1a7392c97eb1`
- Active bundle: `server/server.bundle.npc-unload.js`
- Active bundle SHA-256: `b57d89765daef6e667df52668a47403270fdcf1f7d1bfd154bbad07990df610c`
- Manifest file count: 361
- `source_files_unchanged` remains true for the C, headers, includes, scripts, and bundles. This branch adds documentation around them.

The SHA-256 above is the value stored in the manifest. The file `source244.zip` itself is not in the clone.

## Extraction check

On 2 November 2026, `src-tree.rar` was extracted in this clone.

- Files extracted: 335
- Manifest entries under `src-tree/`: 335
- SHA-256 mismatches: 0
- Extra extracted files: 0

The archive now lives at `provenance/src-tree.rar` so the packed bytes can be hashed again later. Re-check with the manifest rather than by trusting this note.

## Line endings

Git on the machine that prepared the branch had `core.autocrlf=true`. A normal checkout rewrites LF sources to CRLF in the working tree and makes a raw hash disagree with the manifest. The blobs stored by git for the original 26 loose files match the manifest. `.gitattributes` marks source as non-text so a later add does not silently change those bytes.

## History

The repository was created on 2 November 2026. The commits on `main` at the time of the audit are:

| Commit | Subject |
| --- | --- |
| `074a0094` | Initial commit |
| `b400994f` | Add `Runescape-2-build-244-377.zip` |
| `b00d2e78` | Revise README |
| `150daf1d` | Update README with project links |
| `44e7a4d3` | Add the loose sources, bundles, manifest, and `src-tree.rar` |
| `1653874d` | Delete `Runescape-2-build-244-377.zip` from the tip |

The zip blob is `e832254037bbe27d9c3c59010df1fb9c4435858e` (1,462,242 bytes). Its contents are source files of the same export, not a cache or a player save. History was not rewritten.

## Engine pin for the bundles

Both `server/server.bundle.npc-unload.js` and `server/server.bundle.tickparts.js` contain the same `src/util/Environment.ts` text as LostCityRS/Engine-TS commit `e1dea19f256c7ff1a89d47024c811c755ad2184d` (branch `225`, committed 23 February 2026). That file's previous commit, `b502762d6ff29619081bb4c0402008de0201513a`, still has `BUILD_JAVA_PATH` and does not define `IS_BUN`. The bundles have `IS_BUN` and `BUILD_VERBOSE`, and they do not contain `BUILD_JAVA_PATH`.

The tickparts bundle's source-path list is a subset of the npc-unload bundle. The npc-unload bundle adds 31 paths, including `embedded/` and `source/server-experimental/engine/` Barrows modules. Those paths are local work on top of the engine. Notices for the upstream pieces are in `server/NOTICE.md`.

## VPK reviewed and left out

`C:\Users\skant\Desktop\RS2_Ver_0.92.vpk` (25,256,084 bytes, 33 members) was listed and not copied into this branch. It contains `resources/client.pak`, `resources/server.pak`, `sp/offline_test.sav`, `module/libshacccg.suprx`, `eboot.bin`, livearea images, and `rom/` binaries. Publishing that package would put resource archives, a save, and Sony's shader compiler in the public repository.

## Exclusions already recorded

The manifest lists 16 excluded paths: five generated image headers and eleven evidence or test-report files. Those exclusions were kept. The evidence scripts were not fetched from the checkpoint and were not added here.

## Fork layout

On 2 November 2026 the same export was overlaid onto a local clone of `me11yb3an/Client3`, which GitHub created as a fork of `lesleyrs/Client3` at `53c4cf8dbc20cedf588ee07684709f3cd3aee2b7`. The client files that differ from that commit live under `src/`. Files that match after newline normalization were left as the Client3 blobs. `SOURCE-MANIFEST.json` still names the export paths, including `src-tree/`.

The root `units.mk` and `link.sh` are the checkpoint scripts with in-repo paths retargeted. `provenance/checkpoint-units.mk` and `provenance/checkpoint-link.sh` are the unmodified bytes. `provenance/src-tree-offline_vita.c` is the export's other `offline_vita.c` (`0183d4b3`). The file `units.mk` compiles is `src/offline_vita.c` (`853b5b90`).

This branch's tip removes `rom/cache` and `rom/SCC1_Florestan.sf2`. The Roboto fonts stay, with `rom/Roboto/NOTICE`. Older commits and `lesleyrs/Client3` still contain the removed files. No history was rewritten, and no root license was added.
