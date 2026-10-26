# Сборка Windows 10/11 x64

Требуются Windows x64, Visual Studio 2022 с компонентом Desktop development with C++ и Windows SDK, CMake 3.24+, Inno Setup 6.3+. JUCE 9.0.4 закреплён на commit `454fe891acc8204f99f4ef1e3a28c84b1a4da145`.

В новой рабочей папке PowerShell:

```powershell
git clone https://github.com/juce-framework/JUCE.git ../JUCE
git -C ../JUCE checkout 454fe891acc8204f99f4ef1e3a28c84b1a4da145
./packaging/build_windows.ps1
```

Если JUCE находится в другом месте, передайте `-JucePath C:/путь/JUCE`. Python для сборки не требуется.

Результат в dist:

- `FLOWTONE-0.3.1-Windows-x64-Setup.exe` — установщик в Common Files/VST3, удаление и проверяемая резервная копия при обновлении.
- `FLOWTONE-0.3.1-Windows-x64-VST3.zip` — полный VST3 bundle для ручной установки.
- Русская инструкция и SHA256SUMS-Windows.txt.

Сценарий проверяет PE x64 и отсутствие внешнего Visual C++ runtime, добавляет необходимые лицензии, генерирует хеши каждого устанавливаемого файла и компилирует Inno Setup. В установленный плагин не входят Python, проверки, аудиофайлы или исходники.

Сборка на GitHub запускается вручную: **Actions → Windows VST3 installer → Run workflow**. Результат доступен в артефакте **FLOWTONE-Windows-x64**.
