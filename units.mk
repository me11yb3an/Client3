CC := /usr/local/vitasdk/bin/arm-vita-eabi-gcc
CFLAGS := -c -O2 -ffast-math -g -fwrapv -std=c99 -DGL11 -DWITH_RSA_LIBTOM -Dclient -DBUILD_A_EMBEDDED -DVITA_NATIVE_WIDESCREEN -DBA_R9_UPLOAD_MODE=0 -DBA_R10_CACHE=0 -DBA_R10_COMPACT=0 -DBA_R11_DIAG=1 -DBA_R11_SHARED_STATIC=0 -DBA_R12_DIAG=1 -DBA_R12_PROBE=0 -DBA_R12_REBASE=0 -I/tmp/rs225-codex-r15/bridge-poc -I/tmp/rs225-codex-bridge -Isrc -Isave
all: \
	out/ba_hw_r6.o \
	out/ba_resource.o \
	out/ba_shader_loader.o \
	out/clientstream.o \
	out/custom.o \
	out/entry/client.o \
	out/gameshell.o \
	out/gl11.o \
	out/h8_remote.o \
	out/pixmap.o \
	out/platform/vita.o \
	out/r10_camera.o \
	out/r11_diag.o \
	out/r12_waits.o \
	out/r13_capture.o \
	out/r16_fast.o \
	out/title_theme.o \
	out/vita_title.o \
	out/ui474.o \
	out/r27_cursor.o \
	out/vita_enhanced.o \
	out/world3d.o \
	out/offline_vita.o

out/ba_hw_r6.o: src/ba_hw_r6.c
	$(CC) $(CFLAGS) -o $@ src/ba_hw_r6.c

out/ba_resource.o: src/ba_resource.c
	$(CC) $(CFLAGS) -o $@ src/ba_resource.c

out/ba_shader_loader.o: src/ba_shader_loader.c
	$(CC) $(CFLAGS) -o $@ src/ba_shader_loader.c

out/clientstream.o: src/clientstream.c
	$(CC) $(CFLAGS) -o $@ src/clientstream.c

out/entry/client.o: src/r19_popup_state.inc src/entry/client.c src/vita_enhanced.h src/ui474.h src/r17_menu_overlay.inc
	$(CC) $(CFLAGS) -o $@ src/entry/client.c
out/entry/client.o: src/r45_path_recovery.h

out/gameshell.o: src/gameshell.c src/ba_pacing.inc src/ba_cadence_queue.inc
	$(CC) $(CFLAGS) -o $@ src/gameshell.c
out/gameshell.o: src/r44_scene_loading_clock.h

out/gl11.o: src/r40_water_gate.h src/gl11.c src/vita_enhanced.h
	$(CC) $(CFLAGS) -o $@ src/gl11.c

out/h8_remote.o: src/h8_remote.c
	$(CC) $(CFLAGS) -o $@ src/h8_remote.c

out/pixmap.o: src/r19_tiled_ui.inc src/r19_region_diff.inc src/pixmap.c src/pixmap.h
	$(CC) $(CFLAGS) -o $@ src/pixmap.c

out/platform/vita.o: src/platform/vita.c src/vita_button_edges.h src/vita_title_layout.h
	$(CC) $(CFLAGS) -o $@ src/platform/vita.c

out/r10_camera.o: src/r10_camera.c
	$(CC) $(CFLAGS) -o $@ src/r10_camera.c

out/r11_diag.o: src/r11_diag.c
	$(CC) $(CFLAGS) -o $@ src/r11_diag.c

out/r12_waits.o: src/r12_waits.c
	$(CC) $(CFLAGS) -o $@ src/r12_waits.c

out/r13_capture.o: src/r13_capture.c src/r13_capture.h
	$(CC) $(CFLAGS) -o $@ src/r13_capture.c

out/r16_fast.o: src/r20_paving.h src/r35_compact_patterns.inc src/r19_live_tile_index.inc src/r16_fast.c src/r18_actor_contact.inc src/r18_player_ground.inc src/r18_model_water.inc src/r16_furniture_layers.inc src/r16_live_doors.inc src/r16_roof_cache.inc src/r16_coplanar.h
	$(CC) $(CFLAGS) -o $@ src/r16_fast.c

