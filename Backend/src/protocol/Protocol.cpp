#include "protocol/Protocol.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>

#include "layout/Layout.h"

namespace {

using nlohmann::json;
using nlohmann::ordered_json;

const char* const kColors[] = {"white", "red", "black", "gray", "blue", "green", "yellow"};

std::string normalizeColor(const std::string& color) {
    for (const char* allowed : kColors) {
        if (color == allowed) {
            return color;
        }
    }
    std::cerr << "[warn] неизвестный цвет узла '" << color << "', используется white" << std::endl;
    return "white";
}

std::optional<std::int64_t> readInt64(const json& request, const char* field) {
    const auto it = request.find(field);
    if (it == request.end() || !it->is_number_integer()) {
        return std::nullopt;
    }
    if (it->is_number_unsigned()) {
        const auto value = it->get<std::uint64_t>();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    }
    return it->get<std::int64_t>();
}

ordered_json badRequest(const json& id, const std::string& message) {
    return buildErrorResponse(id, ErrorCode::BadRequest, message);
}

ordered_json respond(const World& world, const json& id, const OpResult& result) {
    if (!result.ok()) {
        return buildErrorResponse(id, result.error, result.message);
    }
    return buildLayoutResponse(world, id);
}

ordered_json dispatch(World& world, const json& request, const json& id) {
    const auto cmdIt = request.find("cmd");
    if (cmdIt == request.end() || !cmdIt->is_string()) {
        return badRequest(id, "Field 'cmd' must be a string");
    }
    const std::string cmd = cmdIt->get<std::string>();

    if (cmd == "insert" || cmd == "delete") {
        const auto key = readInt64(request, "key");
        if (!key) {
            return badRequest(id, "Field 'key' must be a 64-bit integer");
        }
        return respond(world, id, cmd == "insert" ? world.insert(*key) : world.erase(*key));
    }

    if (cmd == "random") {
        const auto count = readInt64(request, "count");
        const auto min = readInt64(request, "min");
        const auto max = readInt64(request, "max");
        if (!count || !min || !max) {
            return badRequest(id, "Fields 'count', 'min' and 'max' must be 64-bit integers");
        }
        const OpResult result = world.insertRandom(*count, *min, *max);
        ordered_json response = respond(world, id, result);
        if (result.ok()) {
            response["inserted"] = result.inserted;
        }
        return response;
    }

    if (cmd == "view") {
        const auto treeIt = request.find("tree");
        if (treeIt == request.end() || !treeIt->is_string()) {
            return badRequest(id, "Field 'tree' must be a string");
        }
        return respond(world, id, world.view(treeIt->get<std::string>()));
    }

    if (cmd == "clear") {
        world.clear();
        return buildLayoutResponse(world, id);
    }

    if (cmd == "state") {
        return buildLayoutResponse(world, id);
    }

    return badRequest(id, "Unknown command '" + cmd + "'");
}

}  // namespace

ordered_json buildLayoutResponse(const World& world, const json& id) {
    const TreeSnapshot snapshot = world.activeSnapshot();
    const TreeLayout layout = computeLayout(snapshot, world.activeLayoutKind());

    ordered_json nodes = ordered_json::array();
    for (const auto& placed : layout.nodes) {
        const auto& node = snapshot.nodes[static_cast<std::size_t>(placed.node)];
        ordered_json info = ordered_json::object();
        for (const auto& [name, value] : node.info) {
            info[name] = value;
        }
        ordered_json item;
        item["id"] = placed.node;
        item["keys"] = node.keys;
        item["x"] = placed.x;
        item["depth"] = placed.depth;
        item["color"] = normalizeColor(node.color);
        item["label"] = node.label;
        item["info"] = std::move(info);
        nodes.push_back(std::move(item));
    }

    ordered_json edges = ordered_json::array();
    for (const auto& edge : layout.edges) {
        edges.push_back({{"from", edge.from}, {"to", edge.to}, {"slot", edge.slot}});
    }

    ordered_json response;
    response["id"] = id;
    response["ok"] = true;
    if (world.activeTreeId().empty()) {
        response["tree"] = nullptr;
    } else {
        response["tree"] = world.activeTreeId();
    }
    response["nodes"] = std::move(nodes);
    response["edges"] = std::move(edges);
    return response;
}

ordered_json buildErrorResponse(const json& id, ErrorCode code, const std::string& message) {
    ordered_json response;
    response["id"] = id;
    response["ok"] = false;
    response["error"] = errorCodeName(code);
    response["message"] = message;
    return response;
}

ordered_json handleRequest(World& world, const json& request) {
    if (!request.is_object()) {
        return badRequest(nullptr, "Request must be a JSON object");
    }
    const json id = request.contains("id") ? request["id"] : json(nullptr);
    try {
        return dispatch(world, request, id);
    } catch (const std::exception& e) {
        std::cerr << "[error] исключение при обработке запроса: " << e.what() << std::endl;
        return badRequest(id, std::string("Internal error: ") + e.what());
    }
}

std::string handleRequestLine(World& world, const std::string& line) {
    const json request = json::parse(line, nullptr, false);
    const ordered_json response = request.is_discarded()
                                      ? badRequest(nullptr, "Malformed JSON")
                                      : handleRequest(world, request);
    return response.dump(-1, ' ', false, json::error_handler_t::replace);
}
