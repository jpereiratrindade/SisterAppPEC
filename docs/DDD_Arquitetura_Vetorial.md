# Domain-Driven Design: Arquitetura Vetorial SCORPAN (v4.4.0)

**Bounded Context:** `EcofunctionalLandscape`
**Subdomínio:** Core Domain (Simulação Física & Ecológica)
**Data:** 23 de Dezembro de 2025

---

## 1. Visão Estratégica
A transição para uma **Arquitetura Orientada a Vetores (Data-Oriented)** visa desacoplar a definição do cenário (Input) da lógica de simulação (Process). Adotamos o modelo **SCORPAN** como a estrutura fundamental de dados que permeia todos os sub-sistemas.

## 2. Linguagem Ubíqua (Ubiquitous Language)

*   **Scorpan Vector:** Um conjunto padronizado de vetores de estado que descreve completamente uma unidade mínima de paisagem.
*   **Landscape Cell (Célula de Paisagem):** A unidade atômica espacial $(x,y)$, que atua como o *Aggregate Root* contendo os 7 vetores SCORPAN.
*   **Forcing Vector (Vetor de Forçamento):** Vetores exógenos que impulsionam a mudança ($c, r, p$).
*   **State Vector (Vetor de Estado):** Vetores endógenos que evoluem com o tempo ($s, o$).
*   **Vector Processor:** Qualquer entidade (Função C++ ou Modelo ML) que transforma vetores de input em vetores de output.

---

## 3. Blocos de Construção (Building Blocks)

### 3.1. Aggregate Root: `LandscapeCell`
A entidade que garante a consistência transacional de um ponto no espaço.
```cpp
struct LandscapeCell {
    Position n;          // (n) Space
    ParentMaterial p;    // (p) Geology
    Relief r;            // (r) Topography
    Climate c;           // (c) Climate
    Age a;               // (a) Time
    
    // Mutable States
    SoilState s;         // (s) Soil
    OrganismState o;     // (o) Vegetation
};
```

### 3.2. Value Objects (Os Vetores)
Imutáveis ou substituídos inteiramente durante a atualização.

#### **$\mathbf{p}$ - ParentMaterial (Geologia)**
Define o potencial químico e físico da litologia.
*   `WeatheringRate` [0-1]: Velocidade de formação do solo.
*   `SandBias` [0-1]: Tendência a gerar textura arenosa.
*   `ClayBias` [0-1]: Tendência a gerar textura argilosa.
*   `BaseFertility` [0-1]: Riqueza mineral intrínseca.

#### **$\mathbf{r}$ - Relief (Topografia)**
Define o potencial energético e hidrológico.
*   `Elevation` [m]: Altura absoluta.
*   `Slope` [rad]: Declividade local (Gradiente hidráulico).
*   `Aspect` [rad]: Orientação solar.
*   `Curvature` [-1, 1]: Côncavo vs Convexo (Concentração de fluxo).

#### **$\mathbf{s}$ - SoilState (Estado do Solo)**
O resultado acumulado dos processos.
*   `Depth` [m]: Profundidade do *solum*.
*   `OM` [%]: Conteúdo de Matéria Orgânica.
*   `TextureClass` [Enum]: Classificação granulométrica.
*   `WaterContent` [mm]: Umidade atual.

#### **$\mathbf{o}$ - OrganismState (Vegetação)**
O estado da biomassa sobre o solo.
*   `Biomass_Grass` [kg/m2]: Cobertura de gramíneas.
*   `Biomass_Shrub` [kg/m2]: Cobertura arbustiva.
*   `RootsDensity` [0-1]: Estabilização do solo.

### 3.3. Domain Services (Processadores)

#### **`PedogenesisService` (SoilSystem**)
*   **Função:** $s_{t+1} = f(s_t, p, r, c, a)$
*   Responsabilidade: Aplicar intemperismo e erosão para evoluir o vetor $\mathbf{s}$.

#### **`EcologicalService` (VegetationSystem)**
*   **Função:** $o_{t+1} = f(o_t, s, c)$
*   Responsabilidade: Calcular crescimento de biomassa baseado no suporte do solo ($\mathbf{s}$).

#### **`DataInjectionService` (User Input)**
*   Responsabilidade: Permitir que o usuário "injete" vetores $\mathbf{p}$ customizados (Pintura Geológica) na grade.

---

## 4. Mapa de Contexto e Integração ML

O `EcofunctionalMachineLearning` (Subdomínio de Suporte) atua como um **Observador de Vetores**.
*   Ele não altera o estado físico.
*   Ele lê $\{s, o, r\}$ e prevê atributos visuais (Cor, Textura Renderizada).

$$ \text{Visual} = ML( \mathbf{s}, \mathbf{o} ) $$

Isso garante que a simulação permaneça determinística (física), enquanto a visualização pode ser estocástica/neural.