out/title_theme.o: src/title_theme.c src/vita_title.h
	$(CC) $(CFLAGS) -o $@ src/title_theme.c

out/ui474.o: src/ui474.c src/ui474.h
	$(CC) $(CFLAGS) -o $@ src/ui474.c

out/vita_enhanced.o: src/r19_chat_background.inc src/vita_enhanced.c src/vita_enhanced.h src/vita_map_aperture.generated.h
	$(CC) $(CFLAGS) -o $@ src/vita_enhanced.c

out/world3d.o: src/world3d.c src/r34_normal_index.h
	$(CC) $(CFLAGS) -o $@ src/world3d.c

out/loctype.o: src/loctype.c
	$(CC) $(CFLAGS) -o $@ src/loctype.c

out/world.o: src/world.c src/r25_wall_decor.h
	$(CC) $(CFLAGS) -o $@ src/world.c

out/offline_vita.o: src/offline_vita.c src/ba_hw_r2.inc src/ba_hw_r3.inc src/ba_hw_r4.inc src/ba_hw_r5.inc src/ba_hw_r7.inc
	$(CC) $(CFLAGS) -o $@ src/offline_vita.c

# Server and save objects are not part of `all`. `make client.elf` rebuilds
# any of them that are older than their sources, then runs link.sh. The JS
# bundle is loaded from the package at runtime and is not an input of this link.
STRIP := /usr/local/vitasdk/bin/arm-vita-eabi-strip
ELF_CREATE := /usr/local/vitasdk/bin/vita-elf-create
FSELF := /usr/local/vitasdk/bin/vita-make-fself
SERVER_CFLAGS := -O2 -g -std=c11 -ffunction-sections -fdata-sections -Isave

arm/embedded_server.o: src/embedded_server.c src/world_worker.h save/save_commit.h save/save_login.h save/active_save.h
	$(CC) $(CFLAGS) -o $@ src/embedded_server.c

arm/g17_cache_io.o: g17_cache_io.c
	$(CC) -O2 -g -std=c99 -c -o $@ g17_cache_io.c

arm/save_format.o: save/save_format.c save/save_format.h
	$(CC) $(SERVER_CFLAGS) -c -o $@ save/save_format.c

arm/save_commit.o: save/save_commit.c save/save_commit.h save/save_format.h save/save_restore.h
	$(CC) $(SERVER_CFLAGS) -c -o $@ save/save_commit.c

arm/save_restore.o: save/save_restore.c save/save_restore.h
	$(CC) $(SERVER_CFLAGS) -c -o $@ save/save_restore.c

arm/save_login.o: save/save_login.c save/save_login.h
	$(CC) $(SERVER_CFLAGS) -c -o $@ save/save_login.c

arm/active_save.o: save/active_save.c save/active_save.h
	$(CC) $(SERVER_CFLAGS) -c -o $@ save/active_save.c

client.elf: arm/embedded_server.o arm/g17_cache_io.o arm/save_format.o arm/save_commit.o arm/save_restore.o arm/save_login.o arm/active_save.o out/offline_vita.o out/ba_hw_r6.o out/ba_resource.o out/ba_shader_loader.o out/clientstream.o out/custom.o out/entry/client.o out/gameshell.o out/gl11.o out/h8_remote.o out/pixmap.o out/platform/vita.o out/r10_camera.o out/r11_diag.o out/r12_waits.o out/r13_capture.o out/r16_fast.o out/title_theme.o out/vita_title.o out/ui474.o out/r27_cursor.o out/vita_enhanced.o out/world3d.o out/loctype.o out/world.o link.sh
	bash link.sh

eboot.bin: client.elf
	cp client.elf client-strip.elf
	$(STRIP) -g client-strip.elf
	$(ELF_CREATE) client-strip.elf client.velf
	$(FSELF) client.velf eboot.bin

out/vita_title.o: src/vita_title.c src/vita_title_layout.h src/vita_title.h src/r20_title_cache.inc
	$(CC) $(CFLAGS) -o $@ src/vita_title.c


out/custom.o: src/custom.c src/custom.h
	$(CC) $(CFLAGS) -o $@ src/custom.c

