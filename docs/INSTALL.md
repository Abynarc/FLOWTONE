# FLOWTONE для macOS

1. Скачайте `FLOWTONE-0.4.0-Universal.pkg` из [релиза](https://github.com/Abynarc/FLOWTONE/releases/tag/v0.4.0).
2. Сохраните проект и закройте секвенсор.
3. Откройте PKG и выполните установку. Потребуется пароль администратора.
4. В Logic Pro: **Audio FX → Audio Units → FLOWMINNERS → FLOWTONE**.

macOS 12+, Apple Silicon и Intel. Формат AU. Требования конкретной версии секвенсора учитываются отдельно.

Выберите **RU** или **EN** справа от «Обход». Язык сохраняется в проекте.

Плагин: `/Library/Audio/Plug-Ins/Components/FLOWTONE.component`.
Резервные копии: `/Library/Application Support/FLOWMINNERS/FLOWTONE/Backups`.

Установщик сохраняет предыдущую системную версию. Прежняя пользовательская копия текущего пользователя переносится в резервные копии, чтобы не перекрывать системную.

PKG без Developer ID и нотариального заверения Apple; AU подписан ad-hoc. Если macOS блокирует установщик, после попытки открытия используйте **Системные настройки → Конфиденциальность и безопасность → Всё равно открыть**.
