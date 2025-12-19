# SisterApp: Plataforma de Ecologia Computacional v3.8.4
## Manual Técnico de Modelos Computacionais

**Autor:** José Pedro Trindade  
**Data:** 19 de Dezembro de 2025

---

# 1. Introdução
O **SisterApp** evoluiu de uma engine gráfica para uma plataforma científica robusta focada em ecologia computacional. A versão 3.8.0 consolida ferramentas de navegação, análise de paisagem e validação métrica. A versão 3.8.4 remove definitivamente o suporte a Voxel para focar em terrenos de alta fidelidade (Finite World).

# 2. Modelo de Análise de Declividade (Slope Analysis)
Este modelo substitui a anterior lógica abstrata de resiliência por uma abordagem quantitativa baseada na inclinação local do terreno.

### 2.1. Cálculo de Inclinação (Percentual)
A declividade é calculada como a razão entre a diferença de altura (rise) e a distância horizontal (run), expressa em porcentagem. Para um ponto no terreno, a inclinação *S%* é dada por:

$$
S = \frac{\sqrt{(\Delta x)^2 + (\Delta z)^2}}{run} \times 100
$$

Onde:
*   *Δx* e *Δz* são os gradientes de altura nas direções X e Z.
*   A distância base (*run*) é definida pela resolução do voxel (2 unidades).

Isto permite uma correlação direta com normas técnicas de engenharia civil.

## 2.2. Classificação Topológica (5 Classes)
O terreno é segmentado em classes configuráveis pelo usuário. Os limiares (thresholds) padrão são:

| Classe | Intervalo (*S%*) | Descrição |
| :--- | :--- | :--- |
| Flat (Plano) | 0% - 3.0% | Áreas adequadas para infraestrutura. |
| Gentle Slope (Suave) | 3.0% - 8.0% | Áreas de transição suave. |
| Rolling (Ondulado) | 8.0% - 20.0% | Terreno ondulado, requer terraplanagem. |
| Steep Slope (Íngreme/Forte) | 20.0% - 45.0% | Encostas fortes, risco de erosão. |
| Mountain (Montanha) | > 45.0% | Áreas inacessíveis ou de preservação. |

# 3. Modelo de Vegetação Campestre (Grassland Model)
O SisterApp v3.9.1 introduz um modelo de dinâmica de vegetação campestre baseado em princípios de ecologia espacial e regimes de distúrbio (Fogo e Pastejo). Devido à complexidade biológica, este módulo possui uma **Documentação de Domínio (DDD) Exclusiva** que define suas regras e invariantes.

## 3.1. Estrutura de Dois Estratos (DDD)
A vegetação é modelada como dois estratos competitivos em cada célula do grid ($1m^2$):
*   **Estrato Inferior (EI):** Gramíneas e herbáceas. Alta taxa de crescimento, alta resiliência ao pastejo.
*   **Estrato Superior (ES):** Subarbustos e arbustos baixos (geralmente < 1m). Crescimento lento e não pastejado. Apesar do porte baixo, acumula biomassa lenhosa que, quando senescente, atua como combustível principal.

O estado de cada célula é definido por um vetor de 4 componentes:
**V_cell = { Coverage_EI, Coverage_ES, Vigor_EI, Vigor_ES }**

## 3.2. Definição de Escala e Resolução
A célula (*V_cell*) é a unidade atômica da simulação, representando um estado homogêneo estatístico. Embora a simulação opere em espaço de grade abstrato (Grid Space), a interpretação física da "Cobertura" (*C*) é dependente da resolução métrica configurada na interface:

### 3.2.1. Resolução Física (*R*)
A dimensão espacial da célula é definida pelo parâmetro **Cell Size (Resolution)**, acessível na interface do usuário sob o menu "Map Generator". Este parâmetro permite o ajuste dinâmico em tempo de execução de *R* ∈ [0.1m, 4.0m]. O valor padrão é *R* = 1.0m. A área física de uma célula é dada por *Area_cell* = *R*².

### 3.2.2. Interpretação da Cobertura
O valor de cobertura *C* ∈ [0, 1] representa a fração da área da célula ocupada pelo estrato:
**Área Ocupada (m²) = C · R²**

