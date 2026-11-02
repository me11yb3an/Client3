# Build dependencies for revision 225, build 244

This checklist is what a VitaSDK build of this branch still needs. Nothing here was compiled. `make -f units.mk -n` parses `units.mk`, prints compile lines, and stops with exit code 2 because `src/r63_search_icon.h` is missing. `bash -n link.sh` succeeds. The compiler named by `units.mk` is `/usr/local/vitasdk/bin/arm-vita-eabi-gcc`, and that program is not on the machine that checked this file.

Revision 377 is not part of this build.

## 1. Toolchain

- [ ] Install VitaSDK from the VitaSDK project.
- [ ] Provide `arm-vita-eabi-gcc` at `/usr/local/vitasdk/bin/arm-vita-eabi-gcc`, or change `units.mk` after a tested run.
- [ ] Provide `arm-vita-eabi-strip`, `vita-elf-create`, and `vita-make-fself` for the `eboot.bin` target.

## 2. Generated headers, absent on purpose

Do not fill these by committing sprites taken from a RuneScape cache. Each one can be produced on the build machine from art you have the right to use, or replaced by a shape drawn in source.

- [ ] `src/r63_search_icon.h` — `r63_search_alpha[]`, 28 pixels wide. `units.mk` lists this as a prerequisite of `out/platform/vita.o`, so make stops here today.
- [ ] `src/r27_cursor_images.h` — `r27_cursor_images[5]`, type `R27Cursor` in `src/r27_cursor.h`.
- [ ] `src/runner_icon.generated.h` — `runner_pixels`, `RUNNER_WIDTH`, `RUNNER_HEIGHT`, included by `src/ui474.c`.
- [ ] `src/custom_run_stone.generated.h` — `custom_run_stone_2`, 40 by 36, included by `src/ui474.c`.
- [ ] `src/vita_map_aperture.generated.h` — `ve_frame_argb[]` and `ve_map_aperture[]`, 188 by 184. The dimensions are `VE_FRAME_W188` and `VE_FRAME_H184` in `src/circular_minimap_geometry.h`.

`src/dialogue_geometry.generated.h` and `src/ui474_widgets.generated.h` are already in the tree.

## 3. External link inputs

`link.sh` still names these paths. They are not in the repository. The reproducible source for each one is stated beside it. Objects that exist only on the build 244 machine cannot be recreated from this tree.

- [ ] `libbz2.a` at `/tmp/rs225-codex-r15/arm/libbz2.a`. Build it from upstream bzip2. The in-tree decompressor is `src/thirdparty/bzip.c`, which keeps its LGPLv2 header.
- [ ] `libquickjs.a` at `/tmp/rs225-codex-r15/arm/libquickjs.a`, and `vendor/quickjs-vita/quickjs.h`, which `src/embedded_server.c` includes. Build QuickJS from the tree that matches that header, and record that tree's license before copying it in.
- [ ] `libvita225_native_bridge.a` at `/tmp/rs225-codex-r15/arm/libvita225_native_bridge.a`. Not in the export. Copy it from the build 244 machine, or remove its link and retest.
- [ ] `vita_crypto.o` at `/tmp/rs225-codex-r15/arm/vita_crypto.o`. Same constraint as the native bridge.
- [ ] `vita-segment-gap.ld` at `/tmp/rs225-codex-r15/source/desktop-tests/prayer-client/vita-segment-gap.ld`. Same constraint.
- [ ] Include directories `/tmp/rs225-codex-r15/bridge-poc`, `/tmp/rs225-codex-r15/work/src`, and `/tmp/rs225-codex-bridge`. Same constraint.
- [ ] vitaGL and vitaShaCC libraries named on the link line (`-lvitaGL`, `-lvitashark`, `-lmathneon`, and the Sce stubs). Install them with VitaSDK.
- [ ] `libshacccg.suprx` on the console for the GL path. Do not commit it.

## 4. Runtime files that stay off git

- [ ] The resource pack `src/offline_vita.c` requires from `ba_resource_init()`.
- [ ] `rom/vita-enhanced.bin`, opened by `src/vita_enhanced.c`. The source describes it as original-asset panels.

## 5. Makefiles after the asset removal

`units.mk` and `link.sh` do not read `rom/cache` or `rom/SCC1_Florestan.sf2`.

`vita.mk` is the upstream package rule. It still passes `rom/cache` and `rom/SCC1_Florestan.sf2` to `vita-pack-vpk`. Those paths are not in this branch. `make -f vita.mk` is not the build 244 command, and it will not pack until that rule is changed in a later tested edit.

The unmodified checkpoint scripts remain `provenance/checkpoint-units.mk` and `provenance/checkpoint-link.sh`.
