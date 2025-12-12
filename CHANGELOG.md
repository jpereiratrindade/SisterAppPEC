# SisterApp Engine - Changelog (Updated)

## [3.3.0] - 2025-12-12

### Highlights
- **Critical Stability Fix**: Resolved GPU crash/reset when toggling vegetation with VSync disabled.
- **Safe Resource Destruction**: Implemented fence-aware garbage collection. Resources (meshes) are now queued for destruction and only released when the associated frame fence is signaled, preventing "Use-After-Free" GPU errors.
- **Robustness**: Validated stability under high-stress conditions (unlimited FPS, rapid asset reloading).

#### Files touched
- `voxel_terrain.{h,cpp}`: Deferred destruction queue, fence propagation, non-FIFO garbage collection.
- `voxel_scene.{h,cpp}`: Propagated frame index to terrain.
- `application.cpp`: Pass frame index to scene update.

---

## [3.2.2.5-beta] - 2026-01-xx

### Highlights
- Seleção de **modelo de terreno** em Tools → Performance (Plano c/ ondulações leves, Suave ondulado, Ondulado).
- Perfil aplicado a spawn/teleport e regen de vegetação.
- Pool de workers para geração/meshing com limites de fila; modo seguro de vegetação (regen em lote) para evitar quedas ao alterar densidade/modelo.
- Controles de performance: VSync toggle e **cap manual de FPS** no menu Performance.

#### Files touched
- `voxel_terrain.{h,cpp}`: perfis, pool de workers, fila limitada, modo seguro de vegetação, throttling de regeneração
- `application.{h,cpp}`: cap manual de FPS, VSync toggle, título/log v3.2.2.5-beta
- `ui/ui_layer.{h,cpp}`: controles de VSync e FPS cap
- `README.md`, `CHANGELOG.md`, `DDD.md`: versão e notas 3.2.2.5-beta

---

## [3.2.2.3-beta] - 2026-01-xx

### Highlights
- Classificação de terreno (plano / inclinado / montanha) baseada em gradiente local
- Vegetação reagente ao terreno: grama mais viva em planícies, desaturada em encostas, rala e fria em montanhas
- Árvores quase ausentes em planícies, espaçadas em encostas, desabilitadas em montanhas
- Mantida performance: classificação é leve (reaproveita altura via ruído), budgets configuráveis seguem válidos
- Vegetation Density propaga para chunks novos e já carregados (versão por chunk); probe mostra slope, moisture e classe de vegetação real
- Horizonte estendido (fog 150–450) e grid removido do voxel shader

#### Files touched
- `voxel_terrain.{h,cpp}`: classificação, cores por classe, densidade de árvores, versão de vegetação por chunk, probing enriquecido
- `shaders/voxel.frag`: fog mais distante, grid removido
- `README.md`: versão e novidades 3.2.2.3-beta, controles/performance

---

## [3.2.2.2-beta] - 2026-01-xx

### Highlights
- Spawn visível: warmup síncrono de chunks no início + câmera posicionada na altura do terreno em (0,0)
- Navegação: roll/tilt da câmera (Z/X), ajuste rápido de FOV (`[` `]`), FOV default mais longo (far=500)
- Performance: sliders no menu Performance para view distance, chunks/frame, meshes/frame; VSync toggle mantém controle de FPS
- Horizonte: fog alongado (80–240) e view distance padrão 10 (configurável 4–16)
- Manutenção: descarte de chunks distantes para reduzir memória e draw calls

#### Files touched
- `README.md`: versão, controles, perf sliders, spawn visível
- `CHANGELOG.md`: entrada 3.2.2.2-beta
- `application.cpp`: spawn acima do terreno, far plane estendido, perf sliders
- `voxel_terrain.{h,cpp}`: warmup inicial, budgets configuráveis, pruning de chunks, geração síncrona opcional
- `camera.{h,cpp}`: roll, FOV ajustável
- `shaders/voxel.frag`: fog estendido

---

## [3.2.2] - 2025-12-09

### 🗺️ Navigation Enhancements

#### Added
- **Bookmarks System**:
  - Save favorite camera positions with custom names
  - F5: Quick save bookmark
  - F6-F8: Load bookmarks from slots 1-3
  - UI window with list, save, load, delete functions
  - Menu: Tools → Bookmarks
  
#### Files Modified
- `application.h`: Added `Bookmark` struct, bookmark vector, management methods
- `application.cpp`: Implemented save/load/delete, F5-F8 shortcuts, UI window

---

### 📊 Visual Analysis

#### Added
- **Cluster Filters**:
  - Show/hide individual clusters via checkboxes
  - Opacity sliders per cluster (0.0-1.0)
  - Integrated in Analysis Tools window
  - Real-time visual filtering

#### Files Modified  
- `application.h`: Added `hiddenClusters_` set, `clusterOpacity_` array
- `application.cpp`: Filter UI in Analysis Tools, cluster visibility logic

---

### 📈 Stats Overlay

#### Added
- **Real-time Statistics Display**:
  - FPS counter
  - Point count
  - Cluster count (after K-Means)
  - Current camera mode
  - Camera position (x, y, z)
  - Semi-transparent overlay (bottom-left)

#### Fixed
- **Cluster Labels Bug**: K-Means wasn't populating `vizState_.clusterLabels`
  - Added `hasKMeans` flag to `pendingUpdate_`
  - Added `clusterLabels` vector transfer
  - Stats Overlay now correctly shows cluster count

#### Files Modified
- `application.h`: Added `showStatsOverlay_`, `lastFrameTime_`
- `application.cpp`: Stats overlay window, cluster labels fix in K-Means

---

### 🎨 Theme System

#### Added
- **Dark/Light Theme Toggle**:
  - Menu: Tools → Toggle Theme (Ctrl+T)
  - Dark mode (default): Dark blue theme
  - Light mode: Light gray/white theme
  - Console logging on theme change

#### Files Modified
- `application.h`: Added `Theme` enum, `currentTheme_`, `applyTheme()` method
- `application.cpp`: Theme toggle in menu, `applyTheme()` implementation

---

### 📁 New Includes
- `<set>` in `application.h` for cluster tracking

---

## [3.2.1] - 2025-12-09 (Previous Release)

### Features
- Dual Camera System (Orbital + Free Flight)
- Expanded 3D Environment (50x50 grid, sky dome, markers)
- Animation System (auto-rotation, interpolation)
- Enhanced Shaders (dual lighting, fog)
- Gameplay Improvements (teleport, floor collision)
- Data Navigation Integration (cluster navigation)

---

## Version Comparison

| Feature | v3.2.1 | v3.2.2 |
|---------|--------|--------|
| Camera Modes | 2 | 2 |
| Saved Positions | 0 | **Unlimited (Bookmarks)** |
| Cluster Control | Basic nav | **Full (hide+opacity)** |
| Stats Display | No | **Yes (real-time)** |
| Themes | Dark only | **Dark + Light** |
| Screenshots | No | No (deferred) |
| Nav History | No | No (deferred) |

---

## Credits

**Development**: José Pedro Trindade  
**Version**: 3.2.2.5-beta  
**Release Date**: December 9, 2025  
**Status**: Stable ✅