Exemplos:
*   Para *R*=1.0m (Padrão): *C*=0.5 implica 0.5m² de biomassa.
*   Para *R*=0.5m (Alta Definição): A área total é 0.25m², logo *C*=0.5 implica 0.125m².

Esta abstração permite que o modelo seja agnóstico à escala (Scale-Independent) em sua lógica interna, enquanto a renderização adapta a densidade visual à geometria física.

### 3.2.3. Definição de Vigor (φ)
O Vigor (*φ* ∈ [0, 1]) não representa biomassa, mas sim o **estado fisiológico** da planta.
*   *φ* ≈ 1.0: Turgor máximo, alta clorofila, baixo estresse hídrico. (Visual: Verde)
*   *φ* < 0.5: Senescência ou estresse severo. (Visual: Amarelo/Marrom)
*   *φ* = 0.0: Planta morta (Necromassa em pé).

Diferente de modelos estáticos, o Vigor no SisterApp é dinâmico e espacialmente heterogêneo:
**φ_target(x,y) = 0.8 + 0.2 · Noise(x,y)**
A vegetação busca constantemente esse alvo, simulando microclimas variados. Se perturbada (ex: pisoteio ou seca não simulada explicitamente), o vigor decai, aumentando a probabilidade de fogo (*F*) antes mesmo da perda de cobertura.

## 3.3. Definição Operacional do Distúrbio
O distúrbio (*D*) é definido como uma variável escalar composta que integra três dimensões fundamentais:
*   **Magnitude** (*M*): intensidade do evento;
*   **Frequência Ecológica** (*F*): probabilidade relativa do regime (0.1 = Raro, 0.9 = Crônico);
*   **Escala Espacial** (*E*): proporção da paisagem afetada.

**D = M · F · E**

## 3.4. Resposta Funcional ao Distúrbio
O sistema modela a capacidade de suporte (*K*) de cada estrato como uma função direta do regime de distúrbio vigente:

### 3.4.1. Estrato Inferior (EI - Gramíneas)
Apresenta resposta **logarítmica positiva**:
**R_EI(D) = clamp(log(1 + α·D), 0, 1)**
Onde *α* é o coeficiente de sensibilidade (Ganho). Ecologicamente, isso expressa que distúrbios de baixa intensidade já promovem forte resposta do EI, saturando em regimes intensos.

### 3.4.2. Estrato Superior (ES - Arbustos)
Apresenta resposta **exponencial negativa**:
**R_ES(D) = clamp(e^(-β·D), 0, 1)**
Onde *β* é o coeficiente de decaimento. Isso captura a supressão acelerada de lenhosas sob regimes de perturbação frequente.

## 3.5. Modulação da Capacidade de Suporte
Ao contrário do modelo anterior de remoção direta, os índices *R_EI* e *R_ES* modulam os alvos de equilíbrio:
**K_EI_target ∝ R_EI(D)**  e  **K_ES_target ∝ R_ES(D)**
A vegetação então "relaxa" em direção a esses alvos ao longo do tempo (*τ_rec*), permitindo transições suaves entre estados de savana, campo limpo e arbustal.

## 3.6. Heterogeneidade Espacial (Spatial Noise)
Para evitar a monotonia visual e simular a variabilidade edáfica não mapeada, a inicialização e a capacidade de suporte local ($K$) são moduladas por funções de ruído procedural (Perlin Noise). Isso gera manchas naturais de alta e baixa densidade, independentemente dos distúrbios.

## 3.7. Visualização em Tempo Real
O shader de terreno foi atualizado para combinar a coloração do solo (Pedologia) com a camada de vegetação.
*   **Realistic Mode:** Mistura texturas de solo e vegetação baseada na cobertura. O Vigor modula a cor entre Verde (Saudável) e Amarelo/Marrom (Senescente).
*   **Heatmaps:** Modos de diagnóstico para visualizar cobertura bruta de EI/ES e níveis de estresse fisiológico (Vigor).

## 3.8. Inovações de Visualização Científica (Roadmap)
Para aumentar a fidelidade ontológica da plataforma, foram definidos conceitos avançados de tradução visual (DDD Visual):

