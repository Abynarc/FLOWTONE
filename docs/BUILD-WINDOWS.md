# Сборка Windows 10/11 x64

Требуются Windows x64, Visual Studio 2022 с компонентом Desktop development with C++ и Windows SDK, CMake 3.24+, Inno Setup 6.3+. JUCE 9.0.4 закреплён на commit `454fe891acc8204f99f4ef1e3a28c84b1a4da145`.

В новой рабочей папке PowerShell:

```powershell
git clone https://github.com/juce-framework/JUCE.git ../JUCE
git -C ../JUCE checkout 454fe891acc8204f99f4ef1e3a28c84b1a4da145
./packaging/build_windows.ps1
```

Если JUCE находится в другом месте, передайте `-JucePath C:/путь/JUCE`. Обычная сборка не требует Python и проверочных исходников.

Результат в dist:

- `FLOWTONE-0.3.1-Windows-x64-Setup.exe` — установщик в Common Files/VST3, удаление и проверяемая резервная копия при обновлении.
- `FLOWTONE-0.3.1-Windows-x64-VST3.zip` — полный VST3 bundle для ручной установки.
- Русская инструкция и SHA256SUMS-Windows.txt.

Сценарий проверяет PE x64 и отсутствие внешнего Visual C++ runtime, добавляет необходимые лицензии, генерирует хеши каждого устанавливаемого файла и компилирует Inno Setup. В установленный плагин не входят Python, проверки, аудиофайлы или исходники.

`.github/workflows/windows.yml` подготовлен для Windows Server 2022/x64: сборка VST3/EXE, тесты общего процессора и двух источников анализа, удаление временного файла после переноса, установка/обновление/проверка резервной копии, pluginval 1.0.4 и удаление плагина. Диагностика хранится отдельным CI-артефактом; дистрибутив содержит только установщик, VST3 ZIP, инструкцию и хеши.

Для проверки используется `-WithChecks` и четыре исходника из .github/checks: driver.cpp, ci_fixtures.py, checks.py, file-analysis-checks.py. WAV генерируются в CI из синтетических сигналов. Исходные пользовательские записи для сборки не требуются. Проверочный режим подключается CMake-опцией FLOWTONE_BUILD_VERIFICATION; по умолчанию он выключен.

На Mac проверен общий код с переносимым FFT без Accelerate; это не заменяет запуск Windows VST3. До выполнения Windows CI наличие готового EXE не утверждается. После CI нужно отдельно проверить реальную работу в FL Studio на Windows 10 и Windows 11: оба способа анализа, перенос из Edison, автоматизацию, компенсацию задержки, экспорт и восстановление проекта.
