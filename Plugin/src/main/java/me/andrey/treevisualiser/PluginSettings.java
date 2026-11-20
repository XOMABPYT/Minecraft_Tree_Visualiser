package me.andrey.treevisualiser;

import org.bukkit.Material;
import org.bukkit.configuration.file.FileConfiguration;

import java.util.logging.Logger;

/** Настройки из config.yml, прочитанные один раз при включении плагина. */
record PluginSettings(
        String backendPath,
        long backendSeed,
        long backendTimeoutMs,
        String worldName,
        int originX,
        int originY,
        int originZ,
        int spacingX,
        int spacingY,
        Material edgeMaterial,
        float edgeThickness,
        float labelScale,
        float viewRange,
        long randomMin,
        long randomMax,
        int randomMaxCount,
        int hoverPeriodTicks,
        double hoverMaxDistance
) {
    static PluginSettings load(FileConfiguration config, Logger logger) {
        Material edgeMaterial = Material.matchMaterial(config.getString("render.edge-material", "GRAY_CONCRETE"));
        if (edgeMaterial == null || !edgeMaterial.isBlock()) {
            logger.warning("render.edge-material не является блоком, используется GRAY_CONCRETE");
            edgeMaterial = Material.GRAY_CONCRETE;
        }
        long randomMin = config.getLong("random.min", 0);
        long randomMax = config.getLong("random.max", 999);
        if (randomMin > randomMax) {
            logger.warning("random.min больше random.max, используются 0 и 999");
            randomMin = 0;
            randomMax = 999;
        }
        return new PluginSettings(
                config.getString("backend.path", "../../Backend/build/tree_backend"),
                config.getLong("backend.seed", -1),
                Math.max(100, config.getLong("backend.timeout-ms", 5000)),
                config.getString("render.world", "world"),
                config.getInt("render.origin.x", 0),
                config.getInt("render.origin.y", 30),
                config.getInt("render.origin.z", 20),
                Math.max(1, config.getInt("render.spacing-x", 2)),
                Math.max(1, config.getInt("render.spacing-y", 4)),
                edgeMaterial,
                (float) config.getDouble("render.edge-thickness", 0.08),
                (float) config.getDouble("render.label-scale", 1.0),
                (float) config.getDouble("render.view-range", 2.0),
                randomMin,
                randomMax,
                Math.max(1, config.getInt("random.max-count", 100)),
                Math.max(1, config.getInt("hover.period-ticks", 4)),
                config.getDouble("hover.max-distance", 96)
        );
    }
}
