CC := /usr/local/vitasdk/bin/arm-vita-eabi-gcc
CFLAGS := -c -O2 -ffast-math -g -fwrapv -std=c99 -DGL11 -DWITH_RSA_LIBTOM -Dclient -DBUILD_A_EMBEDDED -DVITA_NATIVE_WIDESCREEN -DBA_R9_UPLOAD_MODE=0 -DBA_R10_CACHE=0 -DBA_R10_COMPACT=0 -DBA_R11_DIAG=1 -DBA_R11_SHARED_STATIC=0 -DBA_R12_DIAG=1 -DBA_R12_PROBE=0 -DBA_R12_REBASE=0 -I/tmp/rs225-codex-r15/bridge-poc -I/tmp/rs225-codex-bridge -I/tmp/rs225-content244-root/src-tree -I/tmp/rs225-content244-root/save
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

out/ba_hw_r6.o: src-tree/ba_hw_r6.c
	$(CC) $(CFLAGS) -o $@ src-tree/ba_hw_r6.c

out/ba_resource.o: src-tree/ba_resource.c
	$(CC) $(CFLAGS) -o $@ src-tree/ba_resource.c

out/ba_shader_loader.o: src-tree/ba_shader_loader.c
	$(CC) $(CFLAGS) -o $@ src-tree/ba_shader_loader.c

out/clientstream.o: src-tree/clientstream.c
	$(CC) $(CFLAGS) -o $@ src-tree/clientstream.c

out/entry/client.o: src-tree/r19_popup_state.inc src-tree/entry/client.c src-tree/vita_enhanced.h src-tree/ui474.h src-tree/r17_menu_overlay.inc
	$(CC) $(CFLAGS) -o $@ src-tree/entry/client.c
out/entry/client.o: src-tree/r45_path_recovery.h

out/gameshell.o: src-tree/gameshell.c src-tree/ba_pacing.inc src-tree/ba_cadence_queue.inc
	$(CC) $(CFLAGS) -o $@ src-tree/gameshell.c
out/gameshell.o: src-tree/r44_scene_loading_clock.h

out/gl11.o: src-tree/r40_water_gate.h src-tree/gl11.c src-tree/vita_enhanced.h
	$(CC) $(CFLAGS) -o $@ src-tree/gl11.c

out/h8_remote.o: src-tree/h8_remote.c
	$(CC) $(CFLAGS) -o $@ src-tree/h8_remote.c

out/pixmap.o: src-tree/r19_tiled_ui.inc src-tree/r19_region_diff.inc src-tree/pixmap.c src-tree/pixmap.h
	$(CC) $(CFLAGS) -o $@ src-tree/pixmap.c

out/platform/vita.o: src-tree/platform/vita.c src-tree/vita_button_edges.h src-tree/vita_title_layout.h
	$(CC) $(CFLAGS) -o $@ src-tree/platform/vita.c

out/r10_camera.o: src-tree/r10_camera.c
	$(CC) $(CFLAGS) -o $@ src-tree/r10_camera.c

out/r11_diag.o: src-tree/r11_diag.c
	$(CC) $(CFLAGS) -o $@ src-tree/r11_diag.c

out/r12_waits.o: src-tree/r12_waits.c
	$(CC) $(CFLAGS) -o $@ src-tree/r12_waits.c

out/r13_capture.o: src-tree/r13_capture.c src-tree/r13_capture.h
	$(CC) $(CFLAGS) -o $@ src-tree/r13_capture.c

out/r16_fast.o: src-tree/r20_paving.h src-tree/r35_compact_patterns.inc src-tree/r19_live_tile_index.inc src-tree/r16_fast.c src-tree/r18_actor_contact.inc src-tree/r18_player_ground.inc src-tree/r18_model_water.inc src-tree/r16_furniture_layers.inc src-tree/r16_live_doors.inc src-tree/r16_roof_cache.inc src-tree/r16_coplanar.h
	$(CC) $(CFLAGS) -o $@ src-tree/r16_fast.c

