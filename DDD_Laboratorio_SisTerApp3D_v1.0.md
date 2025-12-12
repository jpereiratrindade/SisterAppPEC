SisTerApp3D – Domain-Driven Design (DDD) do Laboratório Ecossistêmico
Versão: 1.0
Compatível com: SisterAppEngine v3.3.0
Bounded Context: Ecosystem Laboratory (Novo)
Propósito: Introduzir uma arquitetura científica para simulação ecológica, dinâmica de comunidades vegetais campestres, paisagens e agroecossistemas dentro do mesmo engine Vulkan/SDL do projeto.

---

# 1. Overview
Este DDD descreve o Laboratório Ecossistêmico SisTerApp3D, um novo bounded context que se integra ao SisterAppEngine v3.3.0 sem alterar o domínio voxel original. Enquanto o branch voxel-first representa terreno em blocos e exploração estilo Minecraft, o Laboratório Ecossistêmico fornece:

- visualização científica,
- modelos ecológicos carregáveis,
- vegetação paramétrica,
- simulações de paisagem,
- importação de dados geoespaciais,
- patch models baseados em H3/DEM,
- dinâmicas multi-estrato (inferior / superior),
- agentes (gado, distúrbios, fauna).

O laboratório funciona como uma nova cena (EcosystemScene) paralela à VoxelScene, utilizando os mesmos recursos do Core e Graphics.

---

# 2. Bounded Context: Ecosystem Laboratory
Responsabilidade principal: fornecer ambiente para simulação ecológica interativa, carregamento de modelos externos, visualização e edição científica.

---

# 3. Arquitetura Geral

EcosystemScene
 ├── TerrainDEM
 ├── VegetationSystem
 ├── PatchModel (H3/Grid)
 ├── SimulationRuntime
 │     ├── GrowthModel
 │     ├── SuccessionModel
 │     ├── DisturbanceModel
 │     ├── CommunityModel
 │     └── ClimateIntegration
 ├── EcologicalModelLoader
 └── EcosystemEditor (UI)

---

# 4. Entidades, Agregados e Serviços

## 4.1 Entidades

### TerrainDEM
Dados de relevo não voxel (DEM/GeoTIFF), incluindo slope, aspect, solo, umidade.

### Patch
Unidade espacial H3/Grid. Contém biomassa, espécies, fatores ambientais e estado sucessional.

### Species
Metadados ecológicos: curvas de crescimento, tolerâncias, estrato, parâmetros de competição.

### PlantInstance
Instância renderizada em GPU: posição, escala, espécie, estrato, estado fisiológico.

## 4.2 Agregado Raiz: EcosystemModel
EcosystemModel
 ├── SpeciesLibrary
 ├── PatchGrid
 ├── VegetationInstances
 ├── EcologicalRuleset
 └── SimulationParameters

## 4.3 Serviços

- DistributionService
- SuccessionService
- GrowthService
- DisturbanceService
- ClimateService

---

# 5. Fluxo de Execução

EcosystemScene::update(dt):
    SimulationRuntime.tick(dt)
    VegetationSystem.updateGPUInstances()
    TerrainDEM.updateLODs()

EcosystemScene::render:
    render terrain (eco_terrain pipeline)
    render vegetation (eco_vegetation pipeline)
    render agents
    render overlays (NDVI, biomass, succession)

---

# 6. Carregamento Externo

Suporta:
- JSON (regras ecológicas, espécies, parâmetros),
- GeoTIFF/DEM,
- Shapefile/GeoJSON,
- CSV (séries temporais),
- H3 datasets,
- GLB/OBJ (vegetação 3D).

Responsável: EcologicalModelLoader.

---

# 7. Integração com GraphicsContext

Reutiliza:
- GraphicsContext,
- Renderer,
- Buffer/InstanceBuffers,
- Camera,
- UI Layer.

Isolado do domínio voxel — sem dependências diretas.

---

# 8. Ecosystem Editor (UI Layer)

Ferramentas:
- Load Model
- Edit Vegetation
- Edit Patches
- Disturbance controls
- Growth curves
- Stratification tools
- Time-lapse playback
- Export snapshots/scenarios

---

# 9. Ubiquitous Language

Patch, Species, Community, Stratum, Disturbance, Biomass,
Adaptability Curves, EcologicalRuleset, Succession, SimulationTick.

---

# 10. Fora do escopo
- Voxels, greedy meshing, blocks.
- Chunk scheduling.
- Water voxel passes.

---

# 11. Extensões futuras
- Modelos hidrológicos.
- Fauna avançada.
- Integração SisTerStat.
- Exportação HDF5.
- Suporte VR.

---

# 12. Conclusão
O Laboratório transforma o SisterAppEngine em um ambiente científico interativo, mantendo o DDD do v3.3.0 intacto e expandindo-o por meio de um bounded context isolado e bem definido.
