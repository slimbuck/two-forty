"use strict";

const $ = (selector) => document.querySelector(selector);
let games = [];
let activeGame = "";
let editingGame = "";
let bootGame = "launcher";
let levelState = null;
let displayViewport = null;

async function api(url, options = {}) {
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

function setAction(message, error = false) {
  const element = $("#actionStatus");
  element.textContent = message;
  element.style.color = error ? "var(--red)" : "var(--muted)";
}

async function act(button, label, callback) {
  const old = button.textContent;
  button.disabled = true;
  button.textContent = `${label}…`;
  try { await callback(); setAction(`${label} complete.`); }
  catch (error) { setAction(error.message, true); }
  finally { button.disabled = false; button.textContent = old; }
}

function parseDiagnostics(text) {
  const temperature = /temp=([^\n]+)/.exec(text)?.[1] || "—";
  const throttled = /throttled=([^\n]+)/.exec(text)?.[1] || "—";
  const uptime = /up .+/.exec(text)?.[0] || "—";
  return { temperature, throttled, uptime };
}

function decodeThrottle(value) {
  const flags = Number.parseInt(String(value).replace(/^0x/, ""), 16);
  if (!Number.isFinite(flags)) return "—";
  const current = [];
  if (flags & 0x1) current.push("Undervoltage");
  if (flags & 0x2) current.push("CPU capped");
  if (flags & 0x4) current.push("Throttled");
  if (flags & 0x8) current.push("Soft temp limit");
  if (current.length) return current.join(", ");
  return "Clean";
}

async function refreshStatus() {
  try {
    const data = await api("/api/status");
    $("#statusDot").classList.toggle("online", data.online);
    $("#connectionText").textContent = data.online ? "Pi online · 192.168.137.2" : "Pi unreachable";
    $("#modePill").classList.toggle("online", data.online);
    if (!data.online) return;
    const status = data.status;
    if (Number.isInteger(status.viewport_width) && Number.isInteger(status.viewport_height))
      displayViewport = {width:status.viewport_width, height:status.viewport_height};
    activeGame = status.game || "";
    $("#modeTitle").textContent = status.mode === "game" ?
      (games.find((game) => game.id === status.game)?.name || status.game) : "Game launcher";
    $("#modePill").textContent = status.mode;
    $("#outputMode").textContent = `${status.width}×${status.height} @ ${status.refresh}Hz`;
    const diagnostics = parseDiagnostics(data.diagnostics);
    $("#temperature").textContent = diagnostics.temperature;
    $("#powerState").textContent = decodeThrottle(diagnostics.throttled);
    $("#uptime").textContent = diagnostics.uptime;
    renderGames();
  } catch {
    $("#statusDot").classList.remove("online");
    $("#connectionText").textContent = "Pi unreachable";
  }
}

function renderGames() {
  const root = $("#games");
  if (!games.length) { root.innerHTML = '<p class="hint">No game folders found.</p>'; return; }
  root.innerHTML = games.map((game) => `
    <article class="game-card ${activeGame === game.id ? "active" : ""}">
      <h3>${escapeHtml(game.name)}</h3>
      <p>${escapeHtml(game.description || "No description yet.")}</p>
      <div class="assets">${game.assets.map((asset) =>
        `<a class="asset" href="${asset.url}" target="_blank" title="${asset.size} bytes">${escapeHtml(asset.name)}</a>`).join("")}</div>
      <div class="button-row">
        <button class="button primary" data-launch="${game.id}">Launch</button>
        ${(game.editors || []).filter((editor, index, all) => all.findIndex(e => e.type === editor.type) === index).map((editor) => `<button class="button" data-level-game="${game.id}" data-level-editor="${editor.id}">${editor.type === "sprite" ? "Edit sprites & animations" : "Edit levels"}</button>`).join("")}
        <button class="button" data-edit="${game.id}">Edit settings</button>
      </div>
    </article>`).join("");
  root.querySelectorAll("[data-launch]").forEach((button) => button.addEventListener("click", () =>
    act(button, "Launching", async () => {
      await api("/api/control", { method:"POST", headers:{"Content-Type":"application/json"},
        body:JSON.stringify({ action:"launch", game:button.dataset.launch }) });
      await new Promise((resolve) => setTimeout(resolve, 250)); await refreshStatus();
    })));
  root.querySelectorAll("[data-edit]").forEach((button) => button.addEventListener("click", () => openEditor(button.dataset.edit)));
  root.querySelectorAll("[data-level-editor]").forEach((button) => button.addEventListener("click", () =>
    openLevelEditor(button.dataset.levelGame, button.dataset.levelEditor).catch(error=>setAction(error.message,true))));
}

function escapeHtml(text) {
  return String(text).replace(/[&<>"']/g, (character) => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"})[character]);
}

async function loadGames() {
  const data = await api("/api/games");
  games = data.games;
  bootGame = data.bootGame || "launcher";
  const select = $("#bootGameSelect");
  select.innerHTML = '<option value="launcher">Game launcher</option>' + games.map((game) =>
    `<option value="${game.id}">${escapeHtml(game.name)}</option>`).join("");
  select.value = bootGame;
  renderGames();
}

