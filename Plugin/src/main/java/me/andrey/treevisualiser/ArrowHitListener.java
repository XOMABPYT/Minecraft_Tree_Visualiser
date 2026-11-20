package me.andrey.treevisualiser;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import org.bukkit.block.Block;
import org.bukkit.entity.AbstractArrow;
import org.bukkit.entity.Arrow;
import org.bukkit.entity.Player;
import org.bukkit.entity.SpectralArrow;
import org.bukkit.event.EventHandler;
import org.bukkit.event.Listener;
import org.bukkit.event.block.BlockBreakEvent;
import org.bukkit.event.block.BlockExplodeEvent;
import org.bukkit.event.entity.EntityExplodeEvent;
import org.bukkit.event.entity.ProjectileHitEvent;

/** Удаление ключа выстрелом из лука и защита блоков дерева от разрушения. */
final class ArrowHitListener implements Listener {

    private static final String PERMISSION = "treevisualiser.use";

    private final TreeRenderer renderer;
    private final TreeController controller;

    ArrowHitListener(TreeRenderer renderer, TreeController controller) {
        this.renderer = renderer;
        this.controller = controller;
    }

    @EventHandler(ignoreCancelled = true)
    public void onProjectileHit(ProjectileHitEvent event) {
        if (!(event.getEntity() instanceof Arrow) && !(event.getEntity() instanceof SpectralArrow)) {
            return;
        }
        AbstractArrow arrow = (AbstractArrow) event.getEntity();
        if (!(arrow.getShooter() instanceof Player player)) {
            return;
        }
        Block block = event.getHitBlock();
        if (block == null) {
            return;
        }
        TreeRenderer.NodeHit hit = renderer.lookup(block);
        if (hit == null) {
            return;
        }
        arrow.remove();
        if (!player.hasPermission(PERMISSION)) {
            player.sendActionBar(Component.text("Нет прав удалять ключи", NamedTextColor.RED));
            return;
        }
        controller.deleteByArrow(player, hit.key());
    }

    @EventHandler(ignoreCancelled = true)
    public void onBlockBreak(BlockBreakEvent event) {
        if (renderer.isNodeBlock(event.getBlock())) {
            event.setCancelled(true);
            event.getPlayer().sendActionBar(
                    Component.text("Чтобы удалить ключ, выстрели в него из лука", NamedTextColor.YELLOW));
        }
    }

    @EventHandler(ignoreCancelled = true)
    public void onEntityExplode(EntityExplodeEvent event) {
        event.blockList().removeIf(renderer::isNodeBlock);
    }

    @EventHandler(ignoreCancelled = true)
    public void onBlockExplode(BlockExplodeEvent event) {
        event.blockList().removeIf(renderer::isNodeBlock);
    }
}
