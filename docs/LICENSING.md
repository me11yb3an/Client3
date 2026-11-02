# Licensing and redistribution audit

Audit date: 2 November 2026. Scope: the public repository `me11yb3an/rs2vita377clientsourcecode` at commit `1653874d` (HEAD before this branch), its full git history, the archives in that history, and the local asset folder that was inspected and not imported.

No `LICENSE` file was added. A missing license is not the same as permission, and a public GitHub repository is not a grant to copy, modify, or relicense the files.

## Result

| Area | Finding | Action on this branch |
| --- | --- | --- |
| Repository-wide license | None published | Still none |
| C client lineage | Same layout and platform set as `lesleyrs/Client3`, which has no license | Sources preserved and attributed. No new license added |
| `Brendonm17/Client3-Vita` | Fork of Client3. No license. Documentation was the reference | No source copied from it |
| `Brendonm17/rsc-c-vita` | AGPL-3.0. Cited as the QuickJS-on-Vita approach | No file from that repository was identified in this tree |
| Server bundles | Bundled revision 225 world using `@2004scape/rsbuf` and Lost City module names. Upstream server and content repos are MIT | Bundles preserved. MIT notice is not yet inside the bundles |
| Vendored C in `src/thirdparty/` | Notices are inside the files | Left in place. Seven files differ from Client3 `53c4cf8` and carry the export's bytes |
| QuickJS, vitaGL, VitaSDK | Linked, not vendored here | Documented as external |
| Caches, models, maps, audio, VPKs, saves | The build 244 VPK and the local asset folder were not imported. This fork already contains Client3's `rom/` because GitHub copied the upstream repository | Nothing from `RS2_Ver_0.92.vpk` was added. Upstream `rom/` was not deleted and history was not rewritten |
| Secrets | No private keys, cloud tokens, or personal addresses in the source scan | None removed, because none were found |
| Historical zip | Source-only archive still reachable from an older commit | History left intact |

## Client sources

`src/` contains the Client3 file set (`entry/`, `platform/` including 3ds, dreamcast, nds, psp, sdl, wii, xbox, and vita, plus `thirdparty/` and `wordenc/`) together with the Vita-specific files added for build 244.

Checked on 2 November 2026:

- `lesleyrs/Client3` has no GitHub license file.
- `RustCityRS/client-c` is a fork of Client3 and also has no license file.
- `Brendonm17/Client3-Vita` is a fork of Client3 and has no license file.

Without a license, those authors have not granted rights to copy or relicense the work. Keeping the files in a repository that already contained them preserves the author's own published checkpoint. It does not make the client reusable by others. Anyone who wants to fork the client portion needs permission from the Client3 author, and needs to respect Jagex's rights in the game itself.

RuneScape, the original client, and the official cache are Jagex property. This export omits caches, models, maps, audio, and executables. Omitting those files does not by itself decide the status of a C translation of the client.

## RSC Vita client

`Brendonm17/rsc-c-vita` is AGPL-3.0. The project notes say that client suggested how to embed QuickJS on Vita. The embed in this tree is `src/embedded_server.c`, and the QuickJS library is an external archive. A search of this export did not find RSC Vita source files by name. That is not a clean-room proof. If a later comparison shows a copied AGPL file, that file remains AGPL-3.0 and the distribution terms of that license apply to it. This branch does not relicense anything.

## Server bundles

Both JavaScript files are esbuild bundles. They contain `node-forge`, and the world portion requires `@2004scape/rsbuf` and `@2004scape/rsmod-pathfinder`. The default `ENGINE_REVISION` is 225 and the default database name in the bundle is `lostcity`.

On 2 November 2026, GitHub reported MIT for:

- `2004Scape/Server`
- `LostCityRS/Server`
- `LostCityRS/Content`
- `LostCityRS/Client-TS`

MIT applies to the Lost City engine code when the copyright and permission notice travels with the copy. The bundles are generated and do not embed that notice. [server/NOTICE.md](../server/NOTICE.md) supplies it, plus the notices for `@2004scape/rsbuf` (MIT), `@2004scape/rsmod-pathfinder` (ISC), and node-forge (BSD terms of its dual license). That file does not license the C client.