### 3.8.1. Síntese de NDVI Virtual
Para validação cruzada com sensoriamento remoto, o sistema prevê a geração de "Falso-Cor" baseada na reflectância espectral simulada:
$$ NDVI_{sim} = \frac{(C_{total} \cdot Vigor) - (1 - Vigor)}{(C_{total} \cdot Vigor) + (1 - Vigor)} $$
Isso permite comparar diretamente os output do modelo com imagens Sentinel-2 ou Landsat.

### 3.8.2. Sinalização de Estresse (Early Warning)
Diferente de engines de jogos que apenas "removem" a planta morta, o SisterApp implementa a **Atenuação Visual**: plantas sob estresse hídrico ou de pastejo manifestam redução de turgor (suavização de normal map) e transparência antes da perda efetiva de biomassa.

## 3.9. Formulação Matemática Detalhada
O modelo utiliza uma abordagem híbrida de Autômatos Celulares e Equações Diferenciais Discretas. O estado de cada célula $i$ no tempo $t$ é vetorizado como:
$$ V_i(t) = [ C_{EI}, C_{ES}, \phi_{EI}, \phi_{ES}, \tau_{rec} ] $$
Onde $C$ é a cobertura (0-1), $\phi$ é o vigor fisiológico (0-1) e $\tau_{rec}$ é o temporizador de recuperação pós-distúrbio.

### 3.9.1. Dinâmica de Crescimento e Recuperação
Quando $\tau_{rec} \le 0$, a vegetação recupera biomassa seguindo uma função logística linearizada próxima à capacidade de suporte $K$:

$$
\frac{dC_{EI}}{dt} = r_{EI} \cdot (K_{EI} - C_{EI}) \cdot \phi_{EI}
$$

Implementado numericamente como:
```cpp
if (grid.ei_coverage[i] < maxEI) {
    grid.ei_coverage[i] += 0.1f * dt; // Taxa r = 0.1
    if (grid.ei_coverage[i] > maxEI) grid.ei_coverage[i] = maxEI;
}
```

A colonização por arbustos ($C_{ES}$) é dependente da presença prévia de gramíneas ($C_{EI} > 0.7$), simulando a sucessão secundária facilitada.

#### Determinismo e Estocasticidade
Para garantir a reprodutibilidade científica dos experimentos, a geração de números aleatórios para eventos estocásticos utiliza um gerador `Mersenne Twister` (std::mt19937) com semente fixa, eliminando a variabilidade não-controlada entre execuções.

### 3.9.2. Probabilidade de Fogo (Fire Probability)
A ignição estocástica é calculada localmente com base na inflamabilidade do combustível disponível. O modelo adota a premissa ecológica de que **arbustos secos** ($ES_{dry}$) são o principal vetor de propagação:

1. **Fator de Secura ($\delta_{ES}$):**
$$ \delta_{ES} = \max(0, 1.0 - \phi_{ES}) $$

2. **Inflamabilidade ($\mathcal{F}_i$):**
$$ \mathcal{F}_i = \underbrace{C_{ES} \cdot (2\delta_{ES})}_{\text{Lenhoso Seco}} + \underbrace{C_{EI} \cdot 0.3 \cdot (1-\phi_{EI})}_{\text{Fino Morto}} $$
*Nota: A contribuição do ES é nula se $\delta_{ES} < 0.5$ (Vigor > 0.5).*

3. **Probabilidade de Ignição ($P_{ign}$):**
$$ P_{ign} = P_{base} + \alpha \cdot \mathcal{F}_i $$
Onde $P_{base} = 0.05$ e $\alpha = 0.8$. Se um número aleatório $R \in [0,1] < P_{ign}$, ocorre a remoção total de biomassa.

### 3.9.3. Dinâmica de Pastejo
O pastejo atua como uma força de remoção seletiva sobre o Estrato Inferior:
$$ C_{EI}(t+\Delta t) = C_{EI}(t) - I_{grazing} \cdot \Delta t $$
Com impacto colateral no vigor:
$$ \phi_{EI}(t+\Delta t) = \phi_{EI}(t) - \frac{I_{grazing}}{2} \cdot \Delta t $$
Isso simula o "superpastejo" que reduz não apenas a biomassa, mas a capacidade fotossintética futura.

