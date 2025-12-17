# [LEGACY REPORT] Engineering Review – Voxel Terrain (v3.3.0)
> **NOTE:** This document refers to the legacy Voxel Terrain system which was removed in v3.8.4. It is preserved for historical engineering reference regarding Vulkan synchronization strategies.

# Engineering Review – Voxel Terrain / Render Loop

## Problemas identificados
- **Stall/Crash com VSync off:** `pruneFarChunks` fazia `vkDeviceWaitIdle` no thread de render a cada remoção de chunk. Com VSync desligado e bursts de regeneração (vegetação), isso bloqueava o driver e podia resetar o compositor/GPU.
- **Regeneração de vegetação no thread principal:** `processPendingVegetation` executava `refreshVegetation` (loops 3D grandes sob `meshMutex_`) no render loop. Sem VSync, um toggle de vegetação disparava trabalho pesado e excedia o orçamento de frame.
- **Reset sem quiescer workers:** `reset()` limpava filas/chunks enquanto tarefas em andamento ainda rodavam, abrindo janela para uso de dados invalidados.
- **Destruição de recursos sem sincronização fina:** Descarte de meshes dependia de idle global; faltava amarração a fences de GPU.
- **Persistência do crash com VSync off + vegetação off:** Mesmo com GC por fence, o app ainda cai ao desativar VSync e vegetação, indicando que ainda há recursos/command buffers sendo usados após remoção ou bursts de trabalho que saturam o driver (hipótese: destruição atrelada a “todas as fences” mantém lixo demais e depois libera em bloco, ou ainda há tarefas antigas tocando chunks já removidos).

## Correções aplicadas
- **GC de GPU sem bloquear:** Adicionada integração de fences (`setFrameFences`) em `VoxelTerrain` e coleta diferida agora consulta `vkGetFenceStatus` para só liberar recursos quando seguro, sem `vkDeviceWaitIdle` no caminho quente.
- **GC por frame:** `gcFrameResources` roda no update e só libera recursos diferidos quando as fences sinalizam (removido fallback por tempo para evitar liberar recursos ainda em uso).
- **Vegetação assíncrona:** Regeneração de vegetação agora vai para o pool de workers (`Task::Type::Vegetation`), respeitando orçamento (`vegRegenPerFrame_`) e limite de fila (`maxPendingTasks_`). O render loop apenas agenda.
- **Reset seguro:** `reset()` espera os workers ficarem ociosos (`waitForWorkersIdle`) antes de idlear a GPU e limpar chunks/filas, evitando races com tarefas em andamento.
- **Hook de fences no app:** `Application` passa os fences de in-flight para o terreno logo após a criação, permitindo coleta segura de recursos.
- **Cap de FPS com VSync off:** Adicionado limite configurável (`vsyncOffFpsCap_`) para evitar runaway de frames quando o VSync está desligado.
- **Hotfix (queda ao desativar VSync+vegetação):** Removido fallback de tempo que ainda podia liberar recursos sem fence sinalizada; GC agora depende exclusivamente de fences, eliminando UAF quando frames em voo atrasam.

## Próximos passos sugeridos
- **Fences por recurso:** Amarrar cada pacote de destruição a uma fence específica (por frame) para liberar de forma mais granular.
- **Uploads em DEVICE_LOCAL:** Migrar meshes para staging + memória de dispositivo e reduzir o lock global (`meshMutex_`) para por-chunk.
- **Orçamentos monitorados:** Logar tempo por task e número de pending tasks por frame para detectar regressões sob stress (VSync off, toggles de vegetação).
- **Isolamento do crash atual:** Instrumentar: (1) logar fences usadas ao marcar chunks para destruição, (2) contar chunks e meshes em `deferredDestroy_` por frame, (3) alternar VSync/vegetação sob stress. Ajustar GC para liberar por fence de frame em vez de “todas as fences”, evitando acumular e liberar em bloco. **Solução proposta:** associar cada chunk removido à fence do frame corrente, mover o par {chunks,fence} para uma fila e liberar apenas quando essa fence sinalizar; isso elimina a dependência de todas as fences e reduz bursts de destruição.

## Histórico
- v3.3.0-base: stalls e travamentos ao alternar vegetação com VSync off por `vkDeviceWaitIdle` em loop.
- v3.3.0-fix: coleta diferida baseada em fences, regeneração de vegetação assíncrona, reset mais seguro. Caminho quente do render não bloqueia mais a GPU para descarte.
- v3.3.0-fix2: GC dependente apenas de fences (sem fallback temporal) e cap de FPS para VSync off, mas crash persiste ao desativar VSync+vegetação (investigação pendente).
- **v3.3.0 (Final)**: Implementada destruição diferida segura. Recursos são movidos para uma fila de "lixo" associada à fence do frame atual. O GC varre a lista completa a cada frame, liberando apenas o que tem fence sinalizada. Isso eliminou completamente os resets de GPU por Use-After-Free.

## Resolução Final (v3.3.0)
O problema fundamental era a destruição imediata (ou falsamente segura) de recursos ainda em uso pela GPU durante o overlap de frames (double/triple buffering). A solução definitiva foi:
1. **Fila de Destruição Não-FIFO**: `std::vector` varrido integralmente, não parando no primeiro item ocupado.
2. **Associação de Fence**: Cada mesh removido é pareado com a fence do frame onde ocorreu a remoção.
3. **Scan Agressivo**: `gcFrameResources` roda todo frame antes de updates lógicos.

Resultado: Crash eliminado. Estabilidade confirmada com VSync Off e stress test de vegetação.
