# DDD — Representação da Vegetação Campestre
## Domínio de Tradução Ontológica (Ecologia → Paisagem)

ARQUIVO: DDD_Representacao_Vegetacao_Campestre.md.txt
DATA: 17/12/2025

==================================================
1. DOMÍNIO
==================================================

**Domínio:**  
Representação gráfica e fenomenológica da vegetação campestre a partir de estados
ecológicos simulados, preservando coerência ontológica com o modelo ecológico.

**Princípio Fundamental:**  
A representação não cria realidade ecológica; ela traduz estados ecológicos em
formas perceptíveis e mensuráveis.

**Subdomínios:**
- Tradução Semântica (Core)
- Visualização Gráfica (Supporting)
- Modos de Leitura (Supporting)
- Observáveis Derivados (Generic)

==================================================
2. LINGUAGEM UBÍQUA DA REPRESENTAÇÃO
==================================================

| Termo Representacional | Significado |
|----------------------|-------------|
| Base Verde            | Continuidade fotossintética percebida |
| Estrutura             | Rugosidade e heterogeneidade visual |
| Vitalidade            | Intensidade funcional percebida |
| Atenuação             | Redução visual por estresse |
| Verdejamento          | Leitura sintética tipo NDVI |
| Padrão Visual         | Manifestação de manchas |
| Perturbação Aparente  | Sinal visual de distúrbio |

==================================================
3. ENTIDADES CENTRAIS
==================================================

### 3.1 RepresentationCell
Unidade mínima de leitura visual da paisagem.

**Identidade**
- CellID (compartilhada com o domínio espacial)

**Atributos**
- baseGreenness
- structuralRoughness
- vitality
- visualStress

**Responsabilidade**
- Armazenar o estado visual traduzido de uma célula

--------------------------------------------------

### 3.2 LandscapeAppearance
Estado visual agregado da paisagem.

**Responsabilidades**
- Garantir coerência visual espacial
- Suportar múltiplos modos de leitura
- Agregar RepresentationCells

==================================================
4. VALUE OBJECTS
==================================================

### 4.1 SemanticMapping
Define explicitamente como estados ecológicos são traduzidos em estados visuais.

**Exemplos**
- EI.coverage → baseGreenness
- ES.coverage → structuralRoughness
- vigor → vitality
- estresse → visualStress

--------------------------------------------------

### 4.2 VisualMode
Define o modo de leitura da paisagem.

**Tipos**
- Realista
- NDVI_FalsoCor (Simulated Remote Sensing)
- Estrutural
- Estresse
- Debug

--------------------------------------------------

### 4.4 NDVISimulator
Value Object responsável pela síntese de bandas espectrais virtuais.

**Fórmula Simulada**
`NDVI_Sim = 0.1 + (Biomass * Vigor * 0.8)`
Range Padrão (Scientific):
- **< 0.0**: Água / Nuvens (Não simulado na vegetação, mas na água)
- **0.0 - 0.2**: Solo Exposto (BTextural, Raso)
- **0.2 - 0.5**: Vegetação Esparsa (EI inicial)
- **0.5 - 1.0**: Vegetação Densa e Vigorosa (EI + ES Full)

--------------------------------------------------

### 4.5 VisualStressSignal
Define a assinatura visual de estresse pré-colapso ("Atenuação").

**Atributos**
- OpacityDrop (Transparência/Secura)
- RoughnessSmoothing (Perda de turgor)
- ColorShift (Amarelecimento)

--------------------------------------------------

### 4.3 RepresentationScale
Escala perceptiva ativa.

**Tipos**
- Paisagem
- Mancha
- Local

==================================================
5. AGREGADO RAIZ
==================================================

### VegetationRepresentationSystem (Aggregate Root)

**Responsabilidades**
- Receber estado ecológico (read-only)
- Aplicar mapeamentos semânticos
- Gerar estados visuais coerentes
- Garantir invariantes da representação

==================================================
6. SERVIÇOS DE DOMÍNIO
==================================================

### 6.1 SemanticTranslatorService
Traduz EI, ES, vigor e estresse em canais visuais.

--------------------------------------------------

### 6.2 NDVIVisualInterpreter
Define como o verdejamento (NDVI simulado) é apresentado visualmente.

--------------------------------------------------

### 6.3 PatchVisualSynthesizer
Assegura que manchas ecológicas correspondam a padrões visuais coerentes.

==================================================
7. EVENTOS DE DOMÍNIO
==================================================

- RepresentationUpdated
- VisualModeChanged
- ScaleChanged

==================================================
8. INVARIANTES DA REPRESENTAÇÃO
==================================================

4. Modos visuais não alteram o estado ecológico
5. Nenhuma entidade visual existe sem correspondência ecológica
6. Estresse fisiológico deve ser visível antes do colapso de biomassa (Pre-Signaling)
7. A escala de visualização (LOD) não pode alterar a semântica da mancha (apenas a precisão)

==================================================
9. RELAÇÃO COM O DOMÍNIO ECOLÓGICO
==================================================

- O domínio ecológico é a fonte única de verdade
- A representação é estritamente derivada
- O acoplamento ocorre via contratos semânticos explícitos
- Não há feedback visual → ecológico

==================================================
10. SÍNTESE
==================================================

A representação da vegetação campestre constitui um domínio próprio de tradução
ontológica, responsável por transformar estados ecológicos em paisagens
perceptíveis, mantendo fidelidade funcional, espacial e temporal ao modelo de
vegetação.

Documento de referência para implementação gráfica e visual do SisTerApp.