### 3.9.4. Implementação Computacional
Para garantir a escalabilidade em grandes paisagens ($> 16 \text{ milhões de células}$), o sistema adota uma abordagem de _Fixed-Time Step Simulation_:
*   **Frequency:** 5-10 Hz (desacoplado do Frame Rate de renderização).
*   **Throttling:** A atualização de estado e a transferência de dados (CPU $\to$ GPU) são limitadas temporalmente para evitar gargalos no barramento PCIe.

# 4. Geração de Topologia (Terrain Models)
É fundamental distinguir o **Gerador de Topologia** do **Analisador de Declividade**. O sistema mantém três perfis de geração baseados em ruído Perlin, que definem a geometria física do mundo.

## 4.1. Modelo Experimental Blend (v3.8.3)
Este modelo introduz a composição ponderada de frequências de ruído, permitindo controle fino sobre a morfologia do terreno.
$$ H(x,z) = \text{Norm} \left( \sum_{i \in \{L, M, H\}} W_i \cdot Noise(x \cdot f_i, z \cdot f_i) \right)^{\gamma} $$
Onde:
*   $W_L, W_M, W_H$ são os pesos para frequências Baixa, Média e Alta.
*   $\gamma$ é o expoente de nitidez (Sharpness).
*   **Normalização:** Para evitar o achatamento (clipping) observado em versões anteriores, a soma ponderada é normalizada pelo total dos pesos ($\sum W_i$) antes de ser mapeada para a altura final.

## 4.2. Perfis Padrão
*   **Rippled Flat:** Baixa frequência base, gera predominantemente classes _Flat_ e _Gentle_.
*   **Smooth Hills:** Frequência média, introduz áreas _Rolling_.
*   **Rolling Hills:** Alta amplitude, necessária para gerar áreas _Steep_ e _Mountain_ para validação.

O fluxo de processamento é:
$$ \text{Modelo (Geometria)} \rightarrow \text{Heightmap Grid} \rightarrow \text{Slope Analysis (Classificação)} $$

# 5. Configuração do Usuário
Interface atualizada no menu _Tools_:
*   **Slope Sliders:** Ajuste dos limites percentuais para cada classe.
*   **Probe Tool:** Ferramenta de diagnóstico (clique esquerdo) mostra o valor de $S$ (declividade, em %).

## 5.1. Variáveis de Controle Espacial
O sistema permite o ajuste fino da topografia através de três variáveis principais:
1.  **Feature Size (Frequência):** Controla o tamanho horizontal das montanhas. Valores menores geram grandes maciços; valores maiores geram colinas frequentes.
2.  **Roughness (Persistência):** Controla a irregularidade da superfície.
    *   Baixa ($<0.5$): Colinas suaves e dunas.
    *   Alta ($>0.5$): Terreno rochoso, escarpado e ruidoso.
3.  **Amplitude:** A altura máxima vertical em metros.
4.  **Cell Size (Resolução):** A dimensão física de cada pixel da grade (em metros).

# 6. Modelo de Drenagem (D8 Flow)
A partir da versão v3.6.0, o sistema substituiu o modelo estocástico de erosão por partículas por um algoritmo determinístico de drenagem D8 (Steepest Descent).

## 6.1. Direção do Fluxo (Flow Direction)
Para cada célula do grid de terreno, o algoritmo determina a direção de escoamento para um dos 8 vizinhos com maior **declividade** descendente (Steepest Slope).
$$
\text{Receiver} = \text{argmax}_{n \in \text{Neighbors}} \left( \frac{H_{\text{current}} - H_n}{\text{Distance}_n} \right)
$$
Onde $\text{Distance}_n$ é a distância física até o vizinho (Resolution para cardeais, $\text{Resolution} \times \sqrt{2}$ para diagonais).
Se o declive for $\leq 0$ para todos os vizinhos (mínimo local), a célula é um "sink" (sumidouro).

