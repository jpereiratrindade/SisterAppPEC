# SiBCS — Estrutura Hierárquica Completa dos Níveis Categóricos
## Referência Estruturada para Uso por Sistemas de IA

Fonte conceitual: Sistema Brasileiro de Classificação de Solos (SiBCS – Embrapa Solos)

---

## Visão Geral

O Sistema Brasileiro de Classificação de Solos (SiBCS) é um sistema **hierárquico, multicategórico e aberto**, estruturado em **seis níveis taxonômicos**, que organizam os solos a partir de **processos pedogenéticos dominantes**, **horizontes diagnósticos** e **atributos físico-químicos e morfológicos**.

A hierarquia vai do **mais geral (Ordem)** ao **mais específico (Série)**.

---

## NÍVEL 1 — ORDEM

### Definição
Classe taxonômica de mais alto nível. Representa o **tipo pedogenético dominante** do solo.

### Critério central
- Presença de **horizontes diagnósticos principais**
- Processos dominantes de formação do solo

### Exemplos de Ordens
- Latossolos
- Gleissolos
- Argissolos
- Cambissolos
- Neossolos
- Planossolos
- Plintossolos
- Nitossolos
- Organossolos
- Vertissolos
- Espodossolos
- Chernossolos
- Luvissolos

### Uso recomendado para IA
- Identificação de **estado pedogenético global**
- Macrozonas edáficas
- Classes-raiz em ontologias
- Agregados principais em DDD

---

## NÍVEL 2 — SUBORDEM

### Definição
Subdivisão da Ordem com base em **características adicionais dominantes**, geralmente ligadas a:
- Cor
- Regime hídrico
- Material orgânico
- Expressão do processo pedogenético

### Exemplos
- Latossolo Vermelho
- Latossolo Amarelo
- Gleissolo Háplico
- Gleissolo Tiomórfico

### Uso recomendado para IA
- Refinamento semântico do processo dominante
- Primeira especialização ontológica
- Estados intermediários em modelos ecológicos

---

## NÍVEL 3 — GRANDE GRUPO

### Definição
Classe definida por **atributos químicos, mineralógicos ou texturais chave**, geralmente mensuráveis.

### Critérios comuns
- Saturação por bases (eutrófico / distrófico)
- Atividade da argila (Ta / Tb)
- Presença de certos caracteres diagnósticos

### Exemplos
- Latossolo Vermelho distrófico
- Latossolo Vermelho eutrófico
- Gleissolo Háplico Tb distrófico

### Uso recomendado para IA
- Parâmetros quantitativos
- Classes funcionais
- Pontos de decisão em modelos ML
- Features categóricas estáveis

---

## NÍVEL 4 — SUBGRUPO

### Definição
Refinamento morfológico e funcional do Grande Grupo.  
Introduz **qualificadores diagnósticos adicionais**.

### Características
- Pode acumular até **3 qualificadores**
- Representa variações internas relevantes ao manejo e uso

### Exemplos
- Latossolo Vermelho distrófico típico
- Latossolo Vermelho distrófico húmico
- Gleissolo Háplico Tb distrófico típico

### Uso recomendado para IA
- Alta resolução semântica
- Classificação supervisionada
- Estados finais de classificação padrão
- Outputs interpretáveis

---

## NÍVEL 5 — FAMÍLIA

### Definição
Classe definida por **atributos físico-químicos e mineralógicos específicos**, com forte relação funcional.

### Exemplos de critérios
- Classe textural
- Classe de profundidade
- Mineralogia da fração argila
- Reação do solo (pH)
- Classe de drenagem

### Status no SiBCS
- Critérios **propostos e recomendados**
- Ainda em consolidação científica

### Uso recomendado para IA
- Modelos agronômicos
- Simulações físico-químicas
- Regras de manejo
- Parametrização fina de processos

---

## NÍVEL 6 — SÉRIE

### Definição
Unidade mais específica do sistema.  
Representa um **solo individualizado em contexto local**, geralmente associado a:
- Localidade
- Material de origem
- Paisagem específica

### Status no SiBCS
- **Provisório**
- Pouco utilizado operacionalmente no Brasil

### Uso recomendado para IA
- NÃO recomendado como classe geral
- Uso apenas em bancos locais ou estudos de caso
- Alto risco de overfitting sem contexto espacial

---

## RESUMO PARA IA (TL;DR)

