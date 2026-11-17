const $ = (id) => document.getElementById(id);

function escapeHTML(value) {
    return String(value ?? "").replace(/[&<>"']/g, ch => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;"
    })[ch]);
}

function numberMatch(text, regex) {
    const match = text.match(regex);
    return match ? match[1].replace(/,/g, "") : null;
}

function parseAnalysis(raw) {
    const text = raw || "";

    const records = numberMatch(text, /Dataset records:\s*([\d,]+)/i)
        || numberMatch(text, /Total records processed:\s*([\d,]+)/i);
    const workers = numberMatch(text, /Active workers:\s*(\d+)/i);
    const total = numberMatch(text, /Total marks:\s*([\d,]+)/i);
    const average = numberMatch(text, /Average marks:\s*([\d.]+)/i);
    const min = numberMatch(text, /Minimum marks:\s*([\d.]+)/i);
    const max = numberMatch(text, /Maximum marks:\s*([\d.]+)/i);
    const duration = numberMatch(text, /Analysis execution time:\s*([\d.]+)\s*ms/i);

    const workerMap = {};
    const pidRegex = /\[Manager\]\s+Worker\s+(\d+)\s+created\.\s+PID:\s*(\d+)/g;
    let match;
    while ((match = pidRegex.exec(text)) !== null) {
        workerMap[match[1]] = { id: match[1], pid: match[2] };
    }

    const rangeRegex = /\[Worker\s+(\d+)\]\s+Processing records\s+([\d,]+)\s+to\s+([\d,]+)/g;
    while ((match = rangeRegex.exec(text)) !== null) {
        const id = match[1];
        workerMap[id] ||= { id };
        workerMap[id].start = Number(match[2].replace(/,/g, ""));
        workerMap[id].end = Number(match[3].replace(/,/g, ""));
    }

    const taskMap = {};
    const taskRegex = /Task\s+(\d+)\s*\|\s*Records\s+([\d,]+)-([\d,]+)\s*\|\s*Status:\s*(\w+)\s*\|\s*Retries:\s*(\d+)/g;
    while ((match = taskRegex.exec(text)) !== null) {
        taskMap[match[1]] = {
            id: match[1],
            start: Number(match[2].replace(/,/g, "")),
            end: Number(match[3].replace(/,/g, "")),
            status: match[4],
            retries: match[5]
        };
    }

    return {
        records, workers, total, average, min, max, duration,
        workerList: Object.values(workerMap).sort((a,b) => Number(a.id)-Number(b.id)),
        taskList: Object.values(taskMap).sort((a,b) => Number(a.id)-Number(b.id)),
        successful: text.includes("SUCCESS: Dataset analysis completed.")
    };
}