out/title_theme.o: src-tree/title_theme.c src-tree/vita_title.h
	$(CC) $(CFLAGS) -o $@ src-tree/title_theme.c

out/ui474.o: src-tree/ui474.c src-tree/ui474.h
	$(CC) $(CFLAGS) -o $@ src-tree/ui474.c

out/vita_enhanced.o: src-tree/r19_chat_background.inc src-tree/vita_enhanced.c src-tree/vita_enhanced.h src-tree/vita_map_aperture.generated.h
	$(CC) $(CFLAGS) -o $@ src-tree/vita_enhanced.c

out/world3d.o: src-tree/world3d.c src-tree/r34_normal_index.h
	$(CC) $(CFLAGS) -o $@ src-tree/world3d.c

out/loctype.o: src-tree/loctype.c
	$(CC) $(CFLAGS) -o $@ src-tree/loctype.c

out/world.o: src-tree/world.c src-tree/r25_wall_decor.h
	$(CC) $(CFLAGS) -o $@ src-tree/world.c

out/offline_vita.o: src/offline_vita.c src/ba_hw_r2.inc src/ba_hw_r3.inc src/ba_hw_r4.inc src/ba_hw_r5.inc src/ba_hw_r7.inc
	$(CC) $(CFLAGS) -o $@ src/offline_vita.c

# Server and save objects are not part of `all`. `make client.elf` rebuilds
# any of them that are older than their sources, then runs link.sh. The JS
# bundle is loaded from the package at runtime and is not an input of this link.
STRIP := /usr/local/vitasdk/bin/arm-vita-eabi-strip
ELF_CREATE := /usr/local/vitasdk/bin/vita-elf-create
FSELF := /usr/local/vitasdk/bin/vita-make-fself
SERVER_CFLAGS := -O2 -g -std=c11 -ffunction-sections -fdata-sections -I/tmp/rs225-content244-root/save

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

out/vita_title.o: src-tree/vita_title.c src-tree/vita_title_layout.h src-tree/vita_title.h src-tree/r20_title_cache.inc
	$(CC) $(CFLAGS) -o $@ src-tree/vita_title.c


out/custom.o: src-tree/custom.c src-tree/custom.h
	$(CC) $(CFLAGS) -o $@ src-tree/custom.c

out/r16_fast.o: src-tree/r20_paving.h src-tree/gl11.h
out/gl11.o: src-tree/gl11.h

out/r13_capture.o: src-tree/r21_capture_codec.h src-tree/r13_capture.h
out/loctype.o: src-tree/r21_location_visual.h src-tree/loctype.h
out/world.o: src-tree/loctype.h
out/entry/client.o: src-tree/loctype.h src-tree/r13_capture.h

out/entry/client.o out/ui474.o: src-tree/r23_tutorial.h

out/r16_fast.o: src-tree/r24_door_states.h

out/clientstream.o: src-tree/offline_stream.inc

out/r16_fast.o: src-tree/r25_wall_decor.h

out/r16_fast.o: src-tree/r26_wall_decals.inc
out/r16_fast.o: src-tree/r42_planar_index.h

out/r27_cursor.o: src-tree/r27_cursor.c src-tree/r27_cursor.h src-tree/r27_cursor_images.h
	$(CC) $(CFLAGS) -o $@ src-tree/r27_cursor.c
out/ui474.o out/platform/vita.o: src-tree/r27_cursor.h

out/r16_fast.o: src-tree/r28_window_planes.inc src-tree/r29_live_furnace.inc src-tree/r31_static_compact.inc

out/r16_fast.o: src-tree/r32_roof_runtime.inc src-tree/r32_roof_zones.h src-tree/r32_index_masks.inc

