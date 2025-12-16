# DDD — Simulação de Situações com Manutenção de Padrões de Manchas de Solo

## 1. Propósito do Domínio

Este domínio descreve situações simuláveis capazes de preservar, tensionar ou transformar,
de forma controlada, os padrões espaciais de manchas de solo observados na paisagem,
mantendo coerência com processos pedogenéticos, geomorfológicos, hidrológicos e antrópicos.

O objetivo é garantir que toda simulação respeite os padrões estruturais (forma, complexidade
e circularidade) associados a cada tipo de solo.

---

## 2. Linguagem Ubíqua (Ubiquitous Language)

- **Patch**: unidade espacial contínua de um mesmo tipo de solo  
- **Patch Pattern**: configuração espacial resultante das manchas  
- **Pattern Stability**: capacidade do padrão se manter sob perturbações  
- **Process Driver**: processo dominante que governa o padrão espacial  
- **Scenario**: conjunto de condições ambientais e/ou de manejo  
- **Constraint**: regra ecológica que limita variações do padrão  

---

## 3. Aggregate Root — LandscapeScenario

Responsável por orquestrar cenários de simulação garantindo coerência entre:
- Processos dominantes
- Tipo de solo
- Assinatura espacial das manchas

Entidades associadas:
- SoilPatch  
- SoilType  
- ProcessDriver  
- HydrologicalRegime  
- LandUsePressure  

---

## 4. Entidade — SoilType

Tipos de solo contemplados:
- Solo Raso  
- Solo Bem Desenvolvido  
- Solo Hidromórfico  
- Argila Expansiva  
- Solo B–Textural  

Cada tipo de solo possui limites aceitáveis para os descritores espaciais:
- LSI (Landscape Shape Index)
- CF (Complexidade da Forma)
- RCC (Coeficiente de Circularidade Relativa)

---

## 5. Value Object — PatchPatternSignature

Define a assinatura espacial esperada para cada tipo de solo:

- Intervalo de LSI  
- Intervalo de CF  
- Intervalo de RCC  
- Nível esperado de fragmentação  

Funciona como guarda-corpo ecológico da simulação.

---

## 6. Situações Simuláveis por Tipo de Solo

### 6.1 Solo Raso — Domínio Geomorfológico

**Processos dominantes**
- Declividade elevada
- Baixa profundidade efetiva
- Alta exposição superficial

**Situações simuláveis**
- Intensificação de processos erosivos
- Alterações de cobertura vegetal
- Redução de biomassa sem alteração topográfica

**Restrições**
- LSI elevado deve ser mantido
- RCC não pode aumentar significativamente
- Manchas compactas não são permitidas

---

### 6.2 Solo Bem Desenvolvido — Domínio da Estabilidade

**Processos dominantes**
- Pedogênese madura
- Boa drenagem
- Baixa variabilidade topográfica

**Situações simuláveis**
- Mudanças graduais de uso do solo
- Variações de fertilidade
- Alterações moderadas de cobertura vegetal

**Restrições**
- CF baixo a médio
- RCC elevado
- Fragmentação limitada

---

### 6.3 Solo Hidromórfico — Domínio Hidrológico

**Processos dominantes**
- Lençol freático superficial
- Microtopografia
- Pulsos de saturação hídrica

**Situações simuláveis**
- Variação sazonal de chuvas
- Drenagem artificial parcial
- Compactação superficial

**Restrições**
- Fragmentação variável
- CF relativamente baixo
- Conectividade funcional mantida

---

### 6.4 Argila Expansiva — Domínio Físico-Químico

**Processos dominantes**
- Expansão e retração hídrica
- Fissuração
- Alta plasticidade

**Situações simuláveis**
- Ciclos intensos de seca e chuva
- Uso mecânico do solo
- Alterações de cobertura que afetam umidade

**Restrições**
- CF elevado
- RCC baixo
- Manchas alongadas persistentes

---

### 6.5 Solo B–Textural — Domínio de Transição

**Processos dominantes**
- Gradientes pedogenéticos
- Interação relevo–hidrologia
- Limiares ambientais

**Situações simuláveis**
- Mudanças graduais de uso do solo
- Alterações climáticas progressivas
- Reorganização da drenagem superficial

**Restrições**
- CF elevado
- LSI médio a alto
- Alta sensibilidade a perturbações

---

## 7. Domain Service — PatternIntegrityValidator

Responsável por validar, a cada iteração da simulação, se os padrões espaciais permanecem
coerentes com o tipo de solo.

Função:
- Comparar métricas atuais com a PatchPatternSignature
- Emitir alertas ou eventos de domínio em caso de violação

---

## 8. Eventos de Domínio

- PatternDriftDetected  
- HydrologicalThresholdCrossed  
- PedogeneticStabilityLost  
- TransitionZoneActivated  

Esses eventos alimentam relatórios ecológicos e análises de resiliência.

---

## 9. Regra Mestra do Domínio

A simulação não pode gerar padrões de manchas incompatíveis com os processos ecológicos
declarados. Caso o padrão se desvie excessivamente, o sistema deve sinalizar transição,
degradação ou perda de estabilidade.

---

## 10. Integração Conceitual com o SisterApp3D

- CPU calcula métricas espaciais
- GPU apenas renderiza
- A integridade do padrão define alertas, relatórios e mudanças semânticas

Este domínio transforma o simulador em um laboratório experimental de Ecologia da Paisagem.
