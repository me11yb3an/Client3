# Translation units added for build 244. units.mk compiles these.
# The host Makefile and vita.mk exclude them so their wildcards stay
# on the original Client3 source list.
BUILD244_UNITS := \
	src/ba_hw_r6.c \
	src/ba_native_memory.c \
	src/ba_resource.c \
	src/ba_server_owner.c \
	src/ba_shader_loader.c \
	src/classic_ui.c \
	src/embedded_server.c \
	src/exit_trace.c \
	src/h8_protocol.c \
	src/h8_remote.c \
	src/layoutcore.c \
	src/layoutcore_draw.c \
	src/offline_vita.c \
	src/r10_camera.c \
	src/r10_scene.c \
	src/r11_diag.c \
	src/r12_waits.c \
	src/r13_capture.c \
	src/r16_fast.c \
	src/r27_cursor.c \
	src/title_theme.c \
	src/ui474.c \
	src/ui474_parchment.c \
	src/vita-telemetry.c \
	src/vita_dialogue.c \
	src/vita_enhanced.c \
	src/vita_title.c
