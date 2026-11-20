package me.andrey.treevisualiser;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;
import org.bukkit.command.TabExecutor;

import java.util.List;
import java.util.Locale;

/** /tree insert|random|view|clear */
final class TreeCommand implements TabExecutor {

    private static final List<String> SUBCOMMANDS = List.of("insert", "random", "view", "clear");
    private static final List<String> TREE_IDS = List.of("avl", "rb", "treap", "splay", "btree");

    private final TreeController controller;
    private final PluginSettings settings;

    TreeCommand(TreeController controller, PluginSettings settings) {
        this.controller = controller;
        this.settings = settings;
    }

    @Override
    public boolean onCommand(CommandSender sender, Command command, String label, String[] args) {
        if (args.length == 0) {
            return usage(sender);
        }
        switch (args[0].toLowerCase(Locale.ROOT)) {
            case "insert" -> {
                if (args.length != 2) {
                    return error(sender, "Использование: /tree insert <ключ>");
                }
                Long key = parseLong(args[1]);
                if (key == null) {
                    return error(sender, "Ключ должен быть целым числом от " + Long.MIN_VALUE + " до " + Long.MAX_VALUE);
                }
                controller.insert(sender, key);
            }
            case "random" -> {
                if (args.length != 2 && args.length != 4) {
                    return error(sender, "Использование: /tree random <n> [min max]");
                }
                Long count = parseLong(args[1]);
                if (count == null || count < 1 || count > settings.randomMaxCount()) {
                    return error(sender, "n должно быть от 1 до " + settings.randomMaxCount());
                }
                long min = settings.randomMin();
                long max = settings.randomMax();
                if (args.length == 4) {
                    Long parsedMin = parseLong(args[2]);
                    Long parsedMax = parseLong(args[3]);
                    if (parsedMin == null || parsedMax == null) {
                        return error(sender, "min и max должны быть целыми числами");
                    }
                    if (parsedMin > parsedMax) {
                        return error(sender, "min не должен быть больше max");
                    }
                    min = parsedMin;
                    max = parsedMax;
                }
                controller.random(sender, count.intValue(), min, max);
            }
            case "view" -> {
                if (args.length != 2) {
                    return error(sender, "Использование: /tree view <" + String.join("|", TREE_IDS) + ">");
                }
                controller.view(sender, args[1].toLowerCase(Locale.ROOT));
            }
            case "clear" -> {
                if (args.length != 1) {
                    return error(sender, "Использование: /tree clear");
                }
                controller.clear(sender);
            }
            default -> {
                return usage(sender);
            }
        }
        return true;
    }

    @Override
    public List<String> onTabComplete(CommandSender sender, Command command, String alias, String[] args) {
        if (args.length == 1) {
            return filter(SUBCOMMANDS, args[0]);
        }
        if (args.length == 2 && args[0].equalsIgnoreCase("view")) {
            return filter(TREE_IDS, args[1]);
        }
        return List.of();
    }

    private static List<String> filter(List<String> options, String prefix) {
        String lower = prefix.toLowerCase(Locale.ROOT);
        return options.stream().filter(option -> option.startsWith(lower)).toList();
    }

    private static Long parseLong(String text) {
        try {
            return Long.parseLong(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    private static boolean usage(CommandSender sender) {
        sender.sendMessage(Component.text("""
                /tree insert <ключ> — добавить ключ во все деревья
                /tree random <n> [min max] — добавить n случайных ключей
                /tree view <avl|rb|treap|splay|btree> — показать другое дерево
                /tree clear — очистить все деревья
                Выстрел из лука в блок узла удаляет ключ.""", NamedTextColor.YELLOW));
        return true;
    }

    private static boolean error(CommandSender sender, String message) {
        sender.sendMessage(Component.text(message, NamedTextColor.RED));
        return true;
    }
}