## 6.2. Acumulação de Fluxo (Flow Accumulation)
O fluxo é calculado iterativamente, ordenando as células por altura (decrescente). Cada célula transfere seu valor de fluxo acumulado para o seu vizinho receptor (Receiver), simulando a conservação de massa da água.
$$
F_{\text{receiver}} += F_{\text{upstream}}
$$
O resultado é um _Flux Map_ onde valores altos representam rios e canais principais.

## 6.3. Visualização
O shader utiliza o mapa de fluxo acumulado para renderizar recursos hídricos:
*   **Canais Principais:** Células com fluxo $F > 1.0$ (limite visual configurável) são coloridas em Cyan (0.0, 0.8, 1.0).
*   **Continuidade:** O método D8 garante redes de drenagem dendríticas contínuas sem artefatos geométricos ("spots").

# 7. Análise de Bacias Hidrográficas (Watershed Analysis)
Introduzido na versão v3.6.3, este módulo permite a identificação e delimitação de bacias de drenagem baseadas na topologia D8.

## 7.1. Segmentação Global
O algoritmo de segmentação particiona todo o terreno em bacias distintas. O processo ocorre em duas etapas:
1.  **Identificação de Sinks**: Localização de todos os "sumidouros" (minimos locais ou bordas do mapa). Cada sink recebe um ID único.
2.  **Propagação Upstream (BFS)**: Um algoritmo de busca em largura (Breadth-First Search) percorre a rede de fluxo no sentido inverso (de jusante para montante), atribuindo o ID do sink a todas as células constituintes de sua área de contribuição.

## 7.2. Delineação Interativa
Permite ao usuário consultar a bacia de contribuição de um ponto arbitrário $P(x,y)$. O sistema rastreia recursivamente todos os vizinhos que fluem para $P$, gerando uma máscara binária instantânea da área de captação a montante.

## 7.3. Visualização de Contornos
O usuário pode habilitar a opção "Show Contours" na interface. O sistema utiliza a derivada parcial do ID da bacia (via shader `fwidth`) para detectar arestas onde o ID muda, desenhando uma linha escura de 1 pixel sobre os limites das bacias para melhor distinção visual.

# 8. Métricas Eco-Hidrológicas
O Relatório Hidrológico foi expandido para incluir indicadores funcionais derivados da topografia:

## 8.1. Índice Topográfico de Umidade (TWI)
$$ TWI = \ln \left( \frac{A}{\tan \beta} \right) $$
Onde $A$ é a área de contribuição específica (fluxo) e $\tan \beta$ é a declividade local. O TWI estima zonas de saturação do solo. O sistema reporta a porcentagem da área com $TWI > 8.0$ como proxy para zonas úmidas.

## 8.2. Densidade de Drenagem ($D_d$)
$$ D_d = \frac{L_{total}}{Area_{total}} $$
Calculado como a razão entre células classificadas como "rio" (Fluxo > 100) e o total de células. Indica a permeabilidade e dissecação do relevo.

## 8.3. Estatísticas por Bacia (Basin-Level Metrics)
O sistema agora agrega métricas de elevação, declividade, TWI e densidade de drenagem individualmente para as 3 maiores bacias identificadas, permitindo uma análise comparativa da resposta hidrológica de diferentes sub-regiões do modelo.

## 8.4. Geração Assíncrona (v3.8.3)
A partir da versão 3.8.3, a geração de terrenos (especialmente em resoluções altas como $4096 \times 4096$) é executada de forma assíncrona em uma thread separada. Isso previne o congelamento da interface ("Not Responding") durante o processamento de milhões de células. Uma tela de carregamento informa o progresso ao usuário.

# 9. Resolução Espacial Variável (V3.6.5)
Para atender à necessidade de maior definição nos limites de bacias e redes de drenagem, foi introduzido o controle de **Cell Size (Resolução)**.

## 9.1. Definição de Escala
O usuário pode ajustar o tamanho métrico de cada célula (pixel) da grade de simulação:
*   **1.0 m (Padrão):** Equilíbrio entre cobertura de área e detalhe.
*   **$< 1.0$ m (Alta Resolução):** Aumenta a densidade de vértices por unidade de área. Ideal para suavizar limites de bacias e detalhar canais de drenagem, reduzindo o efeito de "pixelização" (aliasing geométrico).
*   **$> 1.0$ m (Baixa Resolução):** Permite cobrir grandes extents geográficos com menor custo computacional.

