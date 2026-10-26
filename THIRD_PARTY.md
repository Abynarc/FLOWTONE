# Зависимости

JUCE 9.0.4, commit 454fe891acc8204f99f4ef1e3a28c84b1a4da145, https://github.com/juce-framework/JUCE. JUCE распространяется на условиях JUCE EULA / AGPLv3; использование и распространение требуют соблюдения применимых условий JUCE. JUCE не включён в дерево исходников FLOWTONE, используется соседняя папка JUCE.

Signalsmith Stretch 1.3.2, commit 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da, https://github.com/signalsmith-audio/signalsmith-stretch. MIT, автор Signalsmith Audio. Исходный заголовок и текст лицензии в third_party/stretch.

Signalsmith Linear 0.6.6, commit 7bb1c8f36794d6980080ce817c91f8da8fe56a66, https://github.com/Signalsmith-Audio/linear. MIT, автор Signalsmith Audio. Необходимые заголовки и лицензия в third_party/linear. На macOS FFT по умолчанию использует Apple Accelerate; на Windows — переносимую реализацию Signalsmith Linear. Опция FLOWTONE_USE_ACCELERATE=OFF позволяет отдельно проверить переносимый путь на Mac.

Тексты лицензий Signalsmith включаются также в Contents/Resources/Licenses установленного AU/VST3. Аудиоматериал и исходники сторонних тестовых корпусов в плагин и установщик не включаются. Windows-установщик собирается Inno Setup 6.3+; среда сборки и средства проверки не включаются в установленный плагин.
