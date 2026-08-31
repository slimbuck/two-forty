"use strict";

const $ = (selector) => document.querySelector(selector);
let games = [];
let activeGame = "";
let editingGame = "";
let bootGame = "launcher";
let levelState = null;

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
        ${(game.editors || []).map((editor) => `<button class="button" data-level-game="${game.id}" data-level-editor="${editor.id}">Edit ${escapeHtml(editor.name)}</button>`).join("")}
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
    openLevelEditor(button.dataset.levelGame, button.dataset.levelEditor)));
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

function parseLevelText(text) {
  const lines = text.replace(/\r/g, "").split("\n");
  const comments = lines.filter((line) => line.startsWith("# "));
  const sourceRows = lines.filter((line) => line && !line.startsWith("# "));
  const width = Math.max(1, ...sourceRows.map((row) => [...row].length));
  const empty = levelState.editor.empty;
  const rows = (sourceRows.length ? sourceRows : [empty]).map((row) =>
    [...row].concat(Array(width).fill(empty)).slice(0, width));
  return { comments, rows };
}

function levelText() {
  const body = levelState.rows.map((row) => row.join("")).join("\n");
  return `${levelState.comments.join("\n")}${levelState.comments.length ? "\n" : ""}${body}\n`;
}

function copyRows(rows = levelState.rows) {
  return rows.map((row) => [...row]);
}

function restoreRows(rows) {
  levelState.rows = copyRows(rows);
  renderLevel();
}

function checkpointLevel() {
  levelState.undo.push(copyRows());
  if (levelState.undo.length > 100) levelState.undo.shift();
  levelState.redo = [];
}

function changeHistory(from, to) {
  if (!levelState || !levelState[from].length) return;
  levelState[to].push(copyRows());
  restoreRows(levelState[from].pop());
}

function validateLevel() {
  const errors = [];
  const palette = new Map(levelState.editor.palette.map((tile) => [tile.value, tile]));
  const counts = Object.fromEntries(levelState.editor.palette.map((tile) => [tile.value, 0]));
  levelState.rows.forEach((row, y) => row.forEach((value, x) => {
    if (!palette.has(value)) errors.push(`Unknown tile ${JSON.stringify(value)} at ${x + 1}, ${y + 1}.`);
    else counts[value]++;
  }));
  levelState.editor.palette.forEach((tile) => {
    if (tile.minimum !== undefined && counts[tile.value] < tile.minimum)
      errors.push(`${tile.name} requires at least ${tile.minimum}; found ${counts[tile.value]}.`);
    if (tile.maximum !== undefined && counts[tile.value] > tile.maximum)
      errors.push(`${tile.name} allows at most ${tile.maximum}; found ${counts[tile.value]}.`);
  });
  return errors;
}

function updateLevelState() {
  if (!levelState) return;
  const errors = validateLevel();
  const validation = $("#levelValidation");
  validation.textContent = errors.length ? errors[0] :
    `${levelState.editor.type === "sprite" ? "Sprite" : "Level"} is valid`;
  validation.title = errors.join("\n");
  validation.classList.toggle("invalid", errors.length > 0);
  $("#levelDirty").textContent = levelText() === levelState.savedText ?
    "No unsaved changes." : "Unsaved changes.";
  $("#levelWidth").value = levelState.rows[0].length;
  $("#levelHeight").value = levelState.rows.length;
  $("#undoLevel").disabled = !levelState.undo.length;
  $("#redoLevel").disabled = !levelState.redo.length;
  $("#saveLevelLocal").disabled = errors.length > 0;
  $("#saveLevelPlay").disabled = errors.length > 0;
}

