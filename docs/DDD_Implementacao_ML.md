# Domain-Driven Design: Implementação e Construção de ML (v4.2.1)

Este documento detalha a arquitetura técnica, os padrões de construção e o ciclo de vida de uso dos modelos de Machine Learning no SisterApp.

---

## 1. Visão de Implementação (Implementation Context)

O módulo de ML é implementado no namespace `ml` e reside em `src/ml/`. Ele é projetado como um sistema de **Inferência de Alto Desempenho** e **Treinamento On-Device**, permitindo que a aplicação evolua sem depender de frameworks externos pesados (como TensorFlow ou PyTorch), utilizando apenas **Eigen** para aceleração matemática.

### Ubiquitous Language de Implementação
*   **Perceptron:** A unidade mínima de processamento (Single-Layer Perceptron com ativação Sigmóide).
*   **Feature Vector ($\mathbf{x}$):** Vetor de entrada normalizado $[0,1]$ extraído dos grids físicos.
*   **Inference (Inferência):** Operação de leitura (Forward Pass) para obter uma predição em tempo de execução.
*   **Backpropagation:** Algoritmo de ajuste de pesos baseado no erro em relação ao *Ground Truth* físico.
*   **Model Registry:** O mapa interno do `MLService` que gerencia instâncias nomeadas de modelos.

---

## 2. Construção do Modelo: A Classe `Perceptron`

A construção segue o padrão de **Entidade Matemática Imutável na Estrutura, mas Mutável nos Pesos**.

### 2.1. Infraestrutura Matemática (Eigen)
Para garantir que a inferência possa ser feita em tempo real (centenas de vezes por frame no probe ou milhares no renderizador), utilizamos a biblioteca **Eigen**.
*   **Vetorização:** Os pesos (`weights_`) e inputs são processados via instruções SIMD (SSE/AVX).
*   **Otimização de Memória:** O `Perceptron` armazena pesos em um único `Eigen::VectorXf`, minimizando cache misses.

### 2.2. Lógica de Ativação
Implementamos a função Sigmóide de forma estática para garantir consistência:
$$ \sigma(z) = \frac{1}{1 + e^{-z}} $$
Esta escolha permite que todas as saídas do modelo estejam no intervalo $[0,1]$, o que é ideal para probabilidades (Fogo) ou intensidades normalizadas (Runoff/Cor).

---

## 3. Orquestração e Gerenciamento: `MLService`

O `MLService` atua como um **Domain Service** e um **Factory/Repository** (Mapa de Modelos).

### 3.1. Gerenciamento de Ciclo de Vida
O serviço gerencia um `std::map<std::string, std::unique_ptr<Perceptron>>`.
*   **LoadModel:** Carrega pesos de arquivos JSON (`assets/models/`). Se o arquivo não existir, o modelo é inicializado com pesos aleatórios.
*   **CollectTrainingSample:** Acumula dados em um `std::vector<GenericSample>` antes de disparar o treinamento.

### 3.2. Estratégia de Treinamento (Asynchronous Task)
O treinamento é implementado para ser não-bloqueante:
1.  O `Application` coleta dadosexpurgando a lógica física.
2.  O `MLService` executa o loop de `epochs` em uma thread separada (geralmente via `std::async`).
3.  O `Perceptron` aplica o ajuste: $\Delta W = \eta \cdot (target - output) \cdot \sigma'(z) \cdot x$.

---

## 4. Uso e Integração na Aplicação

O uso do modelo segue o padrão **Surrogate Model** (Modelo Substituto).

### 4.1. Na Sonda de Terreno (UI Probe)
A aplicação consome a predição através de *wrappers* tipados no `MLService`:
*   `predictRunoff(rain, infil, bio)`
*   `predictFireRisk(stratas, vigors)`
Este uso é síncrono, mas extremamente rápido devido à simplicidade da rede (uma camada).

### 4.2. Na Renderização (Hybrid Rendering)
O `TerrainRenderer` recebe um ponteiro para o `MLService`. Durante a geração da malha:
1.  O renderizador lê as propriedades do solo.
2.  O `MLService::predictSoilColor` é chamado.
3.  O resultado (RGB) é injetado diretamente no buffer de vértices (`v.color`).

---

## 5. Invariantes de Implementação (Regras de Ouro)

1.  **Normalização Obrigatória:** Todo input deve ser normalizado para $[0,1]$ antes do `infer()`. Inputs brutos (ex: Chuva > 100) quebram a convergência da rede.
2.  **Thread Safety (Inference):** O método `predict` é `const` e seguro para ser chamado de múltiplas threads (ex: paralelização de malha).
3.  **Persistência:** Todo modelo deve ser capaz de salvar e carregar seu estado em JSON para garantir persistência entre sessões de laboratório.

---

## 6. Desacoplamento e Testabilidade Externa

Para permitir o desenvolvimento e teste de modelos **fora da plataforma SisterApp**, o módulo de ML foi projetado com dependências mínimas.

### 6.1. Dependências Mínimas
Para compilar um driver de teste externo, você só precisa de:
*   `src/ml/perceptron.h` e `perceptron.cpp`
*   `src/ml/ml_service.h` e `ml_service.cpp`
*   **Biblioteca Eigen 3** (Header-only)
*   **nlohmann/json** (Para persistência, se aplicável)

### 6.2. Estratégia de Teste Standalone (C++)
Você pode criar um arquivo `test_external.cpp` que instancia o `MLService` e carrega um modelo exportado pelo SisterApp. Isso permite validar a inferência sem precisar subir a interface Vulkan ou o simulador de paisagem.

Exemplo de Driver de Teste:
```cpp
#include "ml_service.h"
int main() {
    ml::MLService service;
    service.loadModel("my_external_model", "path/to/weights.json", 4);
    float result = service.predict("my_external_model", Eigen::VectorXf::Constant(4, 0.5f));
    // Validar resultado...
}
```

### 6.3. Interoperabilidade via JSON (Python/Research)
Como os pesos são armazenados em JSON, é trivial carregar os parâmetros em um ambiente Python (como Jupyter Notebook) para análise estatística ou visualização de convergência externa:

```python
import json
import numpy as np

with open('soil_color.json', 'r') as f:
    data = json.load(f)
    weights = np.array(data['weights'])
    bias = data['bias']

# Simular inferência SisterApp em Python
def infer(x):
    z = np.dot(weights, x) + bias
    return 1.0 / (1.0 + np.exp(-z))
```

### 6.4. Benefícios do Desacoplamento
*   **Validação Cruzada:** Comparar a saída da Engine com implementações de referência em Python.
*   **Treinamento Externo:** É possível treinar modelos em datasets externos massivos e apenas "plugar" o JSON resultante no diretório `assets/models/` da Engine.
*   **Benchmarking:** Medir a performance de inferência (ops/sec) isoladamente.