O sistema ajusta automaticamente a visualização e a lógica de interação (raycasting) para manter a coerência espacial independentemente da escala escolhida.

# 10. Módulo de Análise de Solos (V3.7.3)
O sistema inclui agora uma camada de pedologia probabilística baseada na declividade, conforme a tabela de relação Relevo-Solo definida pelo usuário.

## 10.1. Metodologia: Ruído Coerente e Métricas de Paisagem (v3.8.0)
Para reproduzir os padrões espaciais descritos pelos índices de Ecologia da Paisagem (LSI, CF, RCC), o sistema substituiu a distribuição aleatória simples por um algoritmo de **Competição de Padrões** baseado em ruído procedural (Perlin/Simplex).

Cada tipo de solo possui um "perfil de ruído" configurado para mimetizar suas métricas (Ver Tabela 2):
*   **Domain Warping (Distorção):** Simula o LSI. Solos com alto LSI sofrem forte distorção de coordenadas, criando bordas complexas.
*   **Frequência e Rugosidade:** Simulam o CF. Solos com alto CF utilizam mais oitavas de ruído fractal.
*   **Anisotropia (Estiramento):** Simula o RCC. Solos com baixo RCC são esticados em um eixo para criar formas alongadas.

O solo final em cada pixel é determinado por uma competição onde o tipo com maior "força" de padrão local vence (dentre os candidatos válidos para a declividade).

## 10.2. Minimap e Navegação Interativa
A versão 3.8.0 introduz um Minimapa e controles de câmera aprimorados para facilitar a navegação e a compreensão espacial.
*   **Visualização Top-Down:** Renderiza o mapa de solos e relevo com neblina de guerra (Fog of War) simulada pela distância.
*   **Símbolos (Alegorias):** Um algoritmo de detecção de picos identifica máximos locais na topografia e desenha pequenos triângulos brancos, fornecendo referências visuais "game-like" para orientação.
*   **Nível da Água (Water Level):** Visualização configurável de zonas submersas (Azul), permitindo identificar depressões e lagos mesmo antes da simulação hidrológica.
*   **Controles:**
    *   **Zoom:** Roda do mouse ajusta o Campo de Visão (FOV) no modo voo livre.
    *   **Minimap Zoom/Pan:** Roda do mouse e botão do meio dentro da janela do minimapa.
    *   **Teleporte:** Clique com botão esquerdo no minimapa para viagem rápida.

## 10.3. Classes e Cores
*   **Plano (0-3%):** Hidromórfico (Teal), B Textural (Laranja), Argila Expansiva (Roxo).
*   **Suave (3-8%):** B Textural, Bem Desenvolvido (Terracota), Argila Expansiva.
*   **Ondulado (8-20%):** B Textural, Argila Expansiva.
*   **Forte (20-45%):** B Textural, Solo Raso (Amarelo).
*   **Montanhoso (45-75%):** Solo Raso (Amarelo).
*   **Escarpado ($>75\%$):** Afloramento Rochoso (Cinza).

### Tabela: Descritores de estrutura espacial das manchas de solo (Farina)

| Tipo de Solo | LSI | CF | RCC |
| :--- | :---: | :---: | :---: |
| Solo Raso | 5434.91 | 2.49 | 0.66 |
| Bem Desenvolvido | 2508.07 | 2.36 | 0.68 |
| Hidromórfico | 3272.30 | 2.27 | 0.65 |
| Argila Expansiva | 1827.24 | 2.84 | 0.64 |
| B--Textural | 2766.09 | 3.36 | 0.66 |

**Nota:**
LSI = Índice de Forma da Paisagem (_Landscape Shape Index_); CF = Complexidade da Forma; RCC = Coeficiente de Circularidade Relativa.

### Descritores de forma das manchas