function renderLevel() {
  if (!levelState) return;
  const canvas = $("#levelCanvas");
  const cell = levelState.cell;
  const width = levelState.rows[0].length;
  const height = levelState.rows.length;
  canvas.width = width * cell;
  canvas.height = height * cell;
  const context = canvas.getContext("2d");
  const palette = new Map(levelState.editor.palette.map((tile) => [tile.value, tile]));
  context.textAlign = "center";
  context.textBaseline = "middle";
  context.font = `700 ${Math.max(8, cell - 7)}px ui-monospace, monospace`;
  levelState.rows.forEach((row, y) => row.forEach((value, x) => {
    const tile = palette.get(value);
    context.fillStyle = tile?.color || "#ff00ff";
    context.fillRect(x * cell, y * cell, cell, cell);
    if (value !== levelState.editor.empty && cell >= 16) {
      context.fillStyle = "rgba(0,0,0,.68)";
      context.fillText(value, x * cell + cell / 2, y * cell + cell / 2 + .5);
    }
  }));
  if (cell >= 12) {
    context.beginPath();
    for (let x = 0; x <= width; x++) { context.moveTo(x * cell + .5, 0); context.lineTo(x * cell + .5, height * cell); }
    for (let y = 0; y <= height; y++) { context.moveTo(0, y * cell + .5); context.lineTo(width * cell, y * cell + .5); }
    context.strokeStyle = "rgba(255,255,255,.08)";
    context.lineWidth = 1;
    context.stroke();
  }
  const viewport = levelState.editor.viewport;
  if (viewport) {
    context.save();
    context.setLineDash([7, 5]);
    context.strokeStyle = "rgba(243,184,69,.9)";
    context.lineWidth = 2;
    for (let x = viewport.width; x < width; x += viewport.width) {
      context.beginPath(); context.moveTo(x * cell, 0); context.lineTo(x * cell, height * cell); context.stroke();
    }
    for (let y = viewport.height; y < height; y += viewport.height) {
      context.beginPath(); context.moveTo(0, y * cell); context.lineTo(width * cell, y * cell); context.stroke();
    }
    context.restore();
  }
  if (levelState.editor.type === "sprite") renderSpritePreview(palette);
  updateLevelState();
}

function renderSpritePreview(palette) {
  const canvas = $("#spritePreview");
  const width = levelState.rows[0].length, height = levelState.rows.length;
  canvas.width = width; canvas.height = height;
  const scale = Math.max(1, Math.min(8, Math.floor(128 / width), Math.floor(112 / height)));
  canvas.style.width = `${width * scale}px`;
  canvas.style.height = `${height * scale}px`;
  const context = canvas.getContext("2d");
  context.clearRect(0, 0, width, height);
  levelState.rows.forEach((row, y) => row.forEach((value, x) => {
    if (value === levelState.editor.empty) return;
    context.fillStyle = palette.get(value)?.color || "#ff00ff";
    context.fillRect(x, y, 1, 1);
  }));
  $("#spritePreviewSize").textContent = `${width}×${height} native pixels`;
}

function renderLevelPalette() {
  const palette = $("#tilePalette");
  palette.innerHTML = levelState.editor.palette.map((tile) => `
    <button class="tile-choice ${tile.value === levelState.tile ? "active" : ""}" data-tile="${escapeHtml(tile.value)}">
      <span class="tile-swatch" style="background:${tile.color}">${escapeHtml(tile.value)}</span><span class="tile-name">${escapeHtml(tile.name)}</span>
    </button>`).join("");
  palette.querySelectorAll("[data-tile]").forEach((button) => button.addEventListener("click", () => {
    levelState.tile = button.dataset.tile;
    if (levelState.tool === "erase") selectLevelTool("pencil");
    renderLevelPalette();
  }));
}

function selectLevelTool(tool) {
  levelState.tool = tool;
  $("#levelTools").querySelectorAll("[data-tool]").forEach((button) =>
    button.classList.toggle("active", button.dataset.tool === tool));
}

async function openLevelEditor(gameId, editorId) {
  if (levelState && levelText() !== levelState.savedText &&
      !confirm("Discard the unsaved level changes?")) return;
  const data = await api(`/api/games/${encodeURIComponent(gameId)}/editors/${encodeURIComponent(editorId)}`);
  const isSprite = data.editor.type === "sprite";
  levelState = { gameId, editorId, editor:data.editor, hash:data.hash, savedText:data.text,
    comments:[], rows:[], cell:isSprite ? 32 : 16, tile:data.editor.palette.find((tile) => tile.value !== data.editor.empty)?.value || data.editor.empty,
    tool:"pencil", undo:[], redo:[], drawing:false, start:null, last:null };
  Object.assign(levelState, parseLevelText(data.text));
  $("#editorPanel").classList.add("hidden");
  $("#levelEditorTitle").textContent = `${games.find((game) => game.id === gameId)?.name || gameId} · ${data.editor.name}`;
  $("#gridEditorKind").textContent = isSprite ? "Pixel sprite editor" : "Tilemap editor";
  $("#gridPaletteTitle").textContent = isSprite ? "Palette" : "Tiles";
  $("#gridSizeTitle").textContent = isSprite ? "Image size" : "Map size";
  $("#gridEditorHelp").textContent = isSprite ?
    "Transparent pixels show the checkerboard behind the native preview." :
    "Each dashed frame is one 320×240 screen.";
  $("#spritePreviewPanel").classList.toggle("hidden", !isSprite);
  $("#flipSprite").classList.toggle("hidden", !isSprite);
  $("#levelCanvas").setAttribute("aria-label", isSprite ? "Editable pixel sprite" : "Editable level tilemap");
  $("#levelCanvasScroll").classList.toggle("sprite", isSprite);
  $("#levelWidth").max = isSprite ? "128" : "512";
  $("#levelHeight").max = isSprite ? "128" : "512";
  $("#levelZoom").value = String(levelState.cell);
  selectLevelTool("pencil");
  renderLevelPalette();
  renderLevel();
  $("#levelEditorPanel").classList.remove("hidden");
  $("#levelEditorPanel").scrollIntoView({ behavior:"smooth", block:"start" });
}

