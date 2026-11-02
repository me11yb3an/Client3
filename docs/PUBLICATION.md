# Publication checklist

Date: 2 November 2026. The branch `migration/vita-build-244` carries the build 244 overlay and the asset-free patch. History was not rewritten.

The asset-free patch removes `rom/cache/` and `rom/SCC1_Florestan.sf2` from the tip of this branch. Those blobs remain in the parent commits and in `lesleyrs/Client3`. `rom/Roboto/NOTICE` names Google as the copyright holder. `rom/config.ini` has an empty password on this branch. The previous password remains in `53c4cf8`.

Publishing this branch and merging it into `main` was authorized. That does not add a license, and it does not resolve the Client3 redistribution question in [CLIENT3-REDISTRIBUTION.md](CLIENT3-REDISTRIBUTION.md). No VPK is attached.

Working source on this branch is revision 225, build 244. Revision 377 is not in this tree and is not part of this checklist.

## Ready

- [x] Fork relationship recorded: `me11yb3an/Client3` is a public fork of `lesleyrs/Client3`. The upstream commit this branch starts from is `53c4cf8`.
- [x] Build 244 overlay is on `migration/vita-build-244`.
- [x] `server/NOTICE.md` still carries the Lost City MIT notice, the rsbuf MIT notice, the rsmod-pathfinder ISC notice, and the node-forge BSD notice.
- [x] Vendored headers still carry their own notices: ini (rxi, MIT), ISAAC (Bob Jenkins, public domain), LibTomMath (Unlicense), stb_image (public domain), TinySoundFont (MIT), micro-bunzip (LGPLv2 header).
- [x] `provenance/src-tree.rar` contains 335 files: 175 `.h`, 105 `.c`, 55 `.inc`. No image, audio, font, archive, save, package, or key file. A text scan found no private-key header and no token prefix.
- [x] `RS2_Ver_0.92.vpk` and its resource packs, save, and `libshacccg.suprx` are not in the branch.
- [x] No root `LICENSE` was added.

## Build, not a successful compile

`make -f units.mk -n` parsed `units.mk` and then stopped because `src/r63_search_icon.h` is missing. Exit code 2. The dry run did not compile. `link.sh` passes `bash -n`. A later look found VitaSDK 15.2.0 in WSL at `/usr/local/vitasdk`. That did not become a completed compile. The missing headers and link inputs below were not invented.

A later VitaSDK run still needs, from outside this repository:

- VitaSDK itself, installed from the VitaSDK project.
- Headers and libraries under `/tmp/rs225-codex-r15` and `/tmp/rs225-codex-bridge`: `libquickjs.a`, `vendor/quickjs-vita/quickjs.h`, `libvita225_native_bridge.a`, `libbz2.a`, `vita_crypto.o`, and `vita-segment-gap.ld`.
- `libshacccg.suprx` on the console, not in git.
- The runtime resource pack expected by `ba_resource_init()`, and `rom/vita-enhanced.bin`, which `src/vita_enhanced.c` opens. That binary is described in source as original-asset panels. Keep it on the build machine.

`libbz2.a` can be built from upstream bzip2. QuickJS can be built from its own sources once the exact tree and its license are recorded. The native bridge, `vita_crypto.o`, and the segment linker script are not in the export. They have to come from the machine that produced build 244, or the link line has to be changed and tested. This review did not invent replacements.

## Generated headers

These five headers embed pixels and are absent on purpose. Do not regenerate them by copying cache sprites into the repository.

