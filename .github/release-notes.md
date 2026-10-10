Unofficial build of [NeuralNote](https://github.com/DamRsn/NeuralNote) by Damien Ronssin, made accessible for blind users with the NVDA screen reader. Not supported by the original author: please report accessibility problems in this fork, not upstream.

## What is different

- Every button, fader and field has a name NVDA reads, and everything can be reached with Tab.
- The instrument picker and the model panel work with the arrow keys, Space and Escape.
- The status line, the time display and the transcription progress can be read.
- Transcription progress, its end, and model download progress are spoken wherever the focus is.
- Ctrl+Left/Right moves the playhead by 1 second, Ctrl+Shift+Left/Right by 5 seconds.

Other keys in the main window: Space plays or pauses, Shift+Space goes back to the start, R records, M mutes the input, L toggles loop, C toggles follow playhead, Shift+Backspace clears.

## Install

1. Download the zip below and unpack it.
2. Copy the `NeuralNote.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. Rescan plugins in your DAW.
4. Open the plugin, press the Model button and download a model (Medium is recommended). Models are not included in the zip.

Windows x64, VST3 only. GPU transcription uses Vulkan, and the compute device can be changed in the Settings menu. Tested with NVDA 2026.2 in REAPER 7.79.

---

## По-русски

Неофициальная сборка NeuralNote, доработанная для незрячих пользователей с NVDA. Автор оригинала её не поддерживает: о проблемах с доступностью пишите в этот форк.

- Все элементы подписаны и доступны по Tab. Выбор инструментов и моделей работает стрелками, пробелом и Escape.
- Озвучиваются ход и окончание распознавания и скачивание модели.
- Ctrl+стрелки влево и вправо перематывают на 1 секунду, с Shift на 5 секунд.

Установка: распакуйте zip, скопируйте папку `NeuralNote.vst3` в `C:\Program Files\Common Files\VST3`, пересканируйте плагины в DAW. Модель скачивается в самом плагине кнопкой Model.

License: Apache 2.0, same as the original.
