package me.andrey.treevisualiser;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.TextComponent;
import net.kyori.adventure.text.format.NamedTextColor;
import org.bukkit.Bukkit;
import org.bukkit.FluidCollisionMode;
import org.bukkit.block.Block;
import org.bukkit.entity.Player;
import org.bukkit.scheduler.BukkitRunnable;
import org.bukkit.util.RayTraceResult;

import java.util.Map;

/**
 * Показывает в actionbar подробности узла, на который смотрит игрок.
 * Работает только с последней раскладкой и не обращается к Backend.
 */
final class HoverTask extends BukkitRunnable {

    /** Как часто (в запусках задачи) проверять, не пропали ли сущности дерева. */
    private static final int REPAIR_EVERY_RUNS = 10;

    private final TreeRenderer renderer;
    private final PluginSettings settings;
    private int runs;

    HoverTask(TreeRenderer renderer, PluginSettings settings) {
        this.renderer = renderer;
        this.settings = settings;
    }

    @Override
    public void run() {
        if (++runs % REPAIR_EVERY_RUNS == 0) {
            renderer.repairIfNeeded();
        }
        for (Player player : Bukkit.getOnlinePlayers()) {
            RayTraceResult result = player.rayTraceBlocks(settings.hoverMaxDistance(), FluidCollisionMode.NEVER);
            if (result == null) {
                continue;
            }
            Block block = result.getHitBlock();
            if (block == null) {
                continue;
            }
            TreeRenderer.NodeHit hit = renderer.lookup(block);
            if (hit != null) {
                player.sendActionBar(describe(hit));
            }
        }
    }

    private static Component describe(TreeRenderer.NodeHit hit) {
        TextComponent.Builder text = Component.text()
                .append(Component.text("ключ ", NamedTextColor.GRAY))
                .append(Component.text(hit.key(), NamedTextColor.WHITE));
        for (Map.Entry<String, String> entry : hit.node().info().entrySet()) {
            text.append(Component.text("  │  ", NamedTextColor.DARK_GRAY))
                    .append(Component.text(entry.getKey() + ": ", NamedTextColor.GRAY))
                    .append(Component.text(entry.getValue(), NamedTextColor.WHITE));
        }
        return text.build();
    }
}
