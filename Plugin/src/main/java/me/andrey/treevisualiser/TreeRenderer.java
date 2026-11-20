package me.andrey.treevisualiser;

import net.kyori.adventure.text.Component;
import org.bukkit.Bukkit;
import org.bukkit.Color;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.block.Block;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.BlockDisplay;
import org.bukkit.entity.Display;
import org.bukkit.entity.Entity;
import org.bukkit.entity.TextDisplay;
import org.bukkit.plugin.java.JavaPlugin;
import org.bukkit.util.Transformation;
import org.joml.Quaternionf;
import org.joml.Vector3f;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Рисует раскладку дерева в мире: блок на каждый ключ, подпись над узлом
 * и тонкие рёбра. Все методы вызываются только из главного потока.
 */
final class TreeRenderer {

    record NodeHit(TreeLayout.Node node, long key) {
    }

    private record BlockPos(UUID world, int x, int y, int z) {
        static BlockPos of(Block block) {
            return new BlockPos(block.getWorld().getUID(), block.getX(), block.getY(), block.getZ());
        }
    }

    private static final Map<String, Material> COLOR_BLOCKS = Map.of(
            "white", Material.WHITE_CONCRETE,
            "red", Material.RED_CONCRETE,
            "black", Material.BLACK_CONCRETE,
            "gray", Material.GRAY_CONCRETE,
            "blue", Material.BLUE_CONCRETE,
            "green", Material.GREEN_CONCRETE,
            "yellow", Material.YELLOW_CONCRETE);
    private static final Set<Material> NODE_MATERIALS = EnumSet.copyOf(COLOR_BLOCKS.values());

    /** Примерная высота строки TextDisplay в блоках при масштабе 1. */
    private static final double LABEL_LINE_HEIGHT = 0.27;
    private static final double LABEL_GAP = 0.1;
    private static final String LEFTOVERS_FILE = "rendered-blocks.txt";

    private final Logger logger;
    private final PluginSettings settings;
    private final Path leftoversPath;
    private final Map<BlockPos, NodeHit> nodeBlocks = new HashMap<>();
    private final Map<BlockPos, BlockData> originalBlocks = new LinkedHashMap<>();
    private final List<Entity> entities = new ArrayList<>();
    private TreeLayout current = TreeLayout.empty();

    TreeRenderer(JavaPlugin plugin, PluginSettings settings) {
        this.logger = plugin.getLogger();
        this.settings = settings;
        this.leftoversPath = plugin.getDataFolder().toPath().resolve(LEFTOVERS_FILE);
    }

    void render(TreeLayout layout) {
        clear();
        current = layout;
        World world = resolveWorld();
        if (world == null) {
            logger.warning("Мир для отрисовки не найден");
            return;
        }

        for (TreeLayout.Node node : layout.nodes()) {
            Material material = COLOR_BLOCKS.getOrDefault(node.color(), Material.WHITE_CONCRETE);
            for (int i = 0; i < node.keys().size(); i++) {
                Block block = world.getBlockAt(blockX(node, i), blockY(node), settings.originZ());
                BlockPos pos = BlockPos.of(block);
                if (nodeBlocks.containsKey(pos)) {
                    logger.warning("Два ключа раскладки попали в один блок: " + pos);
                    continue;
                }
                originalBlocks.put(pos, block.getBlockData());
                block.setType(material, false);
                nodeBlocks.put(pos, new NodeHit(node, node.keys().get(i)));
            }
            if (!node.label().isEmpty()) {
                spawnLabel(world, node);
            }
        }

        Map<Integer, TreeLayout.Node> byId = layout.nodesById();
        for (TreeLayout.Edge edge : layout.edges()) {
            TreeLayout.Node parent = byId.get(edge.from());
            TreeLayout.Node child = byId.get(edge.to());
            if (parent != null && child != null) {
                spawnEdge(world, parent, child, edge.slot());
            }
        }
        saveLeftovers();
    }

    /** Удаляет всё нарисованное и возвращает исходные блоки. */
    void clear() {
        for (Entity entity : entities) {
            entity.remove();
        }
        entities.clear();
        for (Map.Entry<BlockPos, BlockData> entry : originalBlocks.entrySet()) {
            Block block = blockAt(entry.getKey());
            if (block != null) {
                block.setBlockData(entry.getValue(), false);
            }
        }
        originalBlocks.clear();
        nodeBlocks.clear();
        current = TreeLayout.empty();
        saveLeftovers();
    }

    NodeHit lookup(Block block) {
        return nodeBlocks.get(BlockPos.of(block));
    }

    boolean isNodeBlock(Block block) {
        return nodeBlocks.containsKey(BlockPos.of(block));
    }

    /**
     * Неперсистентные сущности пропадают при выгрузке чанка; если игрок вернулся
     * и чанк снова загружен, дерево перерисовывается.
     */
    void repairIfNeeded() {
        for (Entity entity : entities) {
            if (!entity.isValid()) {
                Location location = entity.getLocation();
                World world = location.getWorld();
                if (world != null && world.isChunkLoaded(location.getBlockX() >> 4, location.getBlockZ() >> 4)) {
                    render(current);
                    return;
                }
            }
        }
    }