| Header | What the source uses | Way to supply it without committing unauthorized art |
| --- | --- | --- |
| `src/r63_search_icon.h` | `r63_search_alpha[]`, indexed as a 28-pixel-wide mask | Convert a locally owned 28-pixel-wide alpha image to a C array on the build machine, or replace the lookup with a small drawn shape in `platform/vita.c`. |
| `src/r27_cursor_images.h` | `r27_cursor_images[5]`, each a `R27Cursor` | Convert five locally owned cursor images, or draw one procedural cursor in `r27_cursor.c`. |
| `src/runner_icon.generated.h` | `runner_pixels`, `RUNNER_WIDTH`, `RUNNER_HEIGHT` | Convert a locally owned icon. The call site labels it a supplied runner. |
| `src/custom_run_stone.generated.h` | `custom_run_stone_2`, 40 by 36 | Convert the supplied stone image. The comment calls it a supplied red stone. |
| `src/vita_map_aperture.generated.h` | `ve_frame_argb[]` and `ve_map_aperture[]`, 188 by 184 | Generate a geometric mask in `circular_minimap_geometry.h`, which already defines those dimensions, or convert a locally owned mask. |

`src/dialogue_geometry.generated.h` and `src/ui474_widgets.generated.h` are already in the tree. They are layout tables, not the missing image headers.

## Inherited `rom/`

Upstream `main` contained 1,084 `rom/` files, about 11.6 MB. This branch did not add them. Its tip keeps the Roboto fonts and removes the cache and the soundfont. Older commits still have the removed files.

| Path | Contents | License finding | Recommendation |
| --- | --- | --- | --- |
| `rom/Roboto/` | 12 `Roboto-*.ttf` files. The name table of `Roboto-Regular.ttf` says Copyright 2011 Google Inc. `LICENSE.txt` is Apache-2.0. | Apache-2.0 applies to these fonts. | Kept. `rom/Roboto/NOTICE` supplies the copyright line the license appendix left blank. |
| `rom/SCC1_Florestan.sf2` | Florestan Basic GM GS, sampled from a Roland Sound Canvas. | The public-domain label is not settled. | Removed from this branch's tip. The blob remains in history and in `lesleyrs/Client3`. |
| `rom/cache/` | Cache archives, 828 map files, 182 songs, 48 jingles, and `worldmap.jag`. | No license was found. | Removed from this branch's tip. The blobs remain in history and in `lesleyrs/Client3`. |
| `rom/config.ini` | Client options. Upstream stored a username and a password. | The password is a 9-letter token next to the username `shared`. It is the value Client3 already publishes, not a newly found private key. It is still a login password, so this branch stores an empty value. | Cleared on this branch. The old value remains in `53c4cf8`. |

## What a GitHub fork authorizes

GitHub's Terms of Service, effective 27 April 2026, section D.5: making a repository public grants other users a license to use, display, perform, and reproduce that content by forking, through the Service, as GitHub's functionality permits. The same section says further rights come from adopting a license. Section D.6 says that adding content to a repository that has a license is a statement that you have the right to license that content under those terms.

GitHub's own licensing guidance says that a repository with no license remains under default copyright, and that other people may not reproduce, distribute, or create derivative works from it. The right GitHub documents on top of that is to view the repository and to fork it.

Client3 has no `LICENSE`. Its readme does not grant modification or redistribution. No written grant from its author is in this tree. The fork at `me11yb3an/Client3` is the D.5 copy of upstream. It does not license the modified Client3 files, and it does not authorize a repository-wide MIT, GPL, or other license.

Section D.3 still requires the person who posts content to have the right to post it and to comply with third-party licenses. A fork of Client3-Vita, or the fact that Client3 is already public, is not that right. That question stays open in [CLIENT3-REDISTRIBUTION.md](CLIENT3-REDISTRIBUTION.md). No root license was added to paper over it.

## Still open after publication

1. There is still no license or written grant for the files in [CLIENT3-REDISTRIBUTION.md](CLIENT3-REDISTRIBUTION.md). The practical channel is the author of [lesleyrs/Client3](https://github.com/lesleyrs/Client3).
2. The cache, music, maps, and Florestan soundfont are absent from this tip. They remain in older commits and upstream. History was not rewritten.
3. The five image headers and `rom/vita-enhanced.bin` still have to be supplied on the build machine. The steps are in [BUILD-DEPS.md](BUILD-DEPS.md).
4. A downloadable VPK is not part of this publication. [VPK-RELEASE-DRAFT.md](VPK-RELEASE-DRAFT.md) is text only.
