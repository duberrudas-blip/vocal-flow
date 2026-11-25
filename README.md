# VOCAL FLOW

Plugin VST3 de **pitch + formant** para voz (Windows y macOS), pensado para Ableton Live.

| Control | Qué hace |
|---|---|
| **PITCH** | Cambia la afinación ±1200 cents (±1 octava) conservando el timbre de la voz |
| **FORMANT** | Mueve los formantes ±1200 cents sin cambiar la nota (voz más grave/aguda, "ardilla"/"gigante") |
| **LINK** | Une los dos mandos: al mover uno, el otro se mueve igual |
| **FINE** | Reduce el rango de los mandos a ±100 cents para ajuste fino |
| **MIX** | Mezcla seca/procesada (0–100 %) |

Doble clic en un mando = volver a su valor por defecto. Latencia fija de 2048 muestras (Ableton la compensa solo).

## Compilar en GitHub (sin instalar nada)
1. Crea un repositorio vacío en GitHub (ej. `vocal-flow`) y sube estos archivos.
2. Ve a la pestaña **Actions** → "Build Vocal Flow (VST3)". Se ejecuta solo con cada `push` (o con *Run workflow*).
3. Al terminar, abre la ejecución y descarga en **Artifacts**: `VocalFlow-Windows-VST3` y `VocalFlow-macOS-VST3`.
4. Para publicar una versión descargable: `git tag v1.0.0 && git push --tags` (se adjunta a *Releases*).

## Instalar
- **Windows:** descomprime y copia la carpeta `Vocal Flow.vst3` a `C:\Program Files\Common Files\VST3\`.
- **macOS:** descomprime y copia `Vocal Flow.vst3` a `/Library/Audio/Plug-Ins/VST3/`.
  Si macOS lo bloquea (no está notarizado): `xattr -cr "/Library/Audio/Plug-Ins/VST3/Vocal Flow.vst3"`.
- En Ableton: Preferencias → Plug-ins → activar VST3 y pulsar *Rescan*.

## Compilar en local
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
(requiere CMake 3.22+, Visual Studio 2022 en Windows o Xcode en macOS; JUCE se descarga solo).

## Notas
- Motor: vocoder de fases (STFT 2048, solape 75 %) con envolvente espectral cepstral para separar tono y formantes.
- JUCE se usa bajo su licencia AGPLv3/comercial: si vas a vender o distribuir el plugin cerrado necesitas licencia de JUCE.
- Proyecto independiente, sin relación con Waves; los gráficos son propios (dibujados por código).
