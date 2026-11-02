# Publication checklist

Date: 2 November 2026. Branch `migration/vita-build-244` at the review that produced this note. Nothing in this checklist was pushed. No upstream file was deleted. History was not rewritten.

Working source on this branch is revision 225, build 244. Revision 377 is not in this tree and is not part of this checklist.

## Ready

- [x] Fork relationship recorded: `me11yb3an/Client3` is a public fork of `lesleyrs/Client3`. `main` is still `53c4cf8`.
- [x] Build 244 overlay is only on the local migration branch.
- [x] `server/NOTICE.md` still carries the Lost City MIT notice, the rsbuf MIT notice, the rsmod-pathfinder ISC notice, and the node-forge BSD notice.
- [x] Vendored headers still carry their own notices: ini (rxi, MIT), ISAAC (Bob Jenkins, public domain), LibTomMath (Unlicense), stb_image (public domain), TinySoundFont (MIT), micro-bunzip (LGPLv2 header).
- [x] `provenance/src-tree.rar` contains 335 files: 175 `.h`, 105 `.c`, 55 `.inc`. No image, audio, font, archive, save, package, or key file. A text scan found no private-key header and no token prefix.
- [x] `RS2_Ver_0.92.vpk` and its resource packs, save, and `libshacccg.suprx` are not in the branch.
- [x] No root `LICENSE` was added.

## Build, not a successful compile

`make -f units.mk -n` parsed `units.mk` and then stopped because `src/r63_search_icon.h` is missing. Exit code 2. The dry run did not compile. `link.sh` passes `bash -n`. The compiler path `/usr/local/vitasdk/bin/arm-vita-eabi-gcc` does not exist on the machine that ran the check.

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

1,084 files, about 11.6 MB. This branch did not add them. They are in the published fork because `main` is Client3.

| Path | Contents | License finding | Recommendation |
| --- | --- | --- | --- |
| `rom/Roboto/` | 12 `Roboto-*.ttf` files. The name tables contain Roboto, Google, and Apache. `LICENSE.txt` is the Apache-2.0 text. Its appendix still has the blank sample copyright line. | Apache-2.0 is the appropriate license for Google Roboto. The appendix does not name the copyright holder. | Keep the fonts. Add a short notice that names Google as the copyright holder. That notice is not in the tree yet. |
| `rom/SCC1_Florestan.sf2` | Internal name `Florestan Basic GM GS`, with the historical Florestan URLs. | Musical Artifacts labels the bank public domain. The bank is sampled from a Roland Sound Canvas. A 2026 derivative says those samples are Microsoft GS wavetable data and that Roland's Sound Canvas set is licensed for Microsoft operating-system products unless Roland grants another license. The public-domain label is not settled. | Do not describe this file as cleared. Leave it until you choose a replacement bank with a license you can show. |
| `rom/cache/` | Cache archives (`config`, `media`, `models`, `textures`, `title`, `interface`, `sounds`, `wordenc`), 828 map files, 182 files in `songs/`, 48 files in `jingles/`, and `mapview/worldmap.jag`. The MIDI names are RuneScape track names. | No license was found. These are game assets. | Do not treat them as licensed for this project. |
| `rom/config.ini` | Client options, including a remembered username and password. | Not a copyright license. It is a credential in a public file. | Replace the password with an empty placeholder before relying on this tree as the public copy. The current value remains in Client3 history and in this fork's `main`. |

## What a GitHub fork authorizes

GitHub's Terms of Service, effective 27 April 2026, section D.5: making a repository public grants other users a license to use, display, perform, and reproduce that content by forking, through the Service, as GitHub's functionality permits. The same section says further rights come from adopting a license. Section D.6 says that adding content to a repository that has a license is a statement that you have the right to license that content under those terms.

GitHub's own licensing guidance says that a repository with no license remains under default copyright, and that other people may not reproduce, distribute, or create derivative works from it. The right GitHub documents on top of that is to view the repository and to fork it.

Client3 has no `LICENSE`. Its readme does not grant modification or redistribution. No written grant from its author is in this tree. The fork at `me11yb3an/Client3` is the D.5 copy of upstream. It does not authorize publishing the modified Client3 files on `migration/vita-build-244`, and it does not authorize a repository-wide MIT, GPL, or other license.

Section D.3 still requires the person who posts content to have the right to post it and to comply with third-party licenses. A fork of Client3-Vita, or the fact that Client3 is already public, is not that right.

## Decisions required before publishing the Vita branch

1. Obtain a license or a written grant covering the Client3 files this branch copies and modifies, or accept that the branch stays unpublished.
2. Choose what to do with inherited `rom/cache`, the 230 MIDI files, and `worldmap.jag`. Leaving them keeps the faithful fork. Removing them takes a separate approved commit, and the blobs remain reachable from `main` and from upstream history. A fork cannot be made private by itself. A history rewrite was not done and is not proposed here.
3. Choose whether `rom/SCC1_Florestan.sf2` stays as inherited upstream material or is later replaced by a bank with a license you can show.
4. Approve a short Google copyright notice beside `rom/Roboto/`.
5. Approve clearing the remembered password in `rom/config.ini`.
6. Decide how the five image headers and `rom/vita-enhanced.bin` will be supplied on the build machine. Do not commit extracted game art to fill those gaps.
7. Say explicitly when `migration/vita-build-244` may be pushed. Until then it stays local.