A estrutura espacial das manchas de solo foi caracterizada por descritores clássicos da Ecologia da Paisagem, conforme proposto por Farina (1998, 2006).

**Índice de Forma da Paisagem (LSI)**
O LSI expressa a complexidade geométrica das manchas a partir da relação entre perímetro e área:
$$ LSI = \frac{P}{2\sqrt{\pi A}} $$
Valores próximos de 1 indicam formas simples.

**Complexidade da Forma (CF)**
Representa o grau de irregularidade geométrica:
$$ CF = \frac{P}{A} $$
Valores elevados indicam formas alongadas ou dendríticas.

**Coeficiente de Circularidade Relativa (RCC)**
Avalia a proximidade com um círculo perfeito:
$$ RCC = \frac{4\pi A}{P^{2}} $$
Varia entre 0 e 1 (1 = círculo).

# 11. Modelo Integrado Ecofuncional da Paisagem (v4.0)
Com a atualização v4.0 (Dezembro/2025), o SisterApp transcende a simulação isolada de vegetação para incorporar um Modelo Integrado de Paisagem (Integrated Landscape Model - ILM), atendendo aos requisitos de acoplamento ecofuncional definidos na Documentação de Domínio (DDD).

## 11.1. Definição do Domínio
O domínio é definido como a modelagem integrada da paisagem campestre como um sistema ecofuncional complexo, espacialmente acoplado e temporalmente explícito. Nenhuma propriedade sistêmica é modelada diretamente; todas emergem da interação entre estados, processos e regimes de distúrbio.

## 11.2. Arquitetura e Agregado Central (LandscapeCell)
A **LandscapeCell** representa a unidade lógica mínima da paisagem.
1.  **Identidade:** CellID único na grade.
2.  **Responsabilidades:** Representar um ponto espacial, referenciar estados biológicos/físicos e servir de base para métricas emergentes.

## 11.3. Estados de Domínio
O estado de cada célula é composto por vetores independentes que interagem via serviços:

### 11.3.1. SoilState (Fundação Edáfica)
*   **SoilDepth ($d_{soil}$):** Variável crítica sujeita a erosão. Solos $< 0.2m$ limitam o vigor.
*   **PropaguleBank ($\Pi$):** Memória ecológica que define a resiliência ($\tau_{rec}$).
*   **InfiltrationCapacity:** Modulada pela matéria orgânica e compactação.
*   **SurfaceExposure:** Fração de solo descoberto, aumentando risco de erosão.

### 11.3.2. VegetationState (Biomassa)
*   **Coverage ($C_{EI}, C_{ES}$):** Cobertura dos estratos Inferior e Superior.
*   **Vigor ($\phi_{EI}, \phi_{ES}$):** Estado fisiológico (0-1).
*   **StructuralHeterogeneity:** Variabilidade espacial local.

### 11.3.3. HydrologicalState (Fluxo Ativo)
*   **RunoffPotential:** Água disponível para escoamento superficial ($P - I_{infil}$).
*   **FlowAccumulation:** Volume acumulado via D8.
*   **ErosionRisk:** Potencial de destacamento de solo ($StreamPower \times (1 - Coverage)$).

## 11.4. Serviços de Domínio (Processos Ecofuncionais)
Os processos não pertencem à célula, mas operam sobre a grade como serviços:
*   **GrowthService:** Calcula crescimento logístico e competição inter-estratos.
*   **RunoffService:** Resolve o fluxo hidrológico D8 a cada tick, transportando água e sedimentos.
*   **ErosionService:** Remove $d_{soil}$ baseado no risco de erosão e deposita em bacias de sedimentação.
*   **DisturbanceService:** Aplica regimes de Fogo e Pastejo conforme magnitude e frequência.

## 11.5. Loop de Atualização e Tempo
O sistema opera em tempo discreto ($\Delta t$). O ciclo de atualização conceitual segue a ordem:
1.  Aplicação de Regimes de Distúrbio.
2.  Execução de Processos Físicos (Hidrologia, Erosão).
3.  Execução de Processos Biológicos (Crescimento, Mortalidade).
4.  Atualização de Estados.
5.  Derivação de Propriedades Emergentes (ex: Resiliência).

