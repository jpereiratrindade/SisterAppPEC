# Sistema Brasileiro de Classificação de Solos (SiBCS) - Implementação

> **Versão:** 4.5.1
> **Contexto:** SisterApp PEC - Módulo de Solos
> **Base:** 5ª Edição do SiBCS (Embrapa)

## 1. Visão Geral

A implementação do SiBCS no SisterApp busca simular a classificação pedológica baseada em vetores de estado emergentes (Modelo SCORPAN: $S = f(P,R,C,O,A,N)$).
O sistema opera em dois níveis taxonômicos:

1.  **Ordem (1º Nível):** Baseada nas propriedades diagnósticas principais (Textura, Profundidade, Argila).
2.  **Subordem (2º Nível):** Baseada na mineralogia (Cor), saturação por água e química do horizonte.

---

## 2. Taxonomia Implementada

### 2.1 Latossolos (L)
Solos profundos, muito intemperizados, com alto conteúdo de sesquióxidos (Horizonte B Latossólico).
*   **Critério:** Profundidade > 1.0m, Baixa diferenciação textural, Alta Drenagem.
*   **Subordens:**
    *   **Latossolo Vermelho (LV):** Rico em Hematita ($Fe_2O_3$). Associado a rochas básicas ou sedimentos ricos em ferro. *Visual: Vermelho Escuro.*
    *   **Latossolo Amarelo (LA):** Rico em Goethita ($FeOOH$). Associado a ambientes mais úmidos ou rochas ácidas pobres em ferro. *Visual: Amarelo Ocre.*
    *   **Latossolo Vermelho-Amarelo (LVA):** Transição mineralógica. *Visual: Laranja.*

### 2.2 Argissolos (P)
Solos com incremento de argila em profundidade (Horizonte B Textural).
*   **Critério:** Aumento nítido de argila (>35% no B), Profundidade média.
*   **Subordens:**
    *   **Argissolo Vermelho/Amarelo:** Variação cromática herdada do material de origem. *Visual: Marrom Avermelhado.*

### 2.3 Cambissolos (C)
Solos em estágio incipiente de evolução (Horizonte B Incipiente).
*   **Critério:** Textura média, menor profundidade ou desenvolvimento pedogenético restrito.
*   **Subordens:**
    *   **Cambissolo Háplico:** Modal. *Visual: Marrom Claro.*
    *   **Cambissolo Litólico:** Transição para rocha. *Visual: Cinza Pardacento.*

### 2.4 Gleissolos (G)
Solos hidromórficos, saturados por água (Horizonte Gleique).
*   **Critério:** Lençol freático superficial, redução do ferro ($Fe^{3+} \to Fe^{2+}$).
*   **Subordens:**
    *   **Gleissolo Háplico:** Redução típica (Glei). *Visual: Cinza/Azulado.*
    *   **Gleissolo Melânico:** Horizonte A rico em Matéria Orgânica (Hístico/Chernossólico). *Visual: Preto/Azul Escuro.*

### 2.5 Neossolos (R)
Solos jovens, sem horizonte B diagnóstico.
*   **Subordens:**
    *   **Neossolo Litólico (RL):** Raso, contato lítico (Rocha) em < 50cm. *Visual: Cinza/Rochoso.*
    *   **Neossolo Quartzarênico (RQ):** Profundo, mas essencialmente areia (>90% Quartzo). *Visual: Branco/Bege Arenoso.*

---

## 3. Lógica de Classificação (Algoritmo)

O classificador (`TerrainGenerator::classifySoilFromSCORPAN`) opera pixel-a-pixel no grid de simulação:

```cpp
// Pseudocódigo da Lógica
if (Profundidade < 20cm) -> Neossolo Litólico
else if (Areia > 90%) -> Neossolo Quartzarênico
else if (Nível D'água Superficial ou M.O. > 15%) {
    Type = Gleissolo
    Sub = (M.O. > 30%) ? Melânico : Háplico
}
else if (Argila > 35% e B-Textural presente) {
    Type = Argissolo
    Sub = (Rico em Ferro) ? Vermelho : Vermelho-Amarelo
}
else {
    Type = Latossolo
    Sub = (Areia < 20%) ? Vermelho : (Areia > 40%) ? Vermelho-Amarelo : Amarelo
}
```

## 4. Representação Visual

A UI do sistema reflete esta classificação de duas formas:
1.  **Probe (Sonda):** Ao clicar, exibe a string completa (Ex: *"Latossolo Vermelho"*).
2.  **Mapa:** Utiliza uma paleta de cores calibrada no sistema Munsell para representar as Subordens (Ex: LV é 2.5YR 3/6, LA é 10YR 5/8).
