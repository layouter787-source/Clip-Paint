# Clip Paint — plano de implementação nativa

Branch: `feature/procreate-complete-native-engine`

## Decisão de arquitetura
- Meta de implementação: aproximadamente 80% C++20 no motor, processamento de imagem, timeline, composição, efeitos e formatos. Kotlin fica como camada Android: ciclo de vida, acessibilidade, navegação, permissões, dialogs e ligação JNI.
- Preservar o motor atual: C++20, OpenGL ES 3, JNI, tiles raster 64×64, copy-on-write e histórico por snapshots.
- Implementar funcionalidades equivalentes por engenharia independente. Não copiar código, assets proprietários ou identidade visual do Procreate.

## Fases

1. **Fundação de animação (em integração)** — modelo C++20 de timeline, faixas por camada, cels raster por frame, criação/duplicação independente, remoção, busca, FPS e reprodução. Os tiles são guardados por copy-on-write e os dados da animação são persistidos num sidecar `.anim` junto do `.cpd`. Os comandos já estão expostos por JNI e existe um painel Android com controlo de frames e faixas. Ainda faltam: Onion Skin desenhado no canvas, reprodução baseada em exposição, avaliação visual dos keyframes/interpolação, exportação GIF/MP4 e testes de round-trip.
2. **Pintura profissional** — motor de traço, pressão/tilt, curvas de estabilização, taper, Brush Studio, texturas/grão, wet-mix e presets editáveis.
3. **Camadas e composição** — grupos, máscaras raster/vetoriais, clipping, alpha lock, blend modes adicionais, seleção múltipla e composição acelerada.
4. **Transformação e efeitos** — warp/perspectiva, liquify, guias/simetria, QuickShape, ajustes não destrutivos e filtros.
5. **Animação completa** — cels ligados/independentes, exposição, timeline multi-track, keyframes, interpolação, onion skin, playback, áudio/referência se suportado, exportação GIF/MP4/PNG sequence.
6. **Documentos e exportação** — autosave, recuperação, perfis de cor, importação/exportação interoperável e thumbnails.
7. **Polimento e qualidade** — testes, profiling, memória, compatibilidade com stylus e dispositivos Android, CI e artefactos APK.

## Critérios de conclusão
Cada fase deve ter integração real no editor, persistência, testes e build Android verde. Um modelo isolado ou um botão visual não conta como funcionalidade concluída.

## Estado honesto
A integração inicial já associa cels raster reais às camadas do documento, permite mudar de frame, duplicar cels de forma independente, reproduzir a sequência e guardar/carregar a animação pelo sidecar `.anim`. O painel Android está ligado à ponte JNI. Isto é uma base funcional de animação frame-by-frame, não uma réplica completa do Procreate Dreams: Onion Skin visual, exposição temporal, interpolação de keyframes aplicada aos objetos, áudio e exportação de animação ainda não estão concluídos. A validação Android da versão mais recente continua pendente até o GitHub Actions terminar com sucesso.


## Integração contínua
O workflow Android CI também é acionado por pushes em `feature/**`, para validar as fases antes de qualquer merge para `main`.
