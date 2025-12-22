# Manual Teórico de Sistemas (SisterApp)

**Versão:** 1.0 (Ref: v4.3.10)
**Data:** 22 de Dezembro de 2025

Este documento detalha as bases teóricas e equações matemáticas implementadas nos subsistemas físicos do SisterApp. O objetivo é fornecer transparência sobre as aproximações e modelos científicos utilizados na simulação.

---

# 1. Pedologia (Soil System)

O sistema de solos é responsável por definir as propriedades estáticas e dinâmicas da camada superficial. Em vez de simular processos geológicos de milhões de anos, utilizamos uma abordagem heurística baseada em **correlação topográfica** (Pedometria).

## 1.1. Modelo Topográfico (Baseado em *scorpan*)
A distribuição dos solos é modelada primariamente em função do relevo ($\text{slope}$), seguindo a lei geral da pedogênese onde o solo se acumula em áreas planas e sofre erosão em áreas íngremes.

A profundidade do solo ($D$) para uma célula $(x,y)$ é dada por:

$$
D(x,y) = 
\begin{cases} 
0.1 + \eta_1(0.2) & \text{se } \text{slope} > 2.0 \quad (\text{Neossolo Litólico}) \\
0.5 + \eta_2(0.5) & \text{se } 0.5 < \text{slope} \le 2.0 \quad (\text{Cambissolo}) \\
1.0 + \eta_3(1.0) & \text{se } \text{slope} \le 0.5 \quad (\text{Latossolo/Gleissolo})
\end{cases}
$$

Onde $\eta \sim U(0,1)$ é um ruído uniforme estocástico.

## 1.2. Propriedades Físico-Químicas
Outras propriedades são derivadas da profundidade ($D$):

*   **Matéria Orgânica ($OM$)**: Assumimos correlação positiva com a profundidade (horizontes A mais espessos).
    $$ OM = \min(1.0, 0.5 \cdot D) $$
*   **Infiltração ($I$)**: Definida pelo tipo de solo (textura).
    *   *Arenoso/Franco*: Alto $I$ (drenagem rápida).
    *   *Argiloso/Rochoso*: Baixo $I$ (maior escoamento).

---

# 2. Hidrologia (Hydro System)

O sistema hidrológico resolve o balanço de água e o transporte de sedimentos (erosão).

## 2.1. Roteamento de Fluxo (D8 Algorithm)
Para determinar a direção da água, utilizamos o algoritmo **D8 (Deterministic 8-neighbor)**. A direção de fluxo $d_i$ de uma célula $i$ é para o vizinho $j$ que maximiza a declividade (*steepest descent*):

$$
S_{ij} = \frac{z_i - z_j}{L_{ij}}
$$

Onde $L_{ij}$ é a distância euclidiana (1 ou $\sqrt{2}$). A água flui apenas para *um* vizinho (fluxo unidirecional), simplificando a resolução para $O(N)$ após ordenação topológica.

## 2.2. Balanço Hídrico & Runoff
O escoamento superficial ($Q_{surf}$) é gerado quando a precipitação excede a capacidade de infiltração (Mecanismo *Hortonian*):

$$
Q_{input} = \max(0, P - I_{eff})
$$

A infiltração efetiva $I_{eff}$ é modulada pela vegetação (macroporosidade das raízes):
$$
I_{eff} = I_{base} \cdot (1 + k_{veg} \cdot B_{total})
$$

## 2.3. Erosão (Stream Power Law)
A erosão é modelada pela **Lei de Potência do Fluxo** (*Stream Power Law*), que relaciona a capacidade erosiva com a vazão e a declividade.

$$
E = K \cdot Q^m \cdot S^n \cdot (1 - P_{veg})
$$

*   $E$: Taxa de erosão ($\text{m/s}$).
*   $K$: Erodibilidade do solo.
*   $Q$: Fluxo acumulado (Runoff).
*   $S$: Declividade local.
*   $P_{veg}$: Fator de proteção da vegetação ($0 \to 1$).
*   $m, n$: Expoentes empíricos (tipicamente $m \approx 1.5, n \approx 1.0$ para fluxos rasos).

---

# 3. Vegetação (Ecological System)

A vegetação é modelada como um autômato celular contínuo, regido por equações diferenciais de crescimento logístico com interação competitiva.

## 3.1. Tipos Funcionais (PFTs)
Simplificamos o ecossistema em dois Plant Functional Types:
1.  **EI (Estrato Inferior)**: Gramíneas. Crescimento rápido, colonizador, combustível fino.
2.  **ES (Estrato Superior)**: Arbustos/Árvores. Crescimento lento, estrutural, combustível lenhoso.

## 3.2. Dinâmica de Crescimento
A variação da biomassa (cobertura $C$) ao longo do tempo $t$ segue:

$$
\frac{dC_{EI}}{dt} = r_{EI} \cdot C_{EI} \left( 1 - \frac{C_{EI}}{K_{EI}} \right) - \mu_{EI} \cdot C_{EI}
$$

Onde:
*   $r$: Taxa intrínseca de crescimento.
*   $K$: Capacidade de suporte local (função da profundidade do solo).
*   $\mu$: Taxa de mortalidade/dieback.

## 3.3. Competição e Facilitação
O modelo implementa interações interespecíficas:
*   **Competição por Espaço**: A soma das coberturas não pode exceder 100%. O estrato arbustivo ($ES$) tem dominância hierárquica sobre a grama ($EI$) por sombreamento.
    $$ C_{EI}^{real} = \min(C_{EI}, 1 - C_{ES}) $$
*   **Facilitação (Nucleação)**: Arbustos crescem melhor onde já existe grama (retenção de umidade/nutrientes).
    $$ \frac{dC_{ES}}{dt} \propto C_{EI} \quad (\text{Se } C_{EI} > 0.7) $$

## 3.4. Distúrbios (Fogo)
O risco de ignição segue um modelo probabilístico baseado na carga de combustível e umidade (Vigor $V$):

$$
P(fire) \propto (C_{EI} \cdot (1 - V_{EI})) + \alpha \cdot (C_{ES} \cdot (1 - V_{ES}))
$$

Isso captura a física do fogo: áreas com muita biomassa ($C$ alto) mas muito secas ($V$ baixo) são as mais inflamáveis.

---

**Referências Bibliográficas:**
1.  *McBratney, A. B., et al. (2003). On digital soil mapping.*
2.  *O'Callaghan, J. F., & Mark, D. M. (1984). The extraction of drainage networks from digital elevation data.*
3.  *Whittaker, R. H. (1975). Communities and Ecosystems.*
