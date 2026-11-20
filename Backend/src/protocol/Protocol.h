#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "world/World.h"

// Обрабатывает один запрос и возвращает ответ. Не бросает исключений:
// любой некорректный запрос превращается в ответ с ошибкой bad_request.
nlohmann::ordered_json handleRequest(World& world, const nlohmann::json& request);

// То же для строки JSON; результат — одна строка без перевода строки.
std::string handleRequestLine(World& world, const std::string& line);

// Ответ с полной раскладкой активного дерева.
nlohmann::ordered_json buildLayoutResponse(const World& world, const nlohmann::json& id);

nlohmann::ordered_json buildErrorResponse(const nlohmann::json& id, ErrorCode code,
                                          const std::string& message);
