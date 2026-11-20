package me.andrey.treevisualiser;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import org.bukkit.Bukkit;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Player;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Превращает действия игроков в запросы к Backend и применяет ответы:
 * перерисовывает дерево и пишет результат в чат. Работает в главном потоке.
 */
final class TreeController {

    private final BackendProcess backend;
    private final TreeRenderer renderer;
    private final Logger logger;
    private final Set<Long> pendingDeletes = new HashSet<>();

    TreeController(BackendProcess backend, TreeRenderer renderer, Logger logger) {
        this.backend = backend;
        this.renderer = renderer;
        this.logger = logger;
    }

    void requestState() {
        send(request("state"), null, response -> {
        });
    }

    void insert(CommandSender sender, long key) {
        JsonObject request = request("insert");
        request.addProperty("key", key);
        send(request, sender, response -> info(sender, "Ключ " + key + " добавлен."));
    }

    void random(CommandSender sender, int count, long min, long max) {
        JsonObject request = request("random");
        request.addProperty("count", count);
        request.addProperty("min", min);
        request.addProperty("max", max);
        send(request, sender, response -> {
            List<String> keys = new ArrayList<>();
            if (response.has("inserted")) {
                for (JsonElement key : response.getAsJsonArray("inserted")) {
                    keys.add(key.getAsString());
                }
            }
            info(sender, "Добавлено ключей: " + keys.size() + ": " + String.join(", ", keys));
        });
    }

    void view(CommandSender sender, String tree) {
        JsonObject request = request("view");
        request.addProperty("tree", tree);
        send(request, sender, response -> info(sender, "Показано дерево: " + tree));
    }

    void clear(CommandSender sender) {
        send(request("clear"), sender, response -> info(sender, "Все деревья очищены."));
    }

    void deleteByArrow(Player player, long key) {
        if (!pendingDeletes.add(key)) {
            return;
        }
        JsonObject request = request("delete");
        request.addProperty("key", key);
        send(request, player, response -> info(player, "Ключ " + key + " удалён."),
                () -> pendingDeletes.remove(key));
    }

    private void send(JsonObject request, CommandSender sender, Consumer<JsonObject> onSuccess) {
        send(request, sender, onSuccess, () -> {
        });
    }

    private void send(JsonObject request, CommandSender sender, Consumer<JsonObject> onSuccess, Runnable always) {
        backend.send(request, result -> {
            always.run();
            handle(result, sender, onSuccess);
        });
    }

    private void handle(BackendResult result, CommandSender sender, Consumer<JsonObject> onSuccess) {
        if (result.restarted()) {
            Bukkit.broadcast(Component.text("Backend был перезапущен, деревья начаты заново.",
                    NamedTextColor.GOLD));
        }
        if (result.isFailure()) {
            logger.warning("Сбой Backend: " + result.failure());
            renderer.clear();
            error(sender, "Сбой Backend: " + result.failure()
                    + ". При следующей команде он будет запущен заново.");
            return;
        }
        if (!result.ok()) {
            error(sender, describeError(result.errorCode()) + " (" + result.errorMessage() + ")");
            return;
        }
        TreeLayout layout;
        try {
            layout = TreeLayout.parse(result.response());
        } catch (RuntimeException e) {
            logger.log(Level.WARNING, "Некорректный ответ Backend: " + result.response(), e);
            error(sender, "Backend прислал некорректную раскладку, подробности в логе сервера.");
            return;
        }
        renderer.render(layout);
        onSuccess.accept(result.response());
    }

    private static String describeError(String code) {
        return switch (code) {
            case "duplicate_key" -> "Такой ключ уже есть";
            case "key_not_found" -> "Такого ключа нет";
            case "unknown_tree" -> "Это дерево недоступно";
            case "range_exhausted" -> "В диапазоне не хватает свободных ключей";
            case "bad_request" -> "Некорректный запрос";
            default -> "Ошибка Backend: " + code;
        };
    }

    private static JsonObject request(String cmd) {
        JsonObject request = new JsonObject();
        request.addProperty("cmd", cmd);
        return request;
    }

    private static void info(CommandSender sender, String message) {
        if (sender != null) {
            sender.sendMessage(Component.text(message, NamedTextColor.GREEN));
        }
    }

    private static void error(CommandSender sender, String message) {
        if (sender != null) {
            sender.sendMessage(Component.text(message, NamedTextColor.RED));
        }
    }
}