async function openEditor(id) {
  const data = await api(`/api/games/${encodeURIComponent(id)}/config`);
  editingGame = id;
  $("#editorTitle").textContent = `${games.find((game) => game.id === id)?.name || id} configuration`;
  $("#configEditor").value = data.text;
  $("#levelEditorPanel").classList.add("hidden");
  $("#editorPanel").classList.remove("hidden");
  $("#editorPanel").scrollIntoView({ behavior:"smooth", block:"start" });
}

async function saveConfig(deploy) {
  if (!editingGame) return;
  await api(`/api/games/${encodeURIComponent(editingGame)}/config`, {
    method:"PUT", headers:{"Content-Type":"application/json"},
    body:JSON.stringify({ text:$("#configEditor").value, deploy }),
  });
  await loadGames();
}

async function drawPpm(url) {
  const bytes = new Uint8Array(await (await fetch(url)).arrayBuffer());
  let position = 0;
  const token = () => {
    while (position < bytes.length) {
      if (bytes[position] === 35) while (position < bytes.length && bytes[position++] !== 10) {}
      else if (bytes[position] <= 32) position++;
      else break;
    }
    const start = position;
    while (position < bytes.length && bytes[position] > 32 && bytes[position] !== 35) position++;
    return new TextDecoder().decode(bytes.slice(start, position));
  };
  if (token() !== "P6") throw new Error("Unsupported snapshot format");
  const width = Number(token()); const height = Number(token()); const maximum = Number(token());
  while (bytes[position] <= 32) position++;
  if (maximum !== 255) throw new Error("Unsupported snapshot depth");
  const canvas = $("#snapshotCanvas");
  canvas.width = width; canvas.height = height;
  const context = canvas.getContext("2d");
  const image = context.createImageData(width, height);
  for (let source = position, target = 0; target < image.data.length; source += 3, target += 4) {
    image.data[target] = bytes[source]; image.data[target + 1] = bytes[source + 1];
    image.data[target + 2] = bytes[source + 2]; image.data[target + 3] = 255;
  }
  context.putImageData(image, 0, 0);
  $("#screenEmpty").classList.add("hidden");
}

$("#snapshotBtn").addEventListener("click", () => act($("#snapshotBtn"), "Capturing", async () => {
  const result = await api("/api/snapshot", { method:"POST" });
  await drawPpm(result.url);
  $("#captureName").textContent = result.name;
  $("#downloadCapture").href = result.url;
  $("#downloadCapture").classList.remove("hidden");
}));
$("#menuBtn").addEventListener("click", () => act($("#menuBtn"), "Returning", async () => {
  await api("/api/control", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({action:"menu"}) });
  await new Promise((resolve) => setTimeout(resolve, 200)); await refreshStatus();
}));
$("#reloadBtn").addEventListener("click", () => act($("#reloadBtn"), "Reloading", async () => {
  await api("/api/control", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({action:"reload"}) });
}));
$("#deployBtn").addEventListener("click", () => act($("#deployBtn"), "Deploying", async () => {
  const result = await api("/api/deploy", { method:"POST" }); setAction(result.output || "Deploy complete.");
}));
$("#restartBtn").addEventListener("click", () => act($("#restartBtn"), "Restarting", async () => {
  await api("/api/restart", { method:"POST" }); await new Promise((resolve) => setTimeout(resolve, 900)); await refreshStatus();
}));
$("#poweroffBtn").addEventListener("click", async () => {
  if (!confirm("Power down the Two Forty Pi?\n\nThe CRT output and dashboard connection will stop. A physical power cycle is required to start it again.")) return;
  await act($("#poweroffBtn"), "Powering down", async () => {
    await api("/api/control", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({action:"poweroff"}) });
    $("#connectionText").textContent = "Power-down requested";
  });
});
$("#bootGameSelect").addEventListener("change", async (event) => {
  const chosen = event.target.value;
  try {
    const result = await api("/api/boot", { method:"PUT", headers:{"Content-Type":"application/json"}, body:JSON.stringify({game:chosen}) });
    bootGame = result.bootGame;
    setAction(`Next boot will start ${chosen === "launcher" ? "the launcher" : games.find((game) => game.id === chosen)?.name || chosen}.`);
  } catch (error) {
    event.target.value = bootGame;
    setAction(error.message, true);
  }
});
$("#closeEditor").addEventListener("click", () => $("#editorPanel").classList.add("hidden"));
$("#saveLocalBtn").addEventListener("click", () => act($("#saveLocalBtn"), "Saving", () => saveConfig(false)));
$("#saveReloadBtn").addEventListener("click", () => act($("#saveReloadBtn"), "Saving", () => saveConfig(true)));
$("#refreshLogsBtn").addEventListener("click", async () => {
  try { $("#logs").textContent = (await api("/api/logs")).logs || "The host log is empty."; }
  catch (error) { $("#logs").textContent = error.message; }
});

(async function init() {
  try { await loadGames(); await refreshStatus(); }
  catch (error) { setAction(error.message, true); }
  setInterval(refreshStatus, 4000);
})();
