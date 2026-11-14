# Material para la tarjeta (OR-05)

Lista de lo que hace falta para probar y presentar el reproductor. La columna
"Responsable" se llena al confirmar el reparto del grupo (OR-02); "Estado" es
`falta`, `pedido` o `listo`.

| Material | Para qué | Especificación | Responsable | Estado |
|---|---|---|---|---|
| Tarjeta DE1-SoC con fuente de 12 V y cable USB-Blaster (mini-USB) | Todo | Préstamo del laboratorio; anotar la revisión (rev. F o posterior trae LTC2308) | | falta |
| microSD | Canciones y arranque del HPS | ≥ 4 GB, clase 10 | | falta |
| Lector de microSD para el PC | Copiar canciones e imagen | USB | | falta |
| Potenciómetro | Volumen (ADC CH0) | 10 kΩ lineal, entre 5 V y GND del conector J15 "ADC CON" (2×5, junto a GPIO), cursor a CH0 | | falta |
| Cables dupont hembra-hembra | Conector del ADC | 3 (5 V, GND, CH0) | | falta |
| Audífonos o parlantes activos | LINE OUT (verde) | Conector de 3.5 mm | | falta |
| Monitor VGA y cable | Pantalla del reproductor | 640 × 480 a 60 Hz | | falta |
| 10+ archivos WAV de prueba | Demo (PR-06) | 16 bits, 8 / 16 / 44.1 kHz, con chunk `LIST/INFO` | | falta |

Las pruebas que se hacen con este material están en
[docs/pruebas-tarjeta.md](pruebas-tarjeta.md).
