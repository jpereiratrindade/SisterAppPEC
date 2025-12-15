# SisterApp Engine v3.7.3 - Domain-Driven Design (DDD)

## 1. Overview
A partir da versão v3.5.0, a engine suporta **dois modos de operação distintos**:
1. **Modo Voxel (Infinite)**: Minecraft-style, chunks paginados, greedy meshing. (Clássico v3.4)
2. **Modo Finite World (High-Fidelity)**: Mapa de terreno contíguo (Ex: 2048x2048), renderização smooth, erosão hidráulica e iluminação avançada.

A arquitetura unificada (`GraphicsContext`, `Application`) serve a ambos os modos, mas o pipeline de renderização se bifurca (`VoxelTerrain` vs `TerrainRenderer`).
Este documento formaliza boas práticas para ambos os contextos.

---

## 2. Bounded Contexts

### 2.1 Core
Responsabilidade: ciclo de vida da aplicação, janela/input SDL, criação e destruição do device Vulkan, swapchain, sincronização de frames e orquestração do loop principal.

Entidades:
- Application (Aggregate Root)
- GraphicsContext (Single Source of Truth)
- VkUtils

---

### 2.2 Graphics
Responsabilidade: simulação, streaming e renderização do mundo voxel.

Entidades:
- VoxelTerrain
- Chunk
- VoxelScene
- Renderer
- Mesh / Buffer
- **Watershed Algorithm (Service)**: Segmentação e Delineação de Bacias (v3.6.3).
- **HydrologyReport (Service)**: Cálculo de estatísticas eco-hidrológicas.

---

### 2.3 UI
Responsabilidade: interface via Dear ImGui.

Funcionalidades:
- Stats Overlay
- Tools Menu
- Janelas auxiliares

---

## 3. Domain Model & Ubiquitous Language

### 3.1 Domain Logic: Scientific Models
O coração da simulação reside na interação de três variáveis de resiliência:
- **Resiliência Ecológica ($R_{ecol}$)**: Governa a estabilidade topológica (suavidade) e biodiversidade.
- **Resiliência Produtiva ($R_{prod}$)**: Governa a densidade de biomassa (árvores) e fertilidade do solo.
- **Resiliência Social ($R_{soc}$)**: Governa a conectividade humana (estradas, corredores).

Estas variáveis influenciam diretamente a geração procedural (`generateChunk`) e a renderização (`rebuildChunkMesh`).

### 3.2 Termos Ubíquos

- Application: raiz de composição.
- Chunk Scheduling: priorização por frustum.
- Vegetation Lifecycle: versionamento global.
- Pruning: descarregamento assíncrono.
- Visual Feedback: ausência de vegetação muda cor do terreno.

---

## 4. Interaction Flow (Voxel Frame)

1. Input
2. Update
3. Render
4. Present

---

## 5. Architectural Rules & Best Practices

### 5.1 GPU Resource Lifecycle

GP-01 Recursos GPU nunca são destruídos sem sincronização explícita.
GP-02 vkDeviceWaitIdle é proibido no caminho quente.
GP-03 Destruição assíncrona baseada em fences é obrigatória.

---

### 5.2 Render Loop & Performance

RL-01 Nenhuma operação bloqueante no render loop.
RL-02 Coleta de lixo GPU fora do caminho crítico.

---

### 5.3 Streaming, Chunks e Vegetação

ST-01 Atualizações estruturais nunca bloqueiam o frame.
ST-02 Orçamento explícito por frame.
ST-03 Mutex global é antipadrão.

---

### 5.4 Multithreading

MT-01 Reset exige mundo quiescente.
MT-02 Workers não publicam estado renderizável.

---

### 5.5 Memória e Uploads

MM-01 Mesh final em DEVICE_LOCAL.
MM-02 Uploads usam pipeline dedicado.

### 5.6 Future: Semantic Data Architecture (v4.0 Planned)
**Princípio**: "A GPU não decide o solo. Ela apenas pinta."
- **CPU Authority**: A classificação de solo (id) deve ser calculada deterministicamente na CPU.
- **Data Driven**: Enviar IDs via atributos de vértice ou buffer SSBO.
- **Lookup GPU**: Shader faz apenas `lookupColor(id)`.
- **Zero Sync**: O probe (CPU) apenas lê o estado local, garantindo consistência perfeita sem round-trip à GPU.

---

## 6. Observabilidade e Validação

OB-01 Tempo é métrica de primeira classe.
OB-02 Stress test obrigatório.

---

## 7. Antipadrões Proibidos

- vkDeviceWaitIdle em update/render
- Destruição prematura de recursos
- Loops pesados no thread principal
- Reset sem drenar workers
- Mutex global como padrão

---

## 8. Regra de Ouro

Se o GPU ainda pode tocar, o recurso ainda vive.
Se o frame não tem orçamento, ele não é confiável.

---

## 9. Out of Scope / Legacy

Pipeline CSV/PCoA fora do escopo do branch voxel-first.