`src/util/Environment.ts` inside both bundles matches [LostCityRS/Engine-TS](https://github.com/LostCityRS/Engine-TS) commit `e1dea19f256c7ff1a89d47024c811c755ad2184d` on branch `225` (23 February 2026), including two upstream typos (`WEB_SOCKET_TOKEN_RPOTECTION` and `WEB_SOCKET_TOKEN_PROTECTECTION`). The previous revision of that file, `b502762d6ff29619081bb4c0402008de0201513a`, does not match. The npc-unload bundle also contains local modules (`embedded/`, `source/server-experimental/engine/`), so neither bundle is an unmodified upstream release.

`node-forge` is vendored inside the bundles. Its own notice needs to remain available with that code. GitHub's license API did not return a SPDX id for `digitalbazaar/forge` during this audit, so this note does not assign one.

The bundles are code. They are not a cache archive. The runtime resource pack the Vita world asks for is not in this branch. Client3's `rom/cache` is already in the fork because the branch is based on upstream.

## Notices already inside the tree

These headers were read from the extracted `src/thirdparty/` files:

| Files | Notice in the file |
| --- | --- |
| `ini.c`, `ini.h` | Copyright 2016 rxi, MIT-style permission text |
| `isaac.c` | Bob Jenkins, March 1996, public domain |
| `mpi.c` and the `tommath` headers | Unlicense SPDX tags |
| `stb_image.h` | stb_image v2.30, public domain |
| `tsf.h` | TinySoundFont, MIT, copyright Bernhard Schelling |
| `bzip.c` | micro-bunzip by Rob Landley. The header in this copy says LGPLv2 |

The bzip header matters for a statically linked Vita binary: distributing `eboot.bin` can trigger LGPL obligations for that library. The binary is not in this repository. The header must stay with the source.

## External libraries named by the build

- QuickJS (`libquickjs.a`, `vendor/quickjs-vita/quickjs.h`). Bellard's QuickJS publishes its own terms. Confirm which tree produced the `.a` before vendoring it.
- vitaGL, vitaShaCC, and VitaSDK. Their licenses apply to the toolchain and the linked GL library. They are not vendored here.
- LibTomMath is compiled in via `-DWITH_RSA_LIBTOM`. The sources in `thirdparty/` carry Unlicense tags.

## Archives and git history

`provenance/src-tree.rar` is the archive that was previously at the repository root. Extracting it produced 335 files. Every file hash matches `SOURCE-MANIFEST.json`.

The commit `b400994fb0e9170a894b2e9acb427de519740028` still contains `Runescape-2-build-244-377.zip` (1,462,242 bytes). Commit `1653874d` deletes it from the tip. The zip listing contains only `.c`, `.h`, `.inc`, `.js`, `.md`, `.json`, `.sh`, and `.mk`. It does not contain a VPK, cache, model, or save. Because the blob remains in history, GitHub can still serve that commit. This branch does not rewrite history. Removing the blob from public history requires an explicit decision and a force-push, which was not done.

## Local files that were not imported

A separate local Vita folder holds OpenRS2 cache archives, model archives, client ZIPs, and many VPK builds. None of those files were copied into this clone. `.gitignore` now ignores VPK, save, cache, and key filenames so a later add does not pick them up by accident.

## Sensitive data

A scan of the tracked text sources for private-key headers, AWS-style access key prefixes, GitHub tokens, Slack tokens, and Discord webhooks found no matches. The author email is already in the git commits and was not copied into the new documents. Device paths such as `ux0:/data/RS2Vita/` are install locations, not credentials.

`server.bundle.tickparts.js` contains the word "password" inside vendored `node-forge` PKCS routines. Those are library functions, not stored credentials.

## Permissions still required

1. Permission from the Client3 author before anyone treats the C client as licensed for reuse.
2. The Lost City MIT notice, the rsbuf MIT notice, the rsmod-pathfinder ISC notice, and the node-forge BSD notice are in `server/NOTICE.md`. The whole bundle is still not pinned to one upstream commit. Only `Environment.ts` matches `e1dea19f`.
3. A decision on any AGPL exposure if RSC Vita source was copied.
4. Jagex's rights remain in the game and its assets. Do not add caches, models, maps, audio, or official executables.
5. LGPL duties for micro-bunzip if a binary that statically links it is distributed.
6. No project-wide license until the points above are settled. Adding one now would claim rights the checkpoint does not show.