function cellAt(event) {
  const canvas = $("#levelCanvas");
  const bounds = canvas.getBoundingClientRect();
  return {
    x: Math.floor((event.clientX - bounds.left) * canvas.width / bounds.width / levelState.cell),
    y: Math.floor((event.clientY - bounds.top) * canvas.height / bounds.height / levelState.cell),
  };
}

function insideLevel(point) {
  return point.x >= 0 && point.y >= 0 && point.y < levelState.rows.length && point.x < levelState.rows[0].length;
}

function paintCell(point, value) {
  if (insideLevel(point)) levelState.rows[point.y][point.x] = value;
}

function paintLine(from, to, value) {
  let x = from.x, y = from.y;
  const dx = Math.abs(to.x - x), sx = x < to.x ? 1 : -1;
  const dy = -Math.abs(to.y - y), sy = y < to.y ? 1 : -1;
  let error = dx + dy;
  while (true) {
    paintCell({ x, y }, value);
    if (x === to.x && y === to.y) break;
    const doubled = 2 * error;
    if (doubled >= dy) { error += dy; x += sx; }
    if (doubled <= dx) { error += dx; y += sy; }
  }
}

function paintRectangle(from, to, value) {
  const left = Math.min(from.x, to.x), right = Math.max(from.x, to.x);
  const top = Math.min(from.y, to.y), bottom = Math.max(from.y, to.y);
  for (let x = left; x <= right; x++) { paintCell({ x, y:top }, value); paintCell({ x, y:bottom }, value); }
  for (let y = top; y <= bottom; y++) { paintCell({ x:left, y }, value); paintCell({ x:right, y }, value); }
}

function floodFill(start, replacement) {
  if (!insideLevel(start)) return;
  const target = levelState.rows[start.y][start.x];
  if (target === replacement) return;
  const pending = [start];
  while (pending.length) {
    const point = pending.pop();
    if (!insideLevel(point) || levelState.rows[point.y][point.x] !== target) continue;
    levelState.rows[point.y][point.x] = replacement;
    pending.push({x:point.x-1,y:point.y},{x:point.x+1,y:point.y},{x:point.x,y:point.y-1},{x:point.x,y:point.y+1});
  }
}

async function saveLevel(play) {
  const result = await api(`/api/games/${encodeURIComponent(levelState.gameId)}/editors/${encodeURIComponent(levelState.editorId)}`, {
    method:"PUT", headers:{"Content-Type":"application/json"},
    body:JSON.stringify({ text:levelText(), hash:levelState.hash, play }),
  });
  levelState.hash = result.hash;
  levelState.savedText = levelText();
  updateLevelState();
  if (play) { activeGame = levelState.gameId; setTimeout(refreshStatus, 350); }
}