## 11.6. Propriedades Emergentes e Invariantes
**Resiliência Ecofuncional:** Não é uma variável, mas uma função da capacidade do sistema de recuperar $C_{veg}$ após $D$ (Distúrbio), dependendo de $\Pi$ (Banco de Propágulos) e $d_{soil}$.

**Invariantes do Sistema:**
*   Solo descoberto elevado $\implies$ Aumento não-linear de erosão.
*   A perda funcional (Vigor) precede o colapso estrutural (Cobertura).
*   Regeneração é impossível sem banco de propágulos (Estado de Desertificação).

## 11.7. Interface de Controle e Diagnóstico (v4.0)
Para suportar a validação deste modelo complexo, a interface foi expandida:
*   **Rain Intensity Slider:** Permite forçar tempestades extremas para testar $RunoffService$.
*   **Ecological Probe:** A ferramenta de inspeção agora revela estados internos ocultos ($d_{soil}$, $OM$, $E_{risk}$) para "debug" biológico.

## 11.8. Integração Machine Learning Genérica (v4.2.0)
O sistema incorpora um módulo avançado de Machine Learning (`MLService`) capaz de gerenciar múltiplos modelos preditivos simultaneamente.

### 11.8.1. Arquitetura Genérica
A classe `MLService` foi refatorada para abstrair a natureza dos inputs e outputs, permitindo o suporte a qualquer modelo Perceptron via um mapa associativo:

$$
\text{Models} : \{ \text{"soil\_color"} \to P_1, \text{"hydro\_runoff"} \to P_2, \dots \}
$$

### 11.8.2. Pipeline de Treinamento
O treinamento ocorre de forma assíncrona (background thread), utilizando vetores de entrada genéricos ($\mathbf{x} \in \mathbb{R}^n$) e alvos escalares ($y \in [0,1]$). Isso permite que novos modelos (ex: previsão de biomassa ou risco de fogo) sejam adicionados sem recompilação do núcleo da engine.

### 11.8.3. Arquitetura Perceptron (Eigen)
A implementação utiliza um Perceptron Multicamadas otimizado com a biblioteca Eigen para vetorização SIMD (AVX/SSE).

**Modelo Matemático:**

$$
y = \sigma(\mathbf{W} \cdot \mathbf{x} + b)
$$

Onde $\sigma(z) = \frac{1}{1 + e^{-z}}$ é a função de ativação Sigmoid. O modelo processa vetores de entrada normalizados das propriedades do solo:

$$
\mathbf{x} = [ \text{Depth}, \text{OM}, \text{Infiltration}, \text{Compaction} ] \quad (\text{para soil\_color})
$$

### 11.8.4. Modelo de Hidrologia ("hydro_runoff")
Este modelo estima a geração de escoamento superficial sem resolver a equação de fluxo completa, permitindo inferências rápidas para visualização ou heurísticas de IA.

**Inputs ($\mathbf{x}$):**
*   Intensidade da Chuva (Normalizada $0-100 \text{ mm/h}$)
*   Taxa de Infiltração Efetiva (Baseada no Tipo de Solo)
*   Biomassa Vegetal ($C_{EI} + C_{ES}$)

**Target ($y$):**
*   Coeficiente de Runoff ou Fluxo Absoluto ($\text{mm/h}$).

**Geração de Dados Estocástica:**
Para garantir generalização, o treinamento não utiliza apenas a chuva atual do mapa. O sistema sorteia cenários de chuva aleatórios ($R \sim U[0, 100]$) para cada célula amostrada, ensinando a rede a prever o comportamento hidrológico sob diversas condições climáticas.

### 11.8.5. Pipeline de Renderização Híbrida
Diferente de abordagens "Black Box" puras, o SisterApp utiliza o ML como um "Surrogate Model" para enriquecer a visualização:
1.  A simulação física calcula os estados brutos ($d_{soil}$, etc).
2.  O `MLService` infere uma "Assinatura Espectral" (Cor) baseada nesses estados.
3.  O renderizador mistura essa predição com a textura base, permitindo visualizar correlações não-lineares aprendidas de dados reais (quando o modelo for treinado).
