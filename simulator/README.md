# Tab5 browser simulators

Build and run the dedicated container from the repository root:

```powershell
docker compose -f simulator/compose.yaml up --build
```

Open <http://localhost:8088/> for the default retro wood LVGL preview,
<http://localhost:8088/metal.html> for the dedicated brushed-metal LVGL simulator, or
<http://localhost:8088/cyberdeck.html> for the colorful digital cyberdeck LVGL simulator, or
<http://localhost:8088/m5gfx.html> for the original M5GFX `PerformanceUi::drawInitial()` preview.
Both canvases use the Tab5 landscape resolution of 1280 x 720.

The query-string theme URLs <http://localhost:8088/?theme=metal> and
<http://localhost:8088/?theme=cyberdeck> remain supported.

Stop the preview with:

```powershell
docker compose -f simulator/compose.yaml down
```
