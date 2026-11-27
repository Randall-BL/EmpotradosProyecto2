#!/usr/bin/env bash
# Genera docs/documento-diseno.pdf a partir de docs/documento-diseno.md (DOC-09).
#
# El Markdown es la fuente unica (se lee en GitHub con diagramas Mermaid). Para el PDF,
# docs/diseno/pdf.lua quita el titulo y el indice de GitHub y cambia cada diagrama
# Mermaid por su figura TikZ de docs/diseno/tikz/; la portada y el preambulo estan en
# docs/diseno/. Requiere pandoc >= 3 y xelatex (TeX Live con tikz).
#   PANDOC=/ruta/a/pandoc scripts/build-doc.sh     # si pandoc no esta en el PATH
#   (pandoc sin instalar nada global: pip install pypandoc_binary en un venv)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PANDOC="${PANDOC:-pandoc}"
DOCS="$HERE/docs"
OUT="${OUT:-$DOCS/documento-diseno.pdf}"
command -v "$PANDOC" >/dev/null || { echo "ERROR: falta pandoc (PANDOC=...)" >&2; exit 1; }

cd "$DOCS"
DOC_TIKZ="$DOCS/diseno/tikz" "$PANDOC" documento-diseno.md \
    --from=markdown+pipe_tables+raw_html-implicit_figures+implicit_figures \
    --lua-filter=diseno/pdf.lua \
    --resource-path="$DOCS" \
    --pdf-engine=xelatex \
    --toc --toc-depth=2 \
    -H diseno/preambulo.tex -B diseno/portada.tex \
    -V documentclass=article -V papersize=a4 -V fontsize=11pt -V lang=es \
    -V geometry:margin=2.3cm \
    -V mainfont="Noto Serif" -V sansfont="Noto Sans" -V monofont="DejaVu Sans Mono" \
    -V monofontoptions="Scale=0.82" \
    -V colorlinks=true -V linkcolor=black -V urlcolor=blue -V toccolor=black \
    -o "$OUT"
echo ">> $OUT"
