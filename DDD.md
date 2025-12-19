# SisterApp Platform v3.8.4 - Domain-Driven Design (DDD)

## 1. Overview
A versão 3.8.4 marca a transição definitiva para o modelo **Finite World System (High-Fidelity)**. O suporte a Voxel/Minecraft-style foi removido para focar em precisão científica.

A arquitetura agora é simplificada em torno do `TerrainMap` (Heightmap Grid) e `TerrainRenderer`.

---

## 2. Bounded Contexts

### 2.1 Core
Responsabilidade: ciclo de vida da aplicação, janela/input SDL, criação e destruição do device Vulkan, swapchain e loop principal.

Entidades:
- **Application** (Aggregate Root): Gerencia o estado global e a transição de cenas.
- **GraphicsContext** (Single Source of Truth): Encapsula Device, Queue e Swapchain.

---

### 2.2 Terrain (Scientific Domain)
Responsabilidade: simulação hidrológica, análise de declividade e gestão da malha de terreno.

Entidades:
- **TerrainMap**: Entidade central. Contém os dados de altura (Heightmap) e metadados (Erosion, Water).
- **TerrainGenerator**: Serviço de domínio para geração procedural (Perlin/Simplex), Drenagem D8 e Erosão.
- **Watershed (Service)**: Algoritmo de segmentação de bacias hidrográficas.
- **Watershed (Service)**: Algoritmo de segmentação de bacias hidrográficas.
- **LandscapeMetricCalculator (Service)**: Cálculo de métricas de ecologia da paisagem (LSI, CF, RCC).

Documentação de Referência:
- [Padrões de Solo](DDD_Padroes_Manchas_Solo.md)

---

### 2.3 Graphics (Rendering)
Responsabilidade: visualização do terreno e objetos.

Entidades:
- **TerrainRenderer**: Gerencia Vertex Buffers, Index Buffers e Pipelines de renderização para o terreno finito.
- **Camera**: Abstração de View/Projection (Free Flight / Orbital).
- **Mesh**: Wrapper para recursos Vulkan (Buffer, Memory).

- **Mesh**: Wrapper para recursos Vulkan (Buffer, Memory).

---

### 2.4 Vegetation (Ecological Domain)
Responsabilidade: simulação de dinâmica de populações vegetais e distúrbios.

**Contexto e Documentação Exclusiva:**
Devido à alta complexidade biológica, o modelo de vegetação é regido por um DDD próprio e separado:
- [Modelo Ecológico (Regras e Invariantes)](DDD_Vegetacao_Campestre_Pastoril.md)
- [Representação Visual (Tradução Ontológica)](DDD_Representacao_Vegetacao_Campestre.md)

Entidades:
- **VegetationSystem**: Aggregate Root. Gerencia crescimento (Growth), competição e mortalidade.
- **VegetationGrid**: Estrutura de dados (SoA) otimizada para cache contendo biomassa de EI/ES e Vigor.
- **DisturbanceRegime**: Value Object que define Fogo e Pastejo.

### 2.4 UI (Presentation)
Responsabilidade: interface interativa via Dear ImGui.

Funcionalidades:
- **UiLayer**: Camada de orquestração da GUI.
- **Map Generator**: Controle de parâmetros de geração.
- **Minimap**: Navegação Top-down.
- **Hydrology Report**: Exportação de dados.

---

## 3. Domain Model & Ubiquitous Language

### 3.1 Domain Logic: Scientific Models
O foco shiftou de "Resiliência Abstrata" para **Modelagem Física**:
- **Drenagem D8**: Fluxo determinístico de água.
- **Slope Analysis**: Classificação baseada em engenharia (0-3%, 3-8%, etc).
- **Soil Patches**: Distribuição de solos baseada em declividade e ruído espacial.

### 3.2 Termos Ubíquos
- **Finite Map**: O mundo é um quadrado de tamanho fixo (ex: 4096x4096).
- **Regeneration**: Processo assíncrono de recriar o mapa.
- **Basin**: Uma bacia hidrográfica delimitada.
- **Sink**: Ponto de acumulação de água (mínimo local).

---

## 4. Interaction Flow
1. **Input**: SDL Events -> Camera / UI.
2. **Update**:
   - `Application::update()`
   - Se `regenRequested`: Thread worker processa `TerrainGenerator`.
3. **Render**:
   - `Application::render()`
   - `TerrainRenderer::draw()`
   - `UiLayer::render()`
4. **Present**: `vkQueuePresentKHR`.

---

## 5. Architectural Rules & Best Practices

### 5.1 GPU Resource Lifecycle
GP-01 Recursos GPU nunca são destruídos sem sincronização explícita (`vkDeviceWaitIdle`).
GP-02 Destruição assíncrona baseada em fences para recursos de frame (staging buffers).

### 5.2 Threading
MT-01 **Async Regeneration**: Geração pesada de terreno roda em `std::thread` separado.
MT-02 **UI Responsiveness**: O thread principal nunca deve bloquear por >16ms.

### 5.3 Rendering
RN-01 **Deferred Updates**: O upload de malha para a GPU ocorre apenas no início do frame, garantindo que o worker thread já finalizou.

---

## 6. Future Roadmap
- **Compute Shaders**: Mover simulação de erosão para GPU. 
