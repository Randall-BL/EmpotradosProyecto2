-- Filtro de pandoc para la version PDF de docs/documento-diseno.md:
--  * quita lo que esta entre <!-- pdf:omitir --> y <!-- pdf:fin-omitir -->
--    (titulo, integrantes e indice de GitHub: el PDF tiene portada e indice propios);
--  * cambia cada bloque ```mermaid por la figura TikZ docs/diseno/tikz/<fig>.tex,
--    con el titulo de la linea "%% cap:".
local dir = os.getenv("DOC_TIKZ") or "tikz"

local function latex(md)
  return pandoc.write(pandoc.read(md, "markdown"), "latex"):gsub("%s+$", "")
end

function Blocks(blocks)
  local out, skip = {}, false
  for _, b in ipairs(blocks) do
    if b.t == "RawBlock" and b.format == "html" and b.text:match("pdf:omitir") then
      skip = true
    elseif b.t == "RawBlock" and b.format == "html" and b.text:match("pdf:fin%-omitir") then
      skip = false
    elseif not skip then
      out[#out + 1] = b
    end
  end
  return out
end

function CodeBlock(b)
  if not b.classes:includes("mermaid") then return nil end
  local fig = b.text:match("%%%% fig: *(%S+)")
  local cap = b.text:match("%%%% cap: *([^\n]+)") or ""
  if not fig then
    io.stderr:write("pdf.lua: bloque mermaid sin '%% fig:'\n")
    return {}
  end
  return pandoc.RawBlock("latex",
    "\\begin{figure}[htbp]\n\\centering\n\\input{" .. dir .. "/" .. fig .. ".tex}\n" ..
    "\\caption{" .. latex(cap) .. "}\n\\end{figure}")
end