function showAnalysis(raw) {
    $("output").textContent = raw || "No execution logs available.";

    const data = parseAnalysis(raw);
    $("recordsProcessed").textContent = data.records ? Number(data.records).toLocaleString() : "—";
    $("totalMarks").textContent = data.total ? Number(data.total).toLocaleString() : "—";
    $("averageMarks").textContent = data.average || "—";
    $("executionTime").textContent = data.duration ? `${data.duration} ms` : "—";
    $("workerCount").textContent = data.workers || (data.workerList.length || "—");

    const status = $("analysisStatus");
    status.textContent = data.successful ? "COMPLETED" : "LATEST RESULT";
    status.className = data.successful ? "result-status success" : "result-status";

    $("workerSummary").textContent = `${data.workerList.length} WORKERS`;
    $("taskSummary").textContent = `${data.taskList.length} TASKS`;

    if (data.workerList.length) {
        const totalRecords = Number(data.records || 0);
        $("workerGrid").innerHTML = data.workerList.map(worker => {
            const count = worker.start && worker.end ? worker.end - worker.start + 1 : 0;
            const pct = totalRecords ? Math.min(100, count / totalRecords * 100) : 0;
            return `
                <article class="worker-card">
                    <div class="worker-head">
                        <div class="worker-name">Worker ${escapeHTML(worker.id)}</div>
                        <span class="worker-state">COMPLETED</span>
                    </div>
                    <div class="worker-pid">Process ID: ${escapeHTML(worker.pid || "—")}</div>
                    <div class="worker-range">Records ${worker.start?.toLocaleString() || "—"} – ${worker.end?.toLocaleString() || "—"}</div>
                    <div class="worker-track"><div class="worker-fill" style="width:${pct}%"></div></div>
                    <div class="worker-foot"><span>${count.toLocaleString()} records</span><span>${pct.toFixed(1)}% of dataset</span></div>
                </article>`;
        }).join("");
    } else {
        $("workerGrid").innerHTML = '<div class="empty-state">Worker information will appear after analysis.</div>';
    }

    if (data.taskList.length) {
        $("taskTable").innerHTML = data.taskList.map(task => {
            const worker = data.workerList.find(w => w.start === task.start && w.end === task.end);
            return `<tr>
                <td class="task-id">Task ${escapeHTML(task.id)}</td>
                <td>${task.start.toLocaleString()} – ${task.end.toLocaleString()}</td>
                <td>${(task.end-task.start+1).toLocaleString()}</td>
                <td>${worker ? `Worker ${escapeHTML(worker.id)}` : "—"}</td>
                <td><span class="table-status">${escapeHTML(task.status)}</span></td>
            </tr>`;
        }).join("");
    } else {
        $("taskTable").innerHTML = '<tr><td colspan="5" class="empty-cell">No task data available.</td></tr>';
    }
}

async function checkStatus() {
    const button = $("statusBtn");
    button.disabled = true;
    try {
        const response = await fetch("/api/status");
        const data = await response.json();
        const online = response.ok && data.connected;

        $("managerStatus").textContent = online ? "Running" : "Offline";
        $("managerNote").textContent = online ? "Manager responding normally" : (data.error || "Manager unavailable");
        $("sidebarConnection").textContent = online ? "Manager connected" : "Manager disconnected";
        $("sidebarDot").className = online ? "status-dot online" : "status-dot offline";
        $("connectionBadge").innerHTML = `<span class="status-dot ${online ? "online" : "offline"}"></span><span>${online ? "Manager connected" : "Manager offline"}</span>`;
    } catch (error) {
        $("managerStatus").textContent = "Offline";
        $("managerNote").textContent = "Unable to reach manager";
        $("sidebarConnection").textContent = "Manager disconnected";
        $("sidebarDot").className = "status-dot offline";
        $("connectionBadge").innerHTML = '<span class="status-dot offline"></span><span>Manager offline</span>';
    } finally {
        button.disabled = false;
    }
}

async function runAnalysis() {
    const button = $("analyzeBtn");
    button.disabled = true;
    button.innerHTML = "◌ Running analysis...";
    $("analysisStatus").textContent = "PROCESSING";

    try {
        const response = await fetch("/api/analyze", { method: "POST" });
        const data = await response.json();
        const raw = data.response || data.error || "No response received.";
        showAnalysis(raw);

        $("lastExecution").textContent = data.status || (response.ok ? "COMPLETED" : "FAILED");
        $("lastExecutionNote").textContent = data.timestamp || "Just now";
        $("logDetails").open = true;
        await loadHistory();
    } catch (error) {
        $("output").textContent = `Request failed: ${error.message}`;
        $("analysisStatus").textContent = "REQUEST FAILED";
    } finally {
        button.disabled = false;
        button.innerHTML = "<span>▶</span> Run analysis";
    }
}


function historySummary(raw, error) {
    if (!raw) return error || "No output recorded";

    const records = numberMatch(raw, /Total records processed:\s*([\d,]+)/i)
        || numberMatch(raw, /Dataset records:\s*([\d,]+)/i);
    const workers = numberMatch(raw, /Active workers:\s*(\d+)/i);
    const average = numberMatch(raw, /Average marks:\s*([\d.]+)/i);
    const duration = numberMatch(raw, /Analysis execution time:\s*([\d.]+)\s*ms/i);

    const parts = [];
    if (records) parts.push(`${Number(records).toLocaleString()} records`);
    if (workers) parts.push(`${workers} workers`);
    if (average) parts.push(`Average: ${average}`);
    if (duration) parts.push(`${duration} ms`);

    return parts.length ? parts.join(" · ") : raw.replace(/\s+/g, " ").slice(0, 120);
}

