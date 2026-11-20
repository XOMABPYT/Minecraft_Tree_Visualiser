package me.andrey.treevisualiser;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import org.bukkit.Bukkit;
import org.bukkit.plugin.java.JavaPlugin;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Дочерний процесс Backend и обмен с ним строками JSON.
 *
 * <p>Все запросы выполняет один поток ввода-вывода строго по очереди: следующий
 * запрос отправляется только после ответа на предыдущий. Колбэки вызываются
 * в главном потоке сервера. Если процесс упал или не ответил за таймаут, он
 * останавливается и запускается заново при следующем запросе.
 */
final class BackendProcess {

    private record Pending(JsonObject request, Consumer<BackendResult> callback) {
    }

    private static final Pending SHUTDOWN = new Pending(new JsonObject(), result -> {
    });
    /** Маркер конца stdout процесса; сравнивается по ссылке. */
    @SuppressWarnings("StringOperationCanBeSimplified")
    private static final String EOF = new String("<eof>");

    private final JavaPlugin plugin;
    private final Logger logger;
    private final PluginSettings settings;
    private final Gson gson = new Gson();
    private final BlockingQueue<Pending> queue = new LinkedBlockingQueue<>();
    private volatile boolean running;
    private Thread ioThread;

    // Поля ниже используются только потоком ввода-вывода.
    private Process process;
    private BufferedWriter stdin;
    private BlockingQueue<String> stdoutLines;
    private boolean everStarted;
    private long nextId = 1;

    BackendProcess(JavaPlugin plugin, PluginSettings settings) {
        this.plugin = plugin;
        this.logger = plugin.getLogger();
        this.settings = settings;
    }

    void start() {
        running = true;
        ioThread = new Thread(this::ioLoop, "TreeVisualiser-Backend-IO");
        ioThread.setDaemon(true);
        ioThread.start();
    }

    /** Ставит запрос в очередь; поле id проставляется автоматически. */
    void send(JsonObject request, Consumer<BackendResult> callback) {
        if (!running) {
            deliver(callback, BackendResult.failure("плагин выключается", false));
            return;
        }
        queue.add(new Pending(request.deepCopy(), callback));
    }

    void shutdown() {
        running = false;
        queue.add(SHUTDOWN);
        if (ioThread != null) {
            try {
                ioThread.join(5000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private void ioLoop() {
        try {
            while (true) {
                Pending pending = queue.take();
                if (pending == SHUTDOWN) {
                    break;
                }
                deliver(pending.callback(), execute(pending.request()));
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } finally {
            stopProcess(true);
            List<Pending> rest = new ArrayList<>();
            queue.drainTo(rest);
            for (Pending pending : rest) {
                deliver(pending.callback(), BackendResult.failure("плагин выключается", false));
            }
        }
    }

    private BackendResult execute(JsonObject request) throws InterruptedException {
        boolean restarted = false;
        if (process == null || !process.isAlive()) {
            restarted = everStarted;
            String error = launch();
            if (error != null) {
                return BackendResult.failure(error, restarted);
            }
        }

        long id = nextId++;
        request.addProperty("id", id);
        try {
            stdin.write(gson.toJson(request));
            stdin.write('\n');
            stdin.flush();
        } catch (IOException e) {
            stopProcess(false);
            return BackendResult.failure("не удалось отправить запрос Backend: " + e.getMessage(), restarted);
        }

        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(settings.backendTimeoutMs());
        while (true) {
            long left = deadline - System.nanoTime();
            if (left <= 0) {
                stopProcess(false);
                return BackendResult.failure("Backend не ответил за " + settings.backendTimeoutMs()
                        + " мс и был остановлен", restarted);
            }
            String line = stdoutLines.poll(left, TimeUnit.NANOSECONDS);
            if (line == null) {
                continue;
            }
            if (line == EOF) {
                int exitCode = waitForExit();
                stopProcess(false);
                return BackendResult.failure("Backend аварийно завершился (код " + exitCode + ")", restarted);
            }
            JsonObject response = parseResponse(line);
            if (response == null) {
                continue;
            }
            JsonElement responseId = response.get("id");
            boolean matches = responseId == null || responseId.isJsonNull()
                    || (responseId.isJsonPrimitive() && responseId.getAsJsonPrimitive().isNumber()
                    && responseId.getAsLong() == id);
            if (!matches) {
                logger.warning("Пропущен ответ Backend с чужим id: " + line);
                continue;
            }
            return BackendResult.response(response, restarted);
        }
    }

    private JsonObject parseResponse(String line) {
        try {
            JsonElement element = JsonParser.parseString(line);
            if (element.isJsonObject()) {
                return element.getAsJsonObject();
            }
        } catch (JsonParseException e) {
            // ниже
        }
        logger.warning("Backend прислал не JSON-объект: " + line);
        return null;
    }

    /** @return текст ошибки или null, если процесс запущен */
    private String launch() {
        File executable = new File(settings.backendPath()).getAbsoluteFile().toPath().normalize().toFile();
        if (!executable.isFile() || !executable.canExecute()) {
            return "не найден исполняемый файл Backend: " + executable.getAbsolutePath()
                    + " (соберите Backend через CMake или поправьте backend.path в config.yml)";
        }

        List<String> command = new ArrayList<>();
        command.add(executable.getAbsolutePath());
        if (settings.backendSeed() >= 0) {
            command.add("--seed");
            command.add(Long.toString(settings.backendSeed()));
        }
        try {
            process = new ProcessBuilder(command).start();
        } catch (IOException e) {
            process = null;
            return "не удалось запустить Backend: " + e.getMessage();
        }
        everStarted = true;
        stdin = new BufferedWriter(new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8));
        stdoutLines = new LinkedBlockingQueue<>();
        startReader("TreeVisualiser-Backend-stdout", process.getInputStream(), stdoutLines::add, stdoutLines);
        startReader("TreeVisualiser-Backend-stderr", process.getErrorStream(),
                line -> logger.info("[backend] " + line), null);
        logger.info("Backend запущен: " + String.join(" ", command));
        return null;
    }

    private void startReader(String name, InputStream stream, Consumer<String> sink, BlockingQueue<String> eofQueue) {
        Thread thread = new Thread(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    sink.accept(line);
                }
            } catch (IOException e) {
                // Поток закрыт при остановке процесса.
            } finally {
                if (eofQueue != null) {
                    eofQueue.add(EOF);
                }
            }
        }, name);
        thread.setDaemon(true);
        thread.start();
    }

    private int waitForExit() {
        try {
            if (process.waitFor(1, TimeUnit.SECONDS)) {
                return process.exitValue();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        return -1;
    }

    /** @param graceful сначала закрыть stdin и дать процессу завершиться самому */
    private void stopProcess(boolean graceful) {
        if (process == null) {
            return;
        }
        try {
            stdin.close();
        } catch (IOException e) {
            // процесс уже мог завершиться
        }
        try {
            if (!graceful || !process.waitFor(2, TimeUnit.SECONDS)) {
                process.destroy();
                if (!process.waitFor(2, TimeUnit.SECONDS)) {
                    process.destroyForcibly();
                }
            }
        } catch (InterruptedException e) {
            process.destroyForcibly();
            Thread.currentThread().interrupt();
        }
        process = null;
        stdin = null;
    }

    private void deliver(Consumer<BackendResult> callback, BackendResult result) {
        if (!plugin.isEnabled()) {
            return;
        }
        try {
            Bukkit.getScheduler().runTask(plugin, () -> callback.accept(result));
        } catch (RuntimeException e) {
            logger.log(Level.FINE, "Не удалось передать ответ Backend в главный поток", e);
        }
    }
}
