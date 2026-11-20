package me.andrey.treevisualiser;

import org.bukkit.command.PluginCommand;
import org.bukkit.plugin.java.JavaPlugin;
import org.bukkit.scheduler.BukkitTask;

public final class TreeVisualiserPlugin extends JavaPlugin {

    private BackendProcess backend;
    private TreeRenderer renderer;
    private BukkitTask hoverTask;

    @Override
    public void onEnable() {
        saveDefaultConfig();
        PluginSettings settings = PluginSettings.load(getConfig(), getLogger());

        renderer = new TreeRenderer(this, settings);
        renderer.removeLeftovers();

        backend = new BackendProcess(this, settings);
        backend.start();

        TreeController controller = new TreeController(backend, renderer, getLogger());
        PluginCommand command = getCommand("tree");
        if (command != null) {
            TreeCommand executor = new TreeCommand(controller, settings);
            command.setExecutor(executor);
            command.setTabCompleter(executor);
        }
        getServer().getPluginManager().registerEvents(new ArrowHitListener(renderer, controller), this);
        hoverTask = new HoverTask(renderer, settings).runTaskTimer(this, 20L, settings.hoverPeriodTicks());

        controller.requestState();
    }

    @Override
    public void onDisable() {
        if (hoverTask != null) {
            hoverTask.cancel();
        }
        if (backend != null) {
            backend.shutdown();
        }
        if (renderer != null) {
            renderer.clear();
        }
    }
}
