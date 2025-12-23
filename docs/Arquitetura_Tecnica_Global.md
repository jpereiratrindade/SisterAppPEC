# Arquitetura Técnica Global: SisterAppPEC v4.5+

**Data:** 23 de Dezembro de 2025
**Status:** Consolidado (v4.5.x)

## 1. Visão Geral do Sistema

O **SisterAppPEC** é uma plataforma de simulação eco-hidrológica de alta fidelidade ("Finite World System"). Diferente de engines de jogos tradicionais, seu foco não é "gameplay", mas a **precisão científica** da evolução da paisagem.

### 1.1 Diagrama de Blocos Macro
```mermaid
graph TD
    User([Usuário]) --> UI[Camada UI (ImGui)]
    UI --> App[Application (Core)]
    
    subgraph "Simulation Engine (CPU)"
        App --> World[World Container]
        World --> Terrain[Terrain System (Geometria)]
        World --> Land[Landscape System (Ciência)]
        World --> Veg[Vegetation System (Bio)]
        
        Land -->|Erosão/Fluxo| Terrain
        Land -->|Água/Nutrientes| Veg
        Veg -->|Biomassa/Proteção| Land
    end
    
    subgraph "Render Engine (GPU)"
        App --> Renderer[Terrain Renderer]
        Terrain -->|Mesh Data| Renderer
        Land -->|Color Map (Soils)| Renderer
        Veg -->|Instance Data| Renderer
    end
```

---

## 2. Estrutura de Diretórios e Namespaces

O projeto segue uma organização modular onde a estrutura de pastas reflete os *Bounded Contexts* do DDD.

| Diretório | Namespace | Responsabilidade | Principais Classes |
| :--- | :--- | :--- | :--- |
| `src/core` | `core` | **Infraestrutura**: Janela, Input, Loop principal. | `Application`, `Window`, `Input` |
| `src/terrain` | `terrain` | **Geometria**: O "chão" físico. Altura e Malha. | `TerrainMap`, `TerrainGenerator` |
| `src/landscape` | `landscape` | **Ciência Abiótica**: Solos, Hidrologia, Geologia. | `SoilSystem`, `HydroSystem`, `LithologyRegistry` |
| `src/vegetation`| `vegetation`| **Ciência Biótica**: Dinâmica de plantas. | `VegetationSystem` |
| `src/graphics` | `graphics` | **Visualização**: Tradução de dados para Vulkan/OpenGL. | `TerrainRenderer`, `Camera` |
| `src/ui` | `ui` | **Interação**: Controles e Ferramentas. | `UiLayer` |

---

## 3. Subsistemas Detalhados

### 3.1 Core & Terrain (A Base)
O `TerrainMap` é a "Verdade Geométrica". Ele armazena o *Heightmap* (mapa de alturas).
*   **Fluxo de Regeneração:** Quando o usuário muda o "Seed", o `TerrainGenerator` roda em uma **Thread Separada** (para não travar a UI) e recria o Heightmap usando ruído Perlin/Simplex.

### 3.2 Landscape (O Coração Científico)
Local: `src/landscape`
Este módulo implementa a física do ambiente.

#### A. Sistema de Solos (SCORPAN)
Implementa a arquitetura vetorial onde o solo emerge de fatores de formação.
*   **Input:** Geologia (`LithologyRegistry`), Relevo (`TerrainMap`), Clima.
*   **Processo:** `SoilSystem::update` chama `PedogenesisService`.
*   **Output:** `SoilGrid` (Profundidade, Textura, Carbono).
*   **Classificação:** O `SiBCSClassifier` lê o grid e determina se é Latossolo, Argissolo, etc.

#### B. Sistema Hidrológico
*   **Input:** Chuva (Clima) + Topografia.
*   **Processo:** `HydroSystem` calcula fluxo D8 (quem drena para quem).
*   **Output:** `HydroGrid` (Profundidade da Água, Risco de Erosão).

### 3.3 Vegetation (A Resposta Biológica)
Local: `src/vegetation`
*   **Modelo:** Estratos Funcionais (Grama vs Arbusto).
*   **Conexão:** Lê a água do `HydroSystem` e a fertilidade do `SoilSystem`.
*   **Feedback:** Retorna "Biomassa Morta" para o solo (aumenta Carbono) e "Proteção Física" (reduz Erosão).

---

## 4. Fluxo de Dados (Data Pipeline)

A simulação roda em passos discretos de tempo (`dt`). A ordem de atualização é crítica para a causalidade:

1.  **Clima & Tempo**: Define quanto choveu.
2.  **Hidrologia (`HydroSystem`)**: A chuva cai e escorre.
3.  **Solos (`SoilSystem`)**: A água infiltra ou erode. Intemperismo acontece.
4.  **Vegetação (`VegetationSystem`)**: As plantas bebem a água e crescem.
5.  **Renderização**: O estado final é desenhado na tela.

---

## 5. Domain-Driven Design (DDD) - Resumo da Estratégia

O projeto utiliza DDD para combater a complexidade.

*   **Services (Serviços):** Onde mora a matemática e as regras (ex: `PedogenesisService`). São "stateless".
*   **Entities/Grids (Estado):** Onde moram os dados. Usamos **Structure-of-Arrays (SoA)** (vetores `std::vector<float>` em vez de objetos) para performance máxima em cache de CPU.
*   **Systems (Orquestradores):** Classes que conectam os Dados aos Serviços (ex: `SoilSystem`).

> **Regra de Ouro:** A "Física" (Services) nunca depende da "Renderização" (Graphics). O sistema funcionaria mesmo sem placa de vídeo (headless).

## 6. Documentação Específica (Deep Dives)

Para detalhes matemáticos e biológicos de cada domínio, consulte os DDDs específicos (que devem ser mantidos atualizados):

1.  **Solos:** `docs/DDD_Sistema_Solo.md` (SCORPAN & SiBCS)
2.  **Vegetação:** `docs/DDD_Vegetacao_Campestre.md` (Em atualização para v4.5)
3.  **Hidrologia:** (Integrado na Global ou doc futuro)
