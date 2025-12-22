# Domain-Driven Design: Sistema de Solos (Arquitetura Vetorial SCORPAN)

**Contexto:** `SisterApp::Landscape::Soil`
**Definição:** Sistema definido por Vetores de Dados Parametrizáveis.
**Versão:** SCORPAN v2.0 (Consolidada)
**Data:** 22 de Dezembro de 2025

---

## 1. Princípio Fundamental de Projeto

> **REGRA DE OURO:** O Sistema Solo NÃO é definido por estados arbitrários, mas pela interação de vetores de POTENCIAL e FORÇAMENTO.
> *   O usuário define os POTENCIAIS (P, R) e FORÇAMENTOS (C, O, A).
> *   O sistema calcula o ESTADO emergente (S).
> *   **O solo não é escolhido. O solo acontece.**

Isso garante:
1.  **Causalidade Científica:** Todo solo tem um "porquê" físico.
2.  **Rastreabilidade:** É possível auditar quais fatores levaram àquele estado.

---

## 2. A Máquina de Vetores SCORPAN

A simulação opera resolvendo a equação de estado:
$$ S_{t+1} = f(S_t, P, R, C, O, A, N, \Delta t) $$

### 2.1 Vetor P – Parent Material (Geologia)
**Status:** TOTALMENTE PARAMETRIZÁVEL
Define o **Potencial Litológico**.
*   **Atributos:**
    *   `Weathering Rate` [0-1]: Facilidade de transformação rocha $\to$ solo.
    *   `Base Fertility` [0-1]: Reserva nutricional mineral.
    *   `Texture Bias` (Sand/Clay) [0-1]: Tendência granulométrica física.
*   **Interação:** O usuário escolhe/pinta a rocha e ajusta seus sliders.

### 2.2 Vetor R – Relief (Relevo)
**Status:** PARCIALMENTE PARAMETRIZÁVEL (Sensibilidade)
Define o **Controle Gravitacional**.
*   **Atributos:**
    *   `Slope Sensitivity`: O quanto a gravidade acelera a erosão.
    *   `Curvature Weight`: O quanto a forma do terreno concentra fluxos.
*   **Interação:** O relevo bruto vem do terreno, mas o usuário ajusta a "agressividade" dos processos topográficos.

### 2.3 Vetor C – Climate (Clima)
**Status:** PARAMETRIZÁVEL POR CENÁRIOS
Define o **Forçamento Atmosférico**.
*   **Atributos:**
    *   `Rain Intensity`: Energia cinética da chuva (Erosão).
    *   `Seasonality`: Pulsos de umidade (Intemperismo vs Seca).
*   **Interação:** Seleção de cenários (Tropical Úmido, Semiárido, etc.).

### 2.4 Vetor O – Organisms (Biota)
**Status:** PARCIALMENTE PARAMETRIZÁVEL (Pressões)
Define o **Potencial Biológico**.
*   **Atributos:**
    *   `Max Cover`: Capacidade máxima de biomassa.
    *   `Disturbance`: Pressão de pastejo ou frequência de fogo.
*   **Interação:** O usuário define o manejo, não a vegetação instantânea.

### 2.5 Vetor A – Age (Tempo)
**Status:** CONTROLE GLOBAL
Define a **Integral Temporal**.
*   **Atributos:** `Time Scale`, `Total Duration`.

### 2.6 Vetor N – Space (Espaço)
**Status:** DERIVADO (Não Parametrizável)
Define a **Conectividade**.
*   Emerge da topologia do grid (D8 Flow, Vizinhança).

---

## 3. O Resultado: Vetor S (Soil State)

**Status:** NÃO PARAMETRIZÁVEL (Read-Only)
É a **Memória** do sistema, acumulando o histórico de interações.

*   `Depth` ($d$): Espessura do solum.
*   `Texture`: Resultante de P + Transporte (R/N).
*   `Organic Matter`: Resultante de O - Decomposição (C).
*   `Water Status`: Infiltração e Retenção.

> **Ressalva Crítica:** Permitir a edição direta de S quebraria a causalidade. O usuário observa S, ele não o edita.

---

## 4. Interfaces de Visualização (Dual View)

O SisterApp oferece duas formas de "Ler" o solo, mas apenas uma de "Gerar":

### 4.1 Modo Geométrico (Legacy/Fast)
Uma heurística simplificada que mapeia Slope $\to$ Tipo de Solo diretamente.
*   **Uso:** Visualização rápida de aptidão agrícola teórica baseada apenas em topografia.
*   **Classes:** 5 Fixas (Hidromórfico... Rocha).

### 4.2 Modo SCORPAN (Simulation)
A visualização real do Vetor S.
*   **Uso:** Análise ecossistêmica e validação científica.
*   **Saída:** Gradientes contínuos ou classificação emergente baseada nas propriedades de S.

---

## 5. Síntese da Implementação

A arquitetura deve garantir que:
1.  Os inputs da UI (Geology Inspector, Climate Settings) alimentem apenas os vetores P, C, O, R.
2.  O `SoilSystem::update` seja a única forma de escrita no Vetor S.
3.  A visualização (Renderer) possa alternar entre interpretar o terreno via "Regras Geométricas" ou via "Estado S".

---

## 6. Invariantes Arquiteturais e Evolução

### 6.1 Decomposição do Vetor S (Roadmap)
Atualmente, S é tratado como um vetor monolítico. Para evitar conflitos de processos (ex: erosão vs decomposição competindo pelo mesmo escalar), a evolução do sistema deve tratar S como uma composição de sub-vetores:
$$ S = \{ S_{mineral}, S_{orgânico}, S_{hídrico} \} $$
*   **$S_{mineral}$:** Frações texturais, profundidade do regolito.
*   **$S_{orgânico}$:** Carbono lábil, recalcitrante, biomassa morta.
*   **$S_{hídrico}$:** Água atual, capacidade de campo, condutividade.

### 6.2 O Invariante do Fator A (Age)
> **Regra de Implementação:** O Fator A (Tempo) **NUNCA** deve alterar o estado S diretamente.

*   **Incorreto:** `if (Age > 1000) Soil.Depth = 1.0m;` (Atalho teleológico).
*   **Correto:** `Soil.Depth += WeatheringRate * dt;` (Integração temporal).

O Tempo apenas define **quantas vezes** a função de processo $f()$ é aplicada. A profundidade emerge da acumulação de intemperismo menos a erosão ao longo dos passos de tempo.