out/r16_fast.o: src/r20_paving.h src/gl11.h
out/gl11.o: src/gl11.h

out/r13_capture.o: src/r21_capture_codec.h src/r13_capture.h
out/loctype.o: src/r21_location_visual.h src/loctype.h
out/world.o: src/loctype.h
out/entry/client.o: src/loctype.h src/r13_capture.h

out/entry/client.o out/ui474.o: src/r23_tutorial.h

out/r16_fast.o: src/r24_door_states.h

out/clientstream.o: src/offline_stream.inc

out/r16_fast.o: src/r25_wall_decor.h

out/r16_fast.o: src/r26_wall_decals.inc
out/r16_fast.o: src/r42_planar_index.h

out/r27_cursor.o: src/r27_cursor.c src/r27_cursor.h src/r27_cursor_images.h
	$(CC) $(CFLAGS) -o $@ src/r27_cursor.c
out/ui474.o out/platform/vita.o: src/r27_cursor.h

out/r16_fast.o: src/r28_window_planes.inc src/r29_live_furnace.inc src/r31_static_compact.inc

out/r16_fast.o: src/r32_roof_runtime.inc src/r32_roof_zones.h src/r32_index_masks.inc

out/r16_fast.o: src/r46_floor_partition.h src/r47_floor_contacts.inc src/r48_source_ids.h src/r49_floor_supports.h src/r50_wall_fit.h src/r50_wall_furniture.inc src/r50_furniture_types.h

out/r16_fast.o: src/r51_openings.inc src/r51_opening_clip.h src/r51_opening_types.h

out/r16_fast.o: src/r52_underground.inc

out/r16_fast.o: src/r53_ship_priority.inc src/r53_ship_priority.h

out/r16_fast.o: src/r54_crates.h src/r54_crane_duplicates.inc

out/r16_fast.o: src/r55_fixture_contacts.inc

out/r16_fast.o: src/r56_hull_layers.inc src/r56_hull_types.h

