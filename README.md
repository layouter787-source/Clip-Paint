# Clip Paint — Kotlin + C++ (OpenGL ES 3)

App de desenho com **interface em Kotlin** (Views nativas do Android, sem bibliotecas externas) e **motor em C++**
(camadas em tiles, compositor na GPU, pincéis libmypaint/AnimBrush) ligados por JNI.

## Telas
- **Início** (`HomeActivity`): botão "Novo projeto" e grade de projetos recentes com miniatura (renomear, duplicar, excluir).
- **Novo projeto** (`NewProjectActivity`): nome, tamanho (presets Quadrado/Retrato/Paisagem/HD/A4/A5 ou personalizado em px/cm),
  resolução 72/150/300/600 dpi, fundo (branco, transparente ou cor), pré-visualização proporcional e validação (até 16 MP).
- **Editor** (`EditorActivity`): canvas nativo + barras/painéis em Views. Barra inferior **só com ícones**: Pincel, Borracha,
  Tinta e Seleção (+ cor atual e conta-gotas). Tocar na ferramenta ativa abre o painel: Pincéis | Ajustes | Cor.

## Ícones
Material Design Icons (Pictogrammers) convertidos para VectorDrawable em `res/drawable/ic_*.xml`
(licença em `assets/ICONS-LICENSE-Pictogrammers.txt`). Cores aplicadas por tint em tempo de execução.

## Bibliotecas de pincel (código-fonte em `src/main/cpp/third_party/`)
libmypaint 1.6.1 (+ json mínimo no lugar do json-c), MyPaint Brushes 2.0.2 (assets) e AnimBrush (~500 pincéis + stamps).

## Seleção (baseada na documentação do Procreate)
Laço, Retângulo, Elipse e Automática; Novo/Somar/Tirar; inverter, suavizar, pintar, limpar, copiar para camada, duplicar;
Transformar (mover, escalar, girar) em modo Livre, Uniforme ou Perspectiva.

## Estrutura
- `src/main/kotlin/com/clippaint/` — Home, NewProject, Editor, CanvasView, Panels, Bars, UiKit, Projects, Native (JNI).
- `src/main/cpp/` — `jni_bridge.cpp`, `core/` (Engine, Editor, entrada, overlays), `layers/`, `brush/`, `tools/`, `io/`, `graphics/`.
- Projetos ficam em `files/projects/<id>/` (`project.cpd`, `meta.json`, `thumb.png`); PNGs em `Android/data/com.clippaint/files/exports/`.

## Observações
- O C++ foi compilado/testado no PC (motor, pincéis, seleção, E/S). **O Kotlin não pôde ser compilado aqui**; se o CodeAssist
  apontar erros, corrija conforme o log.
