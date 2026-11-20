package me.andrey.treevisualiser;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Раскладка активного дерева из успешного ответа Backend. */
record TreeLayout(String tree, List<Node> nodes, List<Edge> edges) {

    record Node(int id, List<Long> keys, int x, int depth, String color, List<String> label,
                Map<String, String> info) {
    }

    record Edge(int from, int to, int slot) {
    }

    static TreeLayout empty() {
        return new TreeLayout(null, List.of(), List.of());
    }

    /** @throws RuntimeException если ответ не соответствует протоколу */
    static TreeLayout parse(JsonObject response) {
        String tree = response.has("tree") && !response.get("tree").isJsonNull()
                ? response.get("tree").getAsString() : null;

        List<Node> nodes = new ArrayList<>();
        for (JsonElement element : response.getAsJsonArray("nodes")) {
            JsonObject node = element.getAsJsonObject();
            List<Long> keys = new ArrayList<>();
            for (JsonElement key : node.getAsJsonArray("keys")) {
                keys.add(key.getAsLong());
            }
            if (keys.isEmpty()) {
                throw new IllegalArgumentException("узел без ключей: " + node);
            }
            Map<String, String> info = new LinkedHashMap<>();
            if (node.has("info")) {
                for (Map.Entry<String, JsonElement> entry : node.getAsJsonObject("info").entrySet()) {
                    info.put(entry.getKey(), entry.getValue().getAsString());
                }
            }
            nodes.add(new Node(
                    node.get("id").getAsInt(),
                    Collections.unmodifiableList(keys),
                    node.get("x").getAsInt(),
                    node.get("depth").getAsInt(),
                    node.has("color") ? node.get("color").getAsString() : "white",
                    node.has("label") ? strings(node.getAsJsonArray("label")) : List.of(),
                    Collections.unmodifiableMap(info)));
        }

        List<Edge> edges = new ArrayList<>();
        for (JsonElement element : response.getAsJsonArray("edges")) {
            JsonObject edge = element.getAsJsonObject();
            edges.add(new Edge(edge.get("from").getAsInt(), edge.get("to").getAsInt(),
                    edge.get("slot").getAsInt()));
        }
        return new TreeLayout(tree, List.copyOf(nodes), List.copyOf(edges));
    }

    Map<Integer, Node> nodesById() {
        Map<Integer, Node> result = new HashMap<>();
        for (Node node : nodes) {
            result.put(node.id(), node);
        }
        return result;
    }

    private static List<String> strings(JsonArray array) {
        List<String> result = new ArrayList<>();
        for (JsonElement element : array) {
            result.add(element.getAsString());
        }
        return List.copyOf(result);
    }
}
