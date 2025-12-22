# DDD — Padrões de Manchas de Solo
## Domínio de Validação da Integridade Espacial da Paisagem

---

## 0. Declaração de Soberania Ontológica

Este domínio é **soberano** na definição e validação da integridade espacial dos padrões de manchas de solo.

Nenhum resultado de simulação, cenário experimental ou visualização gerada pelo sistema pode ser considerado **ecologicamente interpretável** sem validação explícita por este domínio.

Este domínio **não gera paisagens**, **não otimiza padrões** e **não prevê estados futuros**.  
Ele **delimita o espaço do possível ecológico** a partir de critérios estruturais.

---

## 1. Propósito do Domínio

Este domínio descreve, valida e monitora situações simuláveis capazes de:

- preservar,
- tensionar,
- ou transformar de forma controlada

os **padrões espaciais de manchas de solo** observados na paisagem.

Seu objetivo é garantir que toda simulação mantenha coerência com:

- processos pedogenéticos,
- condicionantes geomorfológicos,
- regimes hidrológicos,
- e pressões antrópicas declaradas,

respeitando **envelopes estruturais** de forma, complexidade e conectividade espacial.

---

## 2. Delimitação Explícita do Não-Domínio

Este domínio **não modela**, **não infere** e **não decide** sobre:

- dinâmica de comunidades vegetais,
- composição florística ou faunística,
- produtividade primária,
- fisiologia vegetal,
- competição interespecífica,
- dinâmica populacional,
- processos bioquímicos do solo.

Esses aspectos pertencem a outros domínios e **podem consumir eventos gerados aqui**, mas **não são definidos aqui**.

---

## 3. Linguagem Ubíqua

| Termo | Definição |
|------|----------|
| Patch | Unidade espacial contínua de um mesmo tipo de solo |
| Patch Pattern | Configuração espacial resultante do conjunto de patches |
| Pattern Stability | Capacidade do padrão se manter dentro de envelopes estruturais |
| Pattern Drift | Desvio progressivo dos descritores espaciais |
| Process Driver | Processo dominante que estrutura o padrão |
| Scenario | Conjunto de condições ambientais e/ou de manejo |
| Constraint | Regra ecológica que limita variações estruturais |
| Transition Zone | Estado intermediário entre estabilidade e ruptura |

---

## 4. Aggregate Root — LandscapeScenario

O `LandscapeScenario` é o **Aggregate Root** deste domínio.

Ele orquestra a coerência entre:
- tipo de solo,
- processos dominantes,
- regime hidrológico,
- pressão de uso,
- e assinatura espacial observada.

Entidades associadas:
- SoilPatch  
- SoilType  
- ProcessDriver  
- HydrologicalRegime  
- LandUsePressure  

Nenhuma entidade pode ser avaliada isoladamente fora de um `LandscapeScenario`.

---

## 5. Entidade — SoilType

`SoilType` representa uma **classificação pedológica estável** no horizonte temporal da simulação.

Ele não possui ciclo de vida, não evolui dinamicamente e não se transforma durante a execução.

Tipos reconhecidos:
- Solo Raso  
- Solo Bem Desenvolvido  
- Solo Hidromórfico  
- Argila Expansiva  
- Solo B–Textural  

Cada tipo possui envelopes aceitáveis para:
- LSI  
- CF  
- RCC  
- Fragmentação  
- Conectividade funcional  

---

## 6. Value Object — PatchPatternSignature

Define a **assinatura espacial esperada** para um tipo de solo.

Componentes:
- Intervalo de LSI  
- Intervalo de CF  
- Intervalo de RCC  
- Nível de fragmentação  
- Grau mínimo de conectividade  

Imutável, sem identidade própria, usado exclusivamente para validação.

---

## 7. Estados Válidos do Padrão Espacial

Todo cenário deve estar exatamente em um dos estados:

1. Stable  
2. UnderTension  
3. InTransition  
4. Incompatible  

Estados intermediários não são permitidos.

---

## 8. Domain Service — PatternIntegrityValidator

Responsável por:
- comparar métricas observadas com a assinatura,
- classificar o estado do padrão,
- emitir eventos de domínio.

As métricas são calculadas externamente.

---

## 9. Eventos de Domínio

- PatternDriftDetected  
- HydrologicalThresholdCrossed  
- PedogeneticStabilityLost  
- TransitionZoneActivated  
- PatternIncompatibilityConfirmed  

Eventos possuem significado ecológico explícito.

---

## 10. Regras Mestras do Domínio

1. Padrões incompatíveis não podem ocorrer sem sinalização.
2. Mudança estrutural não implica degradação.
3. Estabilidade espacial não equivale a qualidade ecológica.
4. Toda ruptura deve ser rastreável.

---

## 11. Tradução Obrigatória para a Interface

Toda validação, estado ou evento deve:
- ser sinalizado ao usuário,
- ser registrado,
- permitir análise temporal.

Visualização não substitui validação semântica.

---

## 12. Integração com o SisterApp3D

- CPU: cálculo de métricas  
- Domínio: validação e interpretação  
- GPU: renderização  

---

## 13. Regra Final

Este domínio não diz o que a paisagem será.  
Ele diz o que ela **não pode deixar de ser** sem mudar de regime.
