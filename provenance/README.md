# Original client archive

`src-tree.rar` is the packed client directory from the build 244 export. It was copied here unchanged. On this fork the extracted client lives under `src/`. Hashes for the export are in `SOURCE-MANIFEST.json`, which still uses the original `src-tree/` paths. See `docs/PROVENANCE.md`.

`checkpoint-units.mk` and `checkpoint-link.sh` are the unmodified build scripts. `src-tree-offline_vita.c` is the export copy that `units.mk` does not compile. The compiled file is `src/offline_vita.c`.