    /** Убирает блоки, оставшиеся после аварийного завершения сервера. */
    void removeLeftovers() {
        if (!Files.exists(leftoversPath)) {
            return;
        }
        int removed = 0;
        try {
            for (String line : Files.readAllLines(leftoversPath, StandardCharsets.UTF_8)) {
                String[] parts = line.trim().split(" ");
                if (parts.length != 4) {
                    continue;
                }
                World world = Bukkit.getWorld(UUID.fromString(parts[0]));
                if (world == null) {
                    continue;
                }
                Block block = world.getBlockAt(Integer.parseInt(parts[1]), Integer.parseInt(parts[2]),
                        Integer.parseInt(parts[3]));
                if (NODE_MATERIALS.contains(block.getType())) {
                    block.setType(Material.AIR, false);
                    removed++;
                }
            }
            Files.delete(leftoversPath);
        } catch (IOException | IllegalArgumentException e) {
            logger.log(Level.WARNING, "Не удалось убрать блоки прошлого запуска", e);
        }
        if (removed > 0) {
            logger.info("Убрано блоков, оставшихся с прошлого запуска: " + removed);
        }
    }

    private void saveLeftovers() {
        try {
            if (originalBlocks.isEmpty()) {
                Files.deleteIfExists(leftoversPath);
                return;
            }
            List<String> lines = new ArrayList<>();
            for (BlockPos pos : originalBlocks.keySet()) {
                lines.add(pos.world() + " " + pos.x() + " " + pos.y() + " " + pos.z());
            }
            Files.createDirectories(leftoversPath.getParent());
            Files.write(leftoversPath, lines, StandardCharsets.UTF_8);
        } catch (IOException e) {
            logger.log(Level.WARNING, "Не удалось сохранить список блоков дерева", e);
        }
    }

    private void spawnLabel(World world, TreeLayout.Node node) {
        Location location = new Location(world, nodeCenterX(node), blockY(node) + 1 + LABEL_GAP,
                settings.originZ() + 0.5);
        TextDisplay label = world.spawn(location, TextDisplay.class, display -> {
            display.text(Component.text(String.join("\n", node.label())));
            display.setBillboard(Display.Billboard.CENTER);
            display.setBackgroundColor(Color.fromARGB(110, 0, 0, 0));
            display.setAlignment(TextDisplay.TextAlignment.CENTER);
            display.setViewRange(settings.viewRange());
            display.setPersistent(false);
            float scale = settings.labelScale();
            display.setTransformation(new Transformation(new Vector3f(), new Quaternionf(),
                    new Vector3f(scale, scale, scale), new Quaternionf()));
        });
        entities.add(label);
    }

    /**
     * Ребро выходит из нижней грани родителя: слот 0 — левый край первого ключа,
     * последний слот — правый край последнего, остальные — между соседними ключами.
     * Входит в верх подписи ребёнка, чтобы не перечёркивать её.
     */
    private void spawnEdge(World world, TreeLayout.Node parent, TreeLayout.Node child, int slot) {
        int keys = parent.keys().size();
        double startX;
        if (slot <= 0) {
            startX = blockX(parent, 0);
        } else if (slot >= keys) {
            startX = blockX(parent, keys - 1) + 1.0;
        } else {
            startX = (blockX(parent, slot - 1) + blockX(parent, slot)) / 2.0 + 0.5;
        }
        double startY = blockY(parent);
        double endX = nodeCenterX(child);
        double endY = blockY(child) + 1 + LABEL_GAP
                + child.label().size() * LABEL_LINE_HEIGHT * settings.labelScale();
        double z = settings.originZ() + 0.5;

        Vector3f direction = new Vector3f((float) (endX - startX), (float) (endY - startY), 0f);
        float length = direction.length();
        if (length < 1e-3f) {
            return;
        }
        direction.div(length);
        float thickness = settings.edgeThickness();
        Quaternionf rotation = new Quaternionf().rotationTo(new Vector3f(0f, 1f, 0f), direction);
        Vector3f translation = rotation.transform(new Vector3f(-thickness / 2f, 0f, -thickness / 2f));

        BlockDisplay edge = world.spawn(new Location(world, startX, startY, z), BlockDisplay.class, display -> {
            display.setBlock(settings.edgeMaterial().createBlockData());
            display.setTransformation(new Transformation(translation, rotation,
                    new Vector3f(thickness, length, thickness), new Quaternionf()));
            display.setViewRange(settings.viewRange());
            display.setPersistent(false);
        });
        entities.add(edge);
    }

    private int blockX(TreeLayout.Node node, int keyIndex) {
        return settings.originX() + (node.x() + keyIndex) * settings.spacingX();
    }

    private int blockY(TreeLayout.Node node) {
        return settings.originY() - node.depth() * settings.spacingY();
    }

    private double nodeCenterX(TreeLayout.Node node) {
        return (blockX(node, 0) + blockX(node, node.keys().size() - 1)) / 2.0 + 0.5;
    }

    private World resolveWorld() {
        World world = Bukkit.getWorld(settings.worldName());
        if (world == null && !Bukkit.getWorlds().isEmpty()) {
            world = Bukkit.getWorlds().get(0);
        }
        return world;
    }

    private static Block blockAt(BlockPos pos) {
        World world = Bukkit.getWorld(pos.world());
        return world == null ? null : world.getBlockAt(pos.x(), pos.y(), pos.z());
    }
}