# Actor material candidates change Model layout and actor construction.
$(filter %.o,$(wildcard out/*.o out/entry/*.o out/platform/*.o)): src/model.h
out/model.o: src/r77_body_selection_cache.inc src/r75_hover_bounds.h src/r57_body_layers.inc src/r57_body_layers.h src/r16_coplanar.h
out/npctype.o out/playerentity.o out/r16_fast.o: src/r57_body_layers.h

out/r16_fast.o: src/r57_body_render.inc

R57_MODEL_OBJECTS := out/allocator.o out/animbase.o out/animframe.o out/ba_hw_r6.o out/ba_native_memory.o out/ba_resource.o out/ba_server_owner.o out/ba_shader_loader.o out/classic_ui.o out/clientstream.o out/collisionmap.o out/component.o out/custom.o out/entity.o out/flotype.o out/gameshell.o out/gl11.o out/ground.o out/h8_protocol.o out/h8_remote.o out/idktype.o out/inputtracking.o out/jagfile.o out/layoutcore.o out/layoutcore_draw.o out/locentity.o out/locmergeentity.o out/loctype.o out/model.o out/npcentity.o out/npctype.o out/objstackentity.o out/objtype.o out/offline_vita.o out/packet.o out/pathingentity.o out/pix24.o out/pix2d.o out/pix3d.o out/pix8.o out/pixfont.o out/pixmap.o out/platform.o out/playerentity.o out/projectileentity.o out/r10_camera.o out/r10_scene.o out/r11_diag.o out/r12_waits.o out/r13_capture.o out/r16_fast.o out/r27_cursor.o out/seqtype.o out/spotanimentity.o out/spotanimtype.o out/tileoverlay.o out/tileunderlay.o out/title_theme.o out/ui474.o out/ui474_parchment.o out/varptype.o out/vita-telemetry.o out/vita_dialogue.o out/vita_enhanced.o out/vita_title.o out/world.o out/world3d.o out/datastruct/doublylinkable.o out/datastruct/doublylinklist.o out/datastruct/hashtable.o out/datastruct/jstring.o out/datastruct/linkable.o out/datastruct/linklist.o out/datastruct/lrucache.o out/entry/client.o out/entry/midi.o out/entry/playground.o out/platform/3ds.o out/platform/dreamcast.o out/platform/dummy.o out/platform/emscripten.o out/platform/nds.o out/platform/psp.o out/platform/sdl1.o out/platform/sdl2.o out/platform/sdl3.o out/platform/vita.o out/platform/webassembly.o out/platform/wii.o out/platform/xbox.o out/sound/envelope.o out/sound/tone.o out/sound/wave.o out/thirdparty/bn.o out/thirdparty/bzip.o out/thirdparty/ini.o out/thirdparty/isaac.o out/thirdparty/mpi.o out/thirdparty/rsa-bigint.o out/thirdparty/rsa-libtom.o out/thirdparty/rsa-openssl.o out/thirdparty/rsa-tiny.o out/wordenc/wordfilter.o out/wordenc/wordpack.o
$(R57_MODEL_OBJECTS): src/model.h
client.elf: $(R57_MODEL_OBJECTS)
out/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ $<

out/r16_fast.o: src/r58_dyn_index.inc
out/platform/vita.o: src/r60_latest_analog.h

out/r16_fast.o: src/r62_shared_crops.inc src/r62_crop_types.h

out/platform/vita.o: src/r63_keyboard_layout.h src/r63_search_icon.h

arm/embedded_server.o: src/r64_banker_defaults.h

out/r16_fast.o: src/r65_bank_fixtures.h

out/r16_fast.o: src/r65_bank_shading.inc

out/loctype.o out/world.o: src/loctype.h src/r66_scenery_visuals.h
out/loctype.o out/r16_fast.o: src/r57_body_layers.h src/r66_scenery_visuals.h
out/r16_fast.o: src/r69_roof_layers.inc
out/r16_fast.o: src/r70_plant_contact.inc
out/r16_fast.o: src/r71_shore_types.h src/r71_shore_plants.inc
out/r16_fast.o: src/r73_animated_view.h

out/gl11.o out/r16_fast.o: src/r79_actor_indices.inc src/gl11.h

out/model.o out/gl11.o: src/r80_exact_words.h

out/r16_fast.o: src/r78_fur_types.h

out/model.o out/r16_fast.o: src/r82_project_divide.h

out/r16_fast.o: src/r83_project_bounds.h

out/pixmap.o: src/r80_exact_words.h

out/model.o out/loctype.o out/r16_fast.o: src/r87_vendor_layers.h src/r57_body_layers.h

out/platform/vita.o: src/r89_stone_cycle.h src/vita_button_edges.h

out/loctype.o out/r16_fast.o: src/r90_sewing_table.h

out/entry/client.o: src/r91_popup_dismiss.inc src/r92_examine_speech.inc

out/gl11.o: src/r93_lava_scroll.h
out/entry/client.o out/sound/wave.o: src/sound/wave.h

out/entry/client.o: src/r94_audio_cost.h
out/r16_fast.o: src/r100_crypt_fixtures.h
out/loctype.o: src/r98_cape_fit.h src/r99_maze_caps.h

out/r16_fast.o: src/r101_bloodworm_ground.inc
out/playerentity.o: src/r98_cape_fit.h

arm/embedded_server.o: src/r103_save_native_read.h

out/npctype.o: src/npctype.c src/r104_npc_alpha.h
	$(CC) $(CFLAGS) -o $@ src/npctype.c
out/platform/vita.o: src/thirdparty/tsf.h

client.elf: out/npctype.o

out/entry/client.o: src/r105_barrows_progress.h

out/entry/client.o out/gl11.o out/r16_fast.o: src/r106_scenery_speed.h
out/entry/client.o out/world.o out/locentity.o: src/locentity.h
out/locentity.o: src/locentity.c
	$(CC) $(CFLAGS) -o $@ src/locentity.c
client.elf: out/locentity.o

out/r16_fast.o: src/r107_column_caps.h

out/entry/client.o arm/embedded_server.o out/ba_resource.o: src/ba_resource.h

out/entry/client.o: src/r109_map_decode_cache.h

out/r16_fast.o: src/r110_roof_bounds.h

out/r16_fast.o: src/r111_floor_bounds.h

out/r16_fast.o: src/r112_roof_planes.h

out/r16_fast.o: src/r113_tower_support.h
