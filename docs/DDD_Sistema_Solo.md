# Domain-Driven Design: Sistema de Solos (Pedogênese Vetorial)

**Bounded Context:** `SoilSystem`
**Responsabilidade:** Implementar a lógica de formação e evolução do solo baseada nos vetores SCORPAN.
**Data:** 22 de Dezembro de 2025

---

## 1. Visão Geral
O Subsistema de Solos é o motor de *Pedogênese*. Ele não "desenha" o solo; ele *calcula* o estado físico e químico resultante da interação entre a **Litologia** (Vetor P) e o **Relevo** (Vetor R).

### Ubiquitous Language (Dialeto Pedológico)
*   **Pedogênese (Soil Genesis):** O processo de transformação da rocha em solo.
*   **Lithology Vector ($\mathbf{p}$):** O conjunto de coeficientes que descreve a rocha mãe (ex: Resistência ao intemperismo).
*   **Soil Horizon (Horizonte):** Camadas verticais do solo. (Simplificado atualmente para `Depth`).
*   **Texture Bias:** A tendência genética do material de origem em produzir areia ou argila.

---

## 2. Modelo de Domínio

### 2.1. Entidades (Entities)
O solo não é uma entidade isolada, mas uma **Propriedade Emergente** da `LandscapeCell`. No entanto, conceituamos o `SoilProfile` virtualmente.

#### `SoilProfile` (Componente da Célula)
Representa a coluna de solo em $(x,y)$.
*   Atributos de Estado ($\mathbf{s}$):
    *   `Depth` ($d$): Profundidade efetiva até a rocha.
    *   `OrganicMatter` ($OM$): % de carbono no solo.
    *   `Texture`: Classificação granulométrica (Areia/Silte/Argila).
    *   `Compaction`: Estado físico de densidade.

### 2.2. Serviços de Domínio (Domain Services)

#### `SoilSystem::initialize(Grid, Vectors)`
O "Big Bang" pedogenético.
*   **Input**:
    *   Vetor Geológico ($\mathbf{p}$): Fornecido pelo usuário ou mapa litológico.
    *   Vetor Topográfico ($\mathbf{r}$): Calculado do terreno (Slope).
*   **Lógica (Business Rules)**:
    1.  **Intemperismo**: $Intemperismo = f(Slope, p.WeatheringRate)$
        *   *Regra*: Declives acentuados removem solo mais rápido que ele forma (Erosão > Pedogênese) -> Solo Raso.
        *   *Regra*: Rochas resistentes (Granito) geram solos mais rasos que rochas frágeis (Basalto) no mesmo declive.
    2.  **Textura**: $Tipo = f(p.SandBias, p.ClayBias)$
        *   *Regra*: Granitos tendem a arenosos.
        *   *Regra*: Basaltos tendem a argilosos (Terras Roxas).

#### `SoilSystem::update(dt)`
A evolução temporal (ainda incipiente na v4.4).
*   Responsável pela recuperação da compactação e acúmulo lento de OM.

### 2.3. Value Objects (Os Vetores de Controle)

#### `LithologyDef` (O Genoma da Rocha)
Estrutura editável pelo usuário na UI.
```cpp
struct LithologyDef {
    float weathering_rate; // 0.0 (Diamante) a 1.0 (Calcário macio)
    float base_fertility;  // Potencial nutricional
    float sand_bias;       // Probabilidade de gerar grãos grossos
    float clay_bias;       // Probabilidade de gerar grãos finos
};
```

---

## 3. Integração com Outros Sistemas

### Soil $\leftrightarrow$ Hydro
*   **Serviço**: O Solo fornece a `InfiltrationRate` (baseada na Textura) para o Sistema Hidrológico.
*   **Feedback**: O Sistema Hidrológico erosiona o solo (reduzindo `Depth`).

### Soil $\leftrightarrow$ Vegetation
*   **Serviço**: O Solo fornece `Fertility` ($OM \times p.Fertility$) como capacidade de suporte ($K$).
*   **Feedback**: A Vegetação deposita biomassa morta, aumentando `OrganicMatter`.

---

## 4. Especificação de Teste (Validação)
Para considerar o sistema válido, ele deve passar no **Teste do Gradiente**:
1.  **Cenário**: Uma encosta uniforme vai de Flat a Steep.
2.  **Input**: Litologia uniforme.
3.  **Resultado Esperado**: O solo deve gradar suavemente de "Profundo/Maduro" no topo plano para "Raso/Jovem" na encosta íngreme. *Se houver degraus abruptos sem mudança de declive, o modelo falhou.*
