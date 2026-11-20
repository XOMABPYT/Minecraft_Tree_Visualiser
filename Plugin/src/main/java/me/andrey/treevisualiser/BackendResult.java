package me.andrey.treevisualiser;

import com.google.gson.JsonObject;

/**
 * Итог одного запроса к Backend: либо ответ по протоколу (успешный или с ошибкой),
 * либо сбой транспорта (процесс не запустился, упал или не ответил вовремя).
 *
 * @param restarted Backend был перезапущен перед этим запросом, прежнее состояние потеряно
 */
record BackendResult(JsonObject response, String failure, boolean restarted) {

    static BackendResult response(JsonObject response, boolean restarted) {
        return new BackendResult(response, null, restarted);
    }

    static BackendResult failure(String failure, boolean restarted) {
        return new BackendResult(null, failure, restarted);
    }

    boolean isFailure() {
        return failure != null;
    }

    boolean ok() {
        return response != null && response.has("ok") && response.get("ok").getAsBoolean();
    }

    String errorCode() {
        return stringField("error", "unknown");
    }

    String errorMessage() {
        return stringField("message", "");
    }

    private String stringField(String name, String fallback) {
        if (response == null || !response.has(name) || response.get(name).isJsonNull()) {
            return fallback;
        }
        return response.get(name).getAsString();
    }
}
