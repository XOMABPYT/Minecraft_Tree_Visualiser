# Minecraft Tree Visualiser

Визуализатор деревьев поиска внутри Minecraft. Ключи добавляются командами в
чате, удаляются выстрелом из лука в блок узла, а при наведении взгляда на узел
в actionbar показываются его подробности.

Сейчас подключено только декартово дерево (`treap`). AVL, красно-чёрное,
splay- и B-дерево будут подключены по мере готовности: команда
`/tree view <дерево>` для них пока отвечает «Это дерево недоступно».

## Как устроено

```
Minecraft-клиент ──localhost──▶ Paper-сервер + плагин (Java, Plugin/)
                                   │ stdin/stdout, одна строка JSON на сообщение
                                   ▼
                                 Backend (C++17, Backend/): деревья и раскладка
```

Плагин сам запускает Backend как дочерний процесс, пересылает ему команды и
рисует готовую раскладку. Подробности архитектуры и протокола — в
[AGENTS.md](AGENTS.md).

## Требования

- CMake 3.20+ и компилятор с поддержкой C++17;
- Java 21 (например, `brew install --cask temurin@21`);
- Minecraft Java Edition **1.21.11** — версия клиента должна совпадать с
  версией сервера Paper из `Plugin/build.gradle.kts`.

## Сборка и тесты Backend

```
cmake -S Backend -B Backend/build -DCMAKE_BUILD_TYPE=Debug
cmake --build Backend/build
ctest --test-dir Backend/build --output-on-failure
```

Бинарник: `Backend/build/tree_backend`. Его можно проверить и без Minecraft:

```
echo '{"id": 1, "cmd": "insert", "key": 42}' | Backend/build/tree_backend
```

## Запуск сервера с плагином

```
cd Plugin
./gradlew build       # собрать jar плагина
./gradlew runServer   # скачать Paper, положить плагин и запустить сервер
```

При первом запуске сервер остановится и попросит принять EULA: поставьте
`eula=true` в `Plugin/run/eula.txt` и запустите `./gradlew runServer` снова.

Рекомендуемые настройки в `Plugin/run/server.properties` (поменять до первого
создания мира): `level-type=minecraft\:flat`, `gamemode=creative`,
`spawn-protection=0`, `spawn-monsters=false`.

В консоли сервера выдайте себе права: `op <ник>`. Затем в клиенте:
«Сетевая игра» → «Прямое подключение» → `localhost`.

Для перезагрузки плагина не используйте `/reload`: перезапускайте сервер.

## Команды

| Команда | Действие |
|---|---|
| `/tree insert <key>` | вставить ключ во все деревья |
| `/tree random <n> [min max]` | вставить `n` случайных ключей (по умолчанию из `[0, 999]`, `n ≤ 100`) |
| `/tree view <avl\|rb\|treap\|splay\|btree>` | показать другое дерево |
| `/tree clear` | очистить все деревья |

- Выстрел из лука в блок узла удаляет его ключ.
- Взгляд на блок узла показывает его подробности в actionbar
  (у treap — высота поддерева, приоритет и размер).

## Настройки

Файл `Plugin/run/plugins/TreeVisualiser/config.yml` создаётся при первом
запуске. Основное:

- `backend.path` — путь к бинарнику Backend относительно папки сервера
  (по умолчанию `../../Backend/build/tree_backend`);
- `backend.seed` — seed случайных ключей и приоритетов treap, `-1` — новый при
  каждом запуске;
- `render.origin`, `render.spacing-x`, `render.spacing-y` — где и с каким шагом
  рисуется дерево (корень в `origin`, глубина растёт вниз);
- `random.min`, `random.max`, `random.max-count` — параметры `/tree random`.

Если Backend упадёт или не ответит за `backend.timeout-ms`, плагин сообщит об
этом в чат и перезапустит его при следующей команде (деревья начнутся заново).