async function loadHistory() {
    try {
        const response = await fetch("/api/history");
        const history = await response.json();
        $("runCount").textContent = history.length;

        if (!history.length) {
            $("historyList").innerHTML = '<div class="empty-state">No analysis history yet.</div>';
            return;
        }

        const latest = history[0];
        $("lastExecution").textContent = latest.status || "—";
        $("lastExecutionNote").textContent = latest.timestamp || "Latest recorded run";

        if (latest.response) showAnalysis(latest.response);

        $("historyList").innerHTML = history.slice(0, 10).map((entry, index) => {
            const failed = entry.status !== "COMPLETED";
            const summary = historySummary(entry.response, entry.error);
            return `
                <article class="history-item">
                    <div class="history-main">
                        <div class="history-title">Analysis run ${history.length - index}</div>
                        <div class="history-time">${escapeHTML(entry.timestamp || "Unknown time")}</div>
                        <div class="history-detail">${escapeHTML(summary)}</div>
                    </div>
                    <span class="history-badge ${failed ? "failed" : ""}">${escapeHTML(entry.status || "UNKNOWN")}</span>
                </article>`;
        }).join("");
    } catch (error) {
        $("historyList").innerHTML = '<div class="empty-state">Unable to load analysis history.</div>';
    }
}


function datasetLabel(path) {
    const labels = {
        "data/dataset.csv": "15 records",
        "data/dataset_1000.csv": "1,000 records",
        "data/dataset_5000.csv": "5,000 records",
        "data/dataset_10000.csv": "10,000 records"
    };
    return labels[path] || path;
}

async function loadConfiguration() {
    const feedback = $("configFeedback");

    try {
        const response = await fetch("/api/config");
        const data = await response.json();

        if (!response.ok || !data.success) {
            throw new Error(data.error || "Unable to load configuration.");
        }

        $("datasetSelect").value = data.dataset;
        $("workersSelect").value = String(data.workers);

        $("activeConfig").textContent =
            `Active configuration: ${datasetLabel(data.dataset)} · ${data.workers} worker${data.workers === 1 ? "" : "s"}`;

        feedback.textContent = "Current configuration loaded from Manager.";
        feedback.className = "config-feedback";
    } catch (error) {
        $("activeConfig").textContent = "Active configuration: unavailable";
        feedback.textContent = error.message;
        feedback.className = "config-feedback error";
    }
}

async function applyConfiguration() {
    const button = $("applyConfigBtn");
    const analyzeButton = $("analyzeBtn");
    const statusButton = $("statusBtn");
    const feedback = $("configFeedback");

    button.disabled = true;
    analyzeButton.disabled = true;
    statusButton.disabled = true;
    button.textContent = "Applying...";

    try {
        const response = await fetch("/api/config", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                dataset: $("datasetSelect").value,
                workers: Number($("workersSelect").value)
            })
        });

        const data = await response.json();

        if (!response.ok || !data.success) {
            throw new Error(data.error || "Configuration update failed.");
        }

        $("activeConfig").textContent =
            `Active configuration: ${datasetLabel(data.dataset)} · ${data.workers} worker${data.workers === 1 ? "" : "s"}`;

        feedback.textContent = "Configuration successfully updated. It will be used for the next analysis.";
        feedback.className = "config-feedback success";
        $("workerCount").textContent = data.workers;
    } catch (error) {
        feedback.textContent = error.message;
        feedback.className = "config-feedback error";
        await loadConfiguration();
    } finally {
        button.disabled = false;
        analyzeButton.disabled = false;
        statusButton.disabled = false;
        button.textContent = "Apply configuration";
    }
}

checkStatus();
loadHistory();
loadConfiguration();
