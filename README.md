# cold-path-map-editor
Map Editor for Cold Path

How to use: https://book.denismakhortov.com/guides/map-editor

## Command line conversion

The desktop build can convert an old map to the current `.map` format without
opening the editor UI. Pass the `cli.convert` Defold config override on the
command line and start the built editor executable. By default the editor
reads/writes the map in its own folder (next to the executable), so no path is
required if `exported_map/` (or a legacy map) sits there.

```powershell
.\ColdPathMapEditor.exe "--config=cli.convert=1"
```

Optional overrides:

- `cli.data_path`: folder that contains `exported_map/` or a `.map` package. Defaults to the executable's own folder.
- `cli.output_path`: target `.map` path. Defaults to the data folder.

```powershell
.\ColdPathMapEditor.exe "--config=cli.convert=1" "--config=cli.data_path=D:\cold-path-maps\my-map" "--config=cli.output_path=D:\cold-path-maps\my-map.map"
```
