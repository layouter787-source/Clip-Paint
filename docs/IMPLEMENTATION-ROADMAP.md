# Clip Paint — plano de implementação nativa

Branch: `feature/procreate-complete-native-engine`

## Decisão de arquitetura
- Meta de implementação: aproximadamente 80% C++20 no motor, processamento de imagem, timeline, composição, efeitos e formatos. Kotlin fica como camada Android: ciclo de vida, acessibilidade, navegação, permissões, dialogs e ligação JNI.
- Preservar o motor atual: C++20, OpenGL ES 3, JNI, tiles raster 64×64, copy-on-write e histórico por snapshots.
- Implementar funcionalidades equivalentes por engenharia independente. Não copiar código, assets proprietários ou identidade visual do Procreate.

## Fases

1. **Fundação de animação (iniciada nesta branch)** — modelo C++ de timeline, tracks, frames/cels, exposição, keyframes, FPS, reprodução e parâmetros de onion skin. Próximos passos: persistência no formato de projeto, integração com Document/Layer, JNI e UI de timeline.
2. **Pintura profissional** — motor de traço, pressão/tilt, curvas de estabilização, taper, Brush Studio, texturas/grão, wet-mix e presets editáveis.
3. **Camadas e composição** — grupos, máscaras raster/vetoriais, clipping, alpha lock, blend modes adicionais, seleção múltipla e composição acelerada.
4. **Transformação e efeitos** — warp/perspectiva, liquify, guias/simetria, QuickShape, ajustes não destrutivos e filtros.
5. **Animação completa** — cels ligados/independentes, exposição, timeline multi-track, keyframes, interpolação, onion skin, playback, áudio/referência se suportado, exportação GIF/MP4/PNG sequence.
6. **Documentos e exportação** — autosave, recuperação, perfis de cor, importação/exportação interoperável e thumbnails.
7. **Polimento e qualidade** — testes, profiling, memória, compatibilidade com stylus e dispositivos Android, CI e artefactos APK.

## Critérios de conclusão
Cada fase deve ter integração real no editor, persistência, testes e build Android verde. Um modelo isolado ou um botão visual não conta como funcionalidade concluída.

## Estado honesto
Esta primeira entrega adiciona o núcleo de dados da timeline em C++; ainda não expõe a timeline na interface nem associa cels às camadas raster atuais. Não anunciar a animação como utilizável até concluir essas integrações.


## Integração contínua
O workflow Android CI também é acionado por pushes em `feature/**`, para validar as fases antes de qualquer merge para `main`.
