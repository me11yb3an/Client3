# Offline world bundles

These files are esbuild bundles loaded by the QuickJS host in `src/embedded_server.c`. They are not compiled by `units.mk`.

| File | Role |
| --- | --- |
| `server.bundle.npc-unload.js` | Active bundle. SHA-256 `b57d89765daef6e667df52668a47403270fdcf1f7d1bfd154bbad07990df610c` |
| `server.bundle.tickparts.js` | Reference bundle from the same checkpoint |

The world code defaults to `ENGINE_REVISION` 225 and imports `@2004scape/rsbuf` and `@2004scape/rsmod-pathfinder`. That identifies it as a Lost City / 2004Scape engine bundle, not as original code for this Vita port.

The required copyright and permission notices for these bundles are in [NOTICE.md](NOTICE.md). That file covers the Lost City engine, `@2004scape/rsbuf`, `@2004scape/rsmod-pathfinder`, and node-forge. It is not a license for the C client. Details are in `docs/LICENSING.md` and `docs/COMPLIANCE.md`.

`SOURCE-MANIFEST.json` still lists the bundles at the export's original paths. The bytes in this directory are those files and were not edited.