| Nível | Nome        | Escala Conceitual | Uso Ideal em IA |
|-----|-------------|------------------|----------------|
| 1 | Ordem | Processo pedogenético global | Classe-raiz |
| 2 | Subordem | Variação dominante | Especialização |
| 3 | Grande Grupo | Atributos mensuráveis | Features |
| 4 | Subgrupo | Morfologia funcional | Classe final |
| 5 | Família | Propriedades físico-químicas | Parametrização |
| 6 | Série | Contexto local | Uso restrito |

---

## OBSERVAÇÃO CRÍTICA (importante!)

> O SiBCS **não é apenas uma taxonomia**, mas um **modelo implícito de processos**.
Qualquer IA que o utilize corretamente deve:
- Respeitar a hierarquia
- Não misturar níveis
- Evitar inferência direta de propriedades de níveis inferiores sem dados

---

---

## PADRÃO DE CORES DAS CLASSES DO SiBCS
## Referência Visual Normativa para IA, SIG e Visualização

Fonte: Anexo H — Padronização das cores das classes de 1º e 2º níveis categóricos do SiBCS (Embrapa Solos).

---

### Objetivo do Padrão de Cores

O padrão cromático do SiBCS tem como função:
- Garantir **consistência visual em mapas de solos**
- Facilitar **reconhecimento rápido das Ordens**
- Evitar ambiguidade em representações cartográficas e computacionais

⚠️ Importante:  
As cores **não representam propriedades químicas diretas**, mas **classes taxonômicas**.

---

## CORES POR ORDEM (1º NÍVEL CATEGÓRICO)

| Ordem | Cor Padrão | HEX Aproximado | Observação Semântica |
|------|-----------|---------------|---------------------|
| Latossolos | Vermelho intenso | `#C00000` | Intemperismo avançado |
| Argissolos | Vermelho-amarelado | `#E67E22` | Textural Bt |
| Nitossolos | Vermelho escuro | `#8B0000` | Estrutura nítica |
| Cambissolos | Marrom | `#8E6E53` | Desenvolvimento incipiente |
| Neossolos | Cinza claro | `#BDBDBD` | Pouco desenvolvidos |
| Gleissolos | Azul acinzentado | `#6FA8DC` | Hidromorfismo |
| Planossolos | Verde oliva | `#7CB342` | Horizonte plânico |
| Plintossolos | Roxo | `#8E44AD` | Plintita / Fe |
| Vertissolos | Preto | `#2C2C2C` | Argilas expansivas |
| Organossolos | Preto profundo | `#000000` | Material orgânico |
| Espodossolos | Cinza arroxeado | `#9B8FBF` | Horizonte espódico |
| Chernossolos | Marrom escuro | `#5D4037` | A chernozêmico |
| Luvissolos | Amarelo escuro | `#F1C40F` | Alta saturação por bases |

---

## CORES PARA SUBORDENS (2º NÍVEL)

### Regra Geral
- A **Subordem herda a cor da Ordem**
- Aplica-se:
  - Variação de **saturação**
  - Variação de **luminosidade**
  - Nunca mudança completa de matiz

### Nota de Implementação (SisterAppPEC)
Para evitar ambiguidades visuais no motor de simulação:
- Subordens cromáticas como `Vermelho/Amarelo/Vermelho-Amarelo` são aplicadas **de forma dependente da Ordem** (ex.: em Latossolos).
- Em Argissolos, a visualização mantém a **família cromática vermelho-amarelada/marrom** para não confundir *"Argissolo Vermelho"* com *"Latossolo Vermelho"*.

### Exemplos
- Latossolo Vermelho → Vermelho puro  
- Latossolo Amarelo → Vermelho com matiz deslocado ao amarelo  
- Gleissolo Tiomórfico → Azul acinzentado mais escuro  

---

## REGRAS DE USO PARA IA E SISTEMAS COMPUTACIONAIS

### 1. Proibição de Inferência Indevida
IA **não deve inferir propriedades do solo apenas pela cor**.

### 2. Hierarquia Visual Obrigatória
- Ordem → define matiz
- Subordem → define tonalidade
- Demais níveis → NÃO alteram cor-base

### 3. Uso Recomendado em Modelos
- SIG e mapas temáticos
- Visualização 2D/3D
- Engines gráficas
- Debug visual de classificação automática

---

## USO EM ONTOLOGIAS E MODELOS DE DADOS

Recomendação de atributo:

```yaml
soil_class:
  order: Latossolo
  color:
    name: Vermelho
    hex: "#C00000"
    source: SiBCS_Anexo_H


## FIM DO DOCUMENTO
