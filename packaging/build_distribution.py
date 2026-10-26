#!/usr/bin/env python3
"""Build a Universal AU and a self-contained macOS installer; Python is build-only."""
import argparse
import hashlib
import json
import plistlib
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def hashes(folder):
    return {str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(folder.rglob('*')) if p.is_file()}


def run(command, **kwargs):
    print('+', ' '.join(map(str, command)), flush=True)
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-build', action='store_true')
    parser.add_argument('--no-dmg', action='store_true')
    args = parser.parse_args()
    build = ROOT / 'build-release'
    output = ROOT / 'dist'
    output.mkdir(exist_ok=True)
    if not args.skip_build:
        run(['cmake', '-S', ROOT, '-B', build, '-G', 'Unix Makefiles', '-DCMAKE_BUILD_TYPE=Release',
             '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0'])
        run(['cmake', '--build', build, '--config', 'Release', '--target', 'FLOWTONE_AU', '-j6'])
    source = build / 'FLOWTONE_artefacts/Release/AU/FLOWTONE.component'
    executable = source / 'Contents/MacOS/FLOWTONE'
    version = plistlib.loads((source / 'Contents/Info.plist').read_bytes())['CFBundleShortVersionString']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise RuntimeError('Unexpected bundle version')
    architectures = subprocess.check_output(['lipo', '-archs', executable], text=True).split()
    if set(architectures) != {'arm64', 'x86_64'}:
        raise RuntimeError('The distribution must contain both arm64 and x86_64')
    load_commands = subprocess.check_output(['vtool', '-show-build', executable], text=True)
    if re.findall(r'minos\s+([\d.]+)', load_commands) != ['12.0', '12.0']:
        raise RuntimeError('Unexpected minimum macOS version')
    licenses = source / 'Contents/Resources/Licenses'
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / 'third_party/stretch/LICENSE.txt', licenses / 'Signalsmith-Stretch-MIT.txt')
    shutil.copy2(ROOT / 'third_party/linear/LICENSE.txt', licenses / 'Signalsmith-Linear-MIT.txt')
    shutil.copy2(ROOT / 'THIRD_PARTY.md', licenses / 'THIRD_PARTY.md')
    run(['codesign', '--force' , '--sign', '-', '--timestamp=none', source])
    run(['codesign', '--verify', '--deep', '--strict', source])
    component_hashes = hashes(source)
    name = f'FLOWTONE-{version}-Universal'
    package = output / f'{name}.pkg'
    readme = output / f'{name}-Установка.txt'
    readme.write_text(f'''FLOWTONE {version} · FLOWMINNERS

УСТАНОВКА НА СТУДИЙНЫЙ MAC

1. Сохраните проект и закройте Logic Pro.
2. Откройте {name}.pkg и следуйте шагам установщика.
   Понадобится пароль администратора. Терминал и Python не нужны.
3. Откройте Logic заново: Audio FX → Audio Units → FLOWMINNERS → FLOWTONE.

Формат AU, Intel + Apple Silicon. Минимальная версия в сборке — macOS 12.
Проверки выполнены на Apple Silicon с текущей macOS. Отдельная проверка на
Intel и macOS 12 не выполнялась; совместимость версии Logic зависит от Apple.

Плагин: /Library/Audio/Plug-Ins/Components/FLOWTONE.component
Копии прежнего плагина: /Library/Application Support/FLOWMINNERS/FLOWTONE/Backups
Пользовательская копия FLOWTONE текущего вошедшего пользователя переносится
в резервные копии после проверки нового системного AU, чтобы не перекрывать
его. Копии в других учётных записях не меняются.

Это персональная сборка без Developer ID и нотариального заверения Apple.
Если macOS блокирует именно этот установщик: попробуйте открыть его, затем
Системные настройки → Конфиденциальность и безопасность → Всё равно открыть.
Не отключайте защиту macOS целиком. Справка Apple:
https://support.apple.com/102445

ДВА СПОСОБА АНАЛИЗА

Файл: перетащите один регион из Logic или аудиофайл из Finder на осциллограмму
либо нажмите «Выбрать файл». Вся запись анализируется в фоне без Play.
Показаны осциллограмма всего файла, прогресс, BPM и тональность. Результат и
осциллограмма сохраняются в проекте; само аудио в проект плагином не дублируется.
Для повторного анализа исходный файл должен оставаться доступным; регион,
полученный перетаскиванием, при необходимости перетащите заново.

Онлайн: нажмите «Онлайн» и включите воспроизведение проекта. Плагин анализирует
вход дорожки: обычно нужен ритмичный музыкальный фрагмент от 12–20 секунд.
«Файл» возвращает результат загруженного файла. Переключение не смешивает анализы.

Ручка меняет высоту звука этой дорожки, в том числе при экспорте проекта.
Загруженный файл служит только для анализа, а не заменяет звук дорожки.
Шаг ¼ тона = 50 центов = 0,5 полутона. Диапазон: −12…+12 полутонов.
Темп и длительность сохраняются. «До / После» сравнивает исходник и результат.
Сохранять исходное аудио в отдельный файл для работы плагина не нужно.
Кнопки ½ / ×2 уточняют найденный BPM и не меняют скорость.
При неоднозначном материале возможен статус «Недостаточно данных».
Тональности подписываются на английском. Абсолютная точность распознавания
на любом аудио не гарантируется.

Сборка проверена автоматическим рендером процессора и реального AU через
его интерфейс AudioComponentPlugInInterface. Окно Logic Pro и его bounce
из этой ограниченной среды не проверены. После установки проверьте плагин
в вашем проекте; полное описание проверок находится в документации проекта.
''', encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='flowtone-package-', dir=output) as temporary:
        work = Path(temporary)
        payload = work / 'payload'
        payload.mkdir()
        shutil.copytree(source, payload / source.name, symlinks=True)
        if hashes(payload / source.name) != component_hashes:
            raise RuntimeError('Staged AU differs from signed build')
        scripts = work / 'scripts'
        shutil.copytree(ROOT / 'packaging/scripts', scripts)
        (scripts / 'component.sha256').write_text(''.join(f'{digest}  {path}\n' for path, digest in component_hashes.items()))
        components = [{'RootRelativeBundlePath': source.name, 'BundleIsRelocatable': False,
                       'BundleIsVersionChecked': False, 'BundleHasStrictIdentifier': True,
                       'BundleOverwriteAction': 'upgrade'}]
        component_plist = work / 'components.plist'
        component_plist.write_bytes(plistlib.dumps(components))
        run(['pkgbuild', '--root', payload, '--identifier', 'com.flowminners.flowtone.pkg',
             '--version', version, '--install-location', '/Library/Audio/Plug-Ins/Components',
             '--component-plist', component_plist, '--scripts', scripts, '--ownership', 'recommended',
             '--compression', 'legacy', work / 'FLOWTONE-AU.pkg'])
        distribution = work / 'Distribution.xml'
        distribution.write_text((ROOT / 'packaging/Distribution.xml.in').read_text().replace('@VERSION@', version))
        run(['productbuild', '--distribution', distribution, '--resources', ROOT / 'packaging/resources',
             '--package-path', work, package])
        # Expanding the actual final package confirms payload, scripts and signed AU bytes.
        expanded = work / 'expanded'
        run(['pkgutil', '--expand-full', package, expanded])
        extracted = list(expanded.glob('*/Payload/FLOWTONE.component'))
        if len(extracted) != 1 or hashes(extracted[0]) != component_hashes:
            raise RuntimeError('Final package payload differs from signed AU')
        run(['codesign', '--verify', '--deep', '--strict', extracted[0]])
        image = output / f'{name}.dmg'
        dmg_created = False
        if not args.no_dmg:
            image_contents = work / 'image'
            image_contents.mkdir()
            shutil.copy2(package, image_contents / package.name)
            shutil.copy2(readme, image_contents / 'Установка.txt')
            result = subprocess.run(['hdiutil', 'create', '-volname', f'FLOWTONE {version}', '-srcfolder', str(image_contents),
                                     '-fs', 'HFS+', '-format', 'UDZO', '-ov', str(image)])
            if result.returncode == 0:
                run(['hdiutil', 'verify', image])
                dmg_created = True
            else:
                image.unlink(missing_ok=True)
                print('DMG creation unavailable; the standalone .pkg is ready.', flush=True)
    delivered = [package, readme] + ([image] if dmg_created else [])
    manifest = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in delivered}
    (output / f'{name}-sha256.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
    print('Ready:', package, flush=True)


if __name__ == '__main__':
    main()
