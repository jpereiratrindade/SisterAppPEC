# Domain-Driven Design: Machine Learning Ecofuncional (v4.2.1)

**Bounded Context:** `EcofunctionalMachineLearning`
**Subdomínio:** Suporte / Genérico (Surrogate Modeling)
**Autor:** Antigravity (Google Deepmind)
**Data:** 19 de Dezembro de 2025

---

## 1. Visão Geral e Ubiquitous Language

Este Bounded Context é responsável por criar, treinar e inferir modelos de Machine Learning que atuam como *Surrogates* (Substitutos) ou *Enhancers* (Amplificadores) para a simulação física principal.

### Linguagem Ubíqua (Ubiquitous Language)
*   **Surrogate Model (Modelo Substituto):** Um modelo ML treinado para aproximar uma função complexa ou computacionalmente custosa da simulação física (ex: Radiative Transfer Model para cor do solo).
*   **Physics-Guided Sampling (Amostragem Guiada por Física):** Processo de extração de dados onde o "Ground Truth" não vem de anotação humana, mas das leis físicas determinísticas do `EcofunctionalLandscape`.
*   **Training Session (Sessão de Treinamento):** Um evento temporal delimitado onde o modelo ajusta seus parâmetros internos.
*   **Spectral Signature (Assinatura Espectral):** O valor de cor (RGB/Reflectância) resultante da inferência, representando a interação da luz com as propriedades físico-químicas do solo.
*   **Training Report (Relatório de Treinamento):** Um artefato gerado ao fim de uma sessão, garantindo **Transparência** e **Rastreabilidade** do processo.

---

## 2. Mapa de Contexto (Context Map)

O contexto de ML se relaciona com o contexto principal (`EcofunctionalLandscape`) através de um padrão **Customer/Supplier** ou **Conformist** (dependendo da implementação).

*   **Upstream (`EcofunctionalLandscape`):** Fornece o "Mundo Físico" e as regras para gerar dados (Sampling).
*   **Downstream (`EcofunctionalMachineLearning`):** Consome estados físicos ($d_{soil}, OM$) e fornece predições visuais (Cor).

---

## 3. Arquitetura de Domínio

### 3.1. Entidades (Entities)

#### `Model` (Aggregate Root)
Representa a rede neural ou algoritmo estatístico. Possui identidade (ex: "SoilColorPredictor") e ciclo de vida.
*   **Identificadores Registrados:**
    *   `soil_color`: Predição de reflectância visual.
    *   `hydro_runoff`: Predição de escoamento superficial.
    *   `fire_risk`: Predição de probabilidade de ignição.
    *   `biomass_growth`: Predição de dinâmica de recuperação vegetal.
*   **Atributos:**
    *   `Weights`: Matrizes de pesos sinápticos (Mutável).
    *   `Architecture`: Definição de camadas (Imutável após criação).
    *   `TrainingCount`: Contador de gerações/épocas treinadas.

#### `TrainingSession`
Representa uma execução única de otimização.
*   **Atributos:**
    *   `SessionID`: UUID.
    *   `StartTime`, `EndTime`.
    *   `Hyperparameters`: Taxa de aprendizado, Batch size.

### 3.2. Objetos de Valor (Value Objects)

#### `TrainingSample`
Um ponto de dados imutável extraído da simulação.
*   **Estrutura:** $\{ \mathbf{x}: [f_1, f_2, \dots, f_n], y: [Target] \}$
*   **Domínios de Amostragem:**
    *   Soil: $[d, OM, Inf, Comp] \to Color$
    *   Hydro: $[Rain, EffInf, Bio] \to Runoff$
    *   Fire: $[C_{EI}, C_{ES}, \phi_{EI}, \phi_{ES}] \to P_{fire}$
    *   Growth: $[C_{total}, K, \phi] \to \Delta C$
*   **Propriedade de Transparência:** Deve conter metadados de origem (coordenada $(x,z)$, ID da Bacia) para rastreabilidade.

#### `TrainingMetrics`
Métricas de qualidade do modelo.
*   **Atributos:** `InitialLoss`, `FinalLoss`, `ConvergenceRate`.

#### `TrainingReport` (Solicitação do Cliente)
Um documento consolidado detalhando a "saúde" e o resultado do treinamento.
*   **Conteúdo Obrigatório:**
    *   Origem dos Dados (ex: "Amostragem Aleatória Uniforme em 1024x1024").
    *   Estatísticas do Dataset (Média/Desvio Padrão das features de entrada).
    *   Curva de Convergência (Delta Loss).
    *   Validação de Invariantes (ex: "Cor não deve ser negativa").

### 3.3. Serviços de Domínio (Domain Services)

#### `SamplingService`
Responsável pela **Transparência na Coleta**. Não apenas "pega dados", mas garante que a amostra seja representativa.
*   **Método:** `collectRepresentativeSamples(TerrainMap, Strategy)`
*   **Regras:**
    *   Deve evitar viés (ex: amostrar só áreas planas).
    *   Deve registrar a distribuição das amostras geradas.

#### `TrainingOrchestrator`
Gerencia o ciclo de vida da `TrainingSession` de forma assíncrona (conforme implementado na v4.0.0).
*   **Responsabilidade:**
    1.  Instanciar `TrainingSession`.
    2.  Executar loop de otimização.
    3.  Emitir `TrainingReport` ao final.

---

## 4. Especificação Funcional (Feature Spec)

### F1: Amostragem Transparente
**Problema:** Usuário vê "Collecting Data" mas não sabe se os dados são bons.
**Solução:**
*   UI deve mostrar histograma ou estatísticas básicas ("Avg Depth: 1.2m").
*   Console/Log deve detalhar a cobertura espacial da amostragem.

### F2: Relatório Pós-Treinamento
**Problema:** Treinamento termina e o usuário não sabe se o modelo aprendeu.
**Solução:**
*   Gerar arquivo `training_report_YYYYMMDD.txt` ou popup na UI.
*   Exibir: "Loss caiu de 0.5 para 0.01 em 100 épocas (Melhoria de 98%)".
