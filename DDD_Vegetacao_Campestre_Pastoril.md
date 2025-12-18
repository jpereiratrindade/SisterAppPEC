# DDD — Modelo Conceitual de Simulação da Vegetação Campestre Pastoril

## 1. Domínio
**Domínio:**  
Simulação da dinâmica espaço-temporal da vegetação campestre em ambientes pastoris, baseada em comunidades vegetais funcionais e regimes de distúrbio.

**Subdomínios:**
- Vegetação Campestre (Core)
- Distúrbios Ecológicos (Core)
- Ambiente Físico (Supporting)
- Espaço-Tempo da Paisagem (Generic)

---

## 2. Entidades Centrais

### 2.1 LandscapeCell
Unidade mínima de simulação espacial.

**Identidade**
- CellID

**Responsabilidades**
- Manter estado ambiental local
- Manter estado da vegetação
- Receber impactos de distúrbio

**Atributos**
- environmentState
- vegetationState
- spatialContext

---

### 2.2 VegetationCommunity
Representa a comunidade vegetal funcional presente na célula.

**Identidade**
- CommunityID

**Responsabilidades**
- Manter composição funcional da vegetação
- Responder a distúrbios e condições ambientais

**Atributos**
- lowerStratum : StratumState
- upperStratum : StratumState

---

### 2.3 StratumState
Estado funcional de um estrato vegetal.

**Atributos**
- coverage [0.0 – 1.0]
- vigor [0.0 – 1.0]
- recoveryPotential [0.0 – 1.0]

**Tipos**
- LowerStratum (Estrato Inferior)
- UpperStratum (Estrato Superior)
  *Nota: No contexto pastoril, refere-se predominantemente a subarbustos e arbustos baixos (< 1m), sendo formas lenhosas altas menos frequentes.*

---

## 3. Value Objects

### 3.1 DisturbanceRegime
Define como o distúrbio atua.

**Atributos**
- spatialExtent
- frequency
- magnitude

---

### 3.2 SpatialExtent
- affectedAreaRatio
- patchSizeDistribution
- connectivityIndex

---

### 3.3 TemporalPattern
- frequency
- seasonality
- returnInterval

---

### 3.4 DisturbanceMagnitude
- biomassRemovalFraction
- selectivityProfile

---

### 3.5 EnvironmentalConditions
- soilType
- soilMoisture
- fertility
- climaticStressIndex

---

## 4. Agregados

### 4.1 LandscapePatch (Aggregate Root)
Conjunto de células ecologicamente conectadas.

**Responsabilidades**
- Garantir coerência espacial
- Permitir propagação lateral de comunidades

**Contém**
- LandscapeCell[]
- ConnectivityRules

---

### 4.2 VegetationSystem (Aggregate Root)
Orquestra a dinâmica da vegetação no tempo.

**Responsabilidades**
- Aplicar regras de transição
- Garantir invariantes ecológicas
- Acoplar ambiente, distúrbio e vegetação

---

## 5. Serviços de Domínio

### 5.1 DisturbanceImpactService
Traduz regimes de distúrbio em impactos funcionais diferenciados.

---

### 5.2 VegetationTransitionService
Atualiza proporções de estratos ao longo do tempo, considerando memória ecológica.

---

### 5.3 SpatialPropagationService
Permite expansão lateral de estratos entre células vizinhas.

---

## 6. Eventos de Domínio
- DisturbanceOccurred
- StratumCollapsed
- StratumRecovered
- CommunityShifted
- PatchHomogenized

---

## 7. Invariantes Ecológicas
1. EI.coverage + ES.coverage ≤ 1.0  
2. ES torna-se inflamável primariamente quando senescente (Vigor < 0.5) e abundante (Biomassa > 0.2)
3. EI não colapsa sob distúrbio crônico  
4. Recuperação de ES é mais lenta que sua degradação  
5. Distúrbio sem magnitude não altera vegetação  

---

## 8. Linguagem Ubíqua
- Estrato Inferior (EI)
- Estrato Superior (ES)
- Regime de Distúrbio
- Magnitude
- Frequência
- Propagação Espacial
- Persistência
- Transição
- Resiliência
- Memória Ecológica
- Mancha Campestre

---

## Síntese
O sistema simula a dinâmica da vegetação campestre como interação entre dois estratos funcionais coexistentes, cujas proporções emergem da aplicação de regimes de distúrbio tridimensionais modulados por condições ambientais, respeitando invariantes ecológicas.