out/r16_fast.o: src-tree/r46_floor_partition.h src-tree/r47_floor_contacts.inc src-tree/r48_source_ids.h src-tree/r49_floor_supports.h src-tree/r50_wall_fit.h src-tree/r50_wall_furniture.inc src-tree/r50_furniture_types.h

out/r16_fast.o: src-tree/r51_openings.inc src-tree/r51_opening_clip.h src-tree/r51_opening_types.h

out/r16_fast.o: src-tree/r52_underground.inc

out/r16_fast.o: src-tree/r53_ship_priority.inc src-tree/r53_ship_priority.h

out/r16_fast.o: src-tree/r54_crates.h src-tree/r54_crane_duplicates.inc

out/r16_fast.o: src-tree/r55_fixture_contacts.inc

out/r16_fast.o: src-tree/r56_hull_layers.inc src-tree/r56_hull_types.h

# Actor material candidates change Model layout and actor construction.
$(filter %.o,$(wildcard out/*.o out/entry/*.o out/platform/*.o)): src-tree/model.h
out/model.o: src-tree/r77_body_selection_cache.inc src-tree/r75_hover_bounds.h src-tree/r57_body_layers.inc src-tree/r57_body_layers.h src-tree/r16_coplanar.h
out/npctype.o out/playerentity.o out/r16_fast.o: src-tree/r57_body_layers.h

out/r16_fast.o: src-tree/r57_body_render.inc

R57_MODEL_OBJECTS := out/allocator.o out/animbase.o out/animframe.o out/ba_hw_r6.o out/ba_native_memory.o out/ba_resource.o out/ba_server_owner.o out/ba_shader_loader.o out/classic_ui.o out/clientstream.o out/collisionmap.o out/component.o out/custom.o out/entity.o out/flotype.o out/gameshell.o out/gl11.o out/ground.o out/h8_protocol.o out/h8_remote.o out/idktype.o out/inputtracking.o out/jagfile.o out/layoutcore.o out/layoutcore_draw.o out/locentity.o out/locmergeentity.o out/loctype.o out/model.o out/npcentity.o out/npctype.o out/objstackentity.o out/objtype.o out/offline_vita.o out/packet.o out/pathingentity.o out/pix24.o out/pix2d.o out/pix3d.o out/pix8.o out/pixfont.o out/pixmap.o out/platform.o out/playerentity.o out/projectileentity.o out/r10_camera.o out/r10_scene.o out/r11_diag.o out/r12_waits.o out/r13_capture.o out/r16_fast.o out/r27_cursor.o out/seqtype.o out/spotanimentity.o out/spotanimtype.o out/tileoverlay.o out/tileunderlay.o out/title_theme.o out/ui474.o out/ui474_parchment.o out/varptype.o out/vita-telemetry.o out/vita_dialogue.o out/vita_enhanced.o out/vita_title.o out/world.o out/world3d.o out/datastruct/doublylinkable.o out/datastruct/doublylinklist.o out/datastruct/hashtable.o out/datastruct/jstring.o out/datastruct/linkable.o out/datastruct/linklist.o out/datastruct/lrucache.o out/entry/client.o out/entry/midi.o out/entry/playground.o out/platform/3ds.o out/platform/dreamcast.o out/platform/dummy.o out/platform/emscripten.o out/platform/nds.o out/platform/psp.o out/platform/sdl1.o out/platform/sdl2.o out/platform/sdl3.o out/platform/vita.o out/platform/webassembly.o out/platform/wii.o out/platform/xbox.o out/sound/envelope.o out/sound/tone.o out/sound/wave.o out/thirdparty/bn.o out/thirdparty/bzip.o out/thirdparty/ini.o out/thirdparty/isaac.o out/thirdparty/mpi.o out/thirdparty/rsa-bigint.o out/thirdparty/rsa-libtom.o out/thirdparty/rsa-openssl.o out/thirdparty/rsa-tiny.o out/wordenc/wordfilter.o out/wordenc/wordpack.o
$(R57_MODEL_OBJECTS): src-tree/model.h
client.elf: $(R57_MODEL_OBJECTS)
out/%.o: src-tree/%.c
	$(CC) $(CFLAGS) -o $@ $<

out/r16_fast.o: src-tree/r58_dyn_index.inc
out/platform/vita.o: src-tree/r60_latest_analog.h

out/r16_fast.o: src-tree/r62_shared_crops.inc src-tree/r62_crop_types.h

out/platform/vita.o: src-tree/r63_keyboard_layout.h src-tree/r63_search_icon.h

arm/embedded_server.o: src/r64_banker_defaults.h

out/r16_fast.o: src-tree/r65_bank_fixtures.h

out/r16_fast.o: src-tree/r65_bank_shading.inc

out/loctype.o out/world.o: src-tree/loctype.h src-tree/r66_scenery_visuals.h
out/loctype.o out/r16_fast.o: src-tree/r57_body_layers.h src-tree/r66_scenery_visuals.h
out/r16_fast.o: src-tree/r69_roof_layers.inc
out/r16_fast.o: src-tree/r70_plant_contact.inc
out/r16_fast.o: src-tree/r71_shore_types.h src-tree/r71_shore_plants.inc
out/r16_fast.o: src-tree/r73_animated_view.h

out/gl11.o out/r16_fast.o: src-tree/r79_actor_indices.inc src-tree/gl11.h

out/model.o out/gl11.o: src-tree/r80_exact_words.h

out/r16_fast.o: src-tree/r78_fur_types.h

out/model.o out/r16_fast.o: src-tree/r82_project_divide.h

out/r16_fast.o: src-tree/r83_project_bounds.h

out/pixmap.o: src-tree/r80_exact_words.h

out/model.o out/loctype.o out/r16_fast.o: src-tree/r87_vendor_layers.h src-tree/r57_body_layers.h

out/platform/vita.o: src-tree/r89_stone_cycle.h src-tree/vita_button_edges.h

out/loctype.o out/r16_fast.o: src-tree/r90_sewing_table.h

out/entry/client.o: src-tree/r91_popup_dismiss.inc src-tree/r92_examine_speech.inc

out/gl11.o: src-tree/r93_lava_scroll.h
out/entry/client.o out/sound/wave.o: src-tree/sound/wave.h

out/entry/client.o: src-tree/r94_audio_cost.h
out/r16_fast.o: src-tree/r100_crypt_fixtures.h
out/loctype.o: src-tree/r98_cape_fit.h src-tree/r99_maze_caps.h

out/r16_fast.o: src-tree/r101_bloodworm_ground.inc
out/playerentity.o: src-tree/r98_cape_fit.h

arm/embedded_server.o: src/r103_save_native_read.h

out/npctype.o: src-tree/npctype.c src-tree/r104_npc_alpha.h
	$(CC) $(CFLAGS) -o $@ src-tree/npctype.c
out/platform/vita.o: src-tree/thirdparty/tsf.h

client.elf: out/npctype.o

out/entry/client.o: src-tree/r105_barrows_progress.h

out/entry/client.o out/gl11.o out/r16_fast.o: src-tree/r106_scenery_speed.h
out/entry/client.o out/world.o out/locentity.o: src-tree/locentity.h
out/locentity.o: src-tree/locentity.c
	$(CC) $(CFLAGS) -o $@ src-tree/locentity.c
client.elf: out/locentity.o

out/r16_fast.o: src-tree/r107_column_caps.h

out/entry/client.o arm/embedded_server.o out/ba_resource.o: src-tree/ba_resource.h

out/entry/client.o: src-tree/r109_map_decode_cache.h

out/r16_fast.o: src-tree/r110_roof_bounds.h

out/r16_fast.o: src-tree/r111_floor_bounds.h

out/r16_fast.o: src-tree/r112_roof_planes.h

out/r16_fast.o: src-tree/r113_tower_support.h
