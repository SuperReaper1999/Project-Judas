# Tilemap Workshop

Editable native 2D tile authoring, compiled to ordinary sprites and MK2 colliders.

WASD/arrows, Space to jump; 1 Trail, 2 Village, R restart; F6/F7 modern save/load. Collect stars and push crates.

In the editor: View > 2D authoring > Tilemap painter. Select a grid, choose a palette tile and enable Paint in Scene view. Left drag draws, right drag erases; rectangle tools fill/erase. Release commits one undoable stroke; Escape cancels.

Paint source scenes; prefabs retain compiled objects. 2,048 cells, 8 layers, full-cell static collision. No animated tiles, slopes or one-way platforms yet. Palette edits require explicit Rebuild from palette. Original art generation: tools/demos/generate_tile_palette.py.