function closeLevelEditor() {
  if (levelState && levelText() !== levelState.savedText &&
      !confirm("Close the editor and discard the unsaved changes?")) return;
  $("#levelEditorPanel").classList.add("hidden");
  levelState = null;
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
$("#closeLevelEditor").addEventListener("click", closeLevelEditor);
$("#levelTools").querySelectorAll("[data-tool]").forEach((button) => button.addEventListener("click", () => {
  if (levelState) selectLevelTool(button.dataset.tool);
}));
$("#levelZoom").addEventListener("change", (event) => {
  if (!levelState) return;
  levelState.cell = Number(event.target.value);
  renderLevel();
});
$("#undoLevel").addEventListener("click", () => changeHistory("undo", "redo"));
$("#redoLevel").addEventListener("click", () => changeHistory("redo", "undo"));
$("#flipSprite").addEventListener("click", () => {
  if (!levelState || levelState.editor.type !== "sprite") return;
  checkpointLevel();
  levelState.rows.forEach((row) => row.reverse());
  renderLevel();
});
$("#resizeLevel").addEventListener("click", () => {
  if (!levelState) return;
  const width = Number($("#levelWidth").value), height = Number($("#levelHeight").value);
  const maximum = levelState.editor.type === "sprite" ? 128 : 512;
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 || width > maximum || height > maximum) {
    setAction(`Dimensions must be between 1 and ${maximum}.`, true); return;
  }
  const oldWidth = levelState.rows[0].length, oldHeight = levelState.rows.length;
  if (width === oldWidth && height === oldHeight) return;
  const empty = levelState.editor.empty;
  const losesTiles = levelState.rows.some((row, y) => y >= height && row.some((tile) => tile !== empty) ||
    y < height && row.slice(width).some((tile) => tile !== empty));
  if (losesTiles && !confirm("Shrinking the map will remove non-empty tiles. Continue?")) return;
  checkpointLevel();
  levelState.rows = Array.from({length:height}, (_, y) =>
    Array.from({length:width}, (_, x) => levelState.rows[y]?.[x] ?? empty));
  renderLevel();
});
$("#saveLevelLocal").addEventListener("click", () => act($("#saveLevelLocal"), "Saving", () => saveLevel(false)));
$("#saveLevelPlay").addEventListener("click", () => act($("#saveLevelPlay"), "Saving and launching", () => saveLevel(true)));

const levelCanvas = $("#levelCanvas");
levelCanvas.addEventListener("pointerdown", (event) => {
  if (!levelState || event.button !== 0) return;
  const point = cellAt(event);
  if (!insideLevel(point)) return;
  event.preventDefault();
  levelCanvas.setPointerCapture(event.pointerId);
  levelState.drawing = true; levelState.start = point; levelState.last = point;
  checkpointLevel();
  const value = levelState.tool === "erase" ? levelState.editor.empty : levelState.tile;
  if (levelState.tool === "fill") { floodFill(point, value); levelState.drawing = false; renderLevel(); }
  else if (levelState.tool === "pencil" || levelState.tool === "erase") { paintCell(point, value); renderLevel(); }
});
levelCanvas.addEventListener("pointermove", (event) => {
  if (!levelState) return;
  const point = cellAt(event);
  $("#levelCoordinates").textContent = insideLevel(point) ? `Tile ${point.x}, ${point.y}` : "—";
  if (!levelState.drawing || !insideLevel(point) ||
      (point.x === levelState.last.x && point.y === levelState.last.y)) return;
  if (levelState.tool === "pencil" || levelState.tool === "erase") {
    const value = levelState.tool === "erase" ? levelState.editor.empty : levelState.tile;
    paintLine(levelState.last, point, value); levelState.last = point; renderLevel();
  }
});
function finishLevelStroke(event) {
  if (!levelState?.drawing) return;
  const point = cellAt(event);
  const end = insideLevel(point) ? point : levelState.last;
  const value = levelState.tool === "erase" ? levelState.editor.empty : levelState.tile;
  if (levelState.tool === "line") paintLine(levelState.start, end, value);
  else if (levelState.tool === "rectangle") paintRectangle(levelState.start, end, value);
  levelState.drawing = false;
  renderLevel();
}
levelCanvas.addEventListener("pointerup", finishLevelStroke);
levelCanvas.addEventListener("pointercancel", finishLevelStroke);
window.addEventListener("keydown", (event) => {
  if (!levelState || $("#levelEditorPanel").classList.contains("hidden") || !(event.ctrlKey || event.metaKey)) return;
  if (event.key.toLowerCase() === "z") { event.preventDefault(); changeHistory(event.shiftKey ? "redo" : "undo", event.shiftKey ? "undo" : "redo"); }
  else if (event.key.toLowerCase() === "y") { event.preventDefault(); changeHistory("redo", "undo"); }
});
window.addEventListener("beforeunload", (event) => {
  if (levelState && levelText() !== levelState.savedText) event.preventDefault();
});
$("#refreshLogsBtn").addEventListener("click", async () => {
  try { $("#logs").textContent = (await api("/api/logs")).logs || "The host log is empty."; }
  catch (error) { $("#logs").textContent = error.message; }
});

(async function init() {
  try { await loadGames(); await refreshStatus(); }
  catch (error) { setAction(error.message, true); }
  setInterval(refreshStatus, 4000);
})();
