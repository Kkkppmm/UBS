(() => {
  "use strict";

  const state = {
    page: "welcome",
    mode: "usb", // usb | iso
    busy: false,
    lastFailed: false,
    version: "—",
  };

  const $ = (sel) => document.querySelector(sel);
  const $$ = (sel) => Array.from(document.querySelectorAll(sel));

  const LICENSE = `USBForge Media Creation Tool
Copyright (c) USBForge contributors

Applicable notices
------------------
This tool creates bootable installation media (ISO or USB flash drive),
similar to the Windows Media Creation Tool.

• Writing to a USB flash drive will erase all data on that drive.
• Always double-check the selected drive before continuing.
• Windows ISOs are prepared for FAT32 (large install.wim may be split).
• After booting USBForge media you get a USB Lab for testing and docs;
  install is optional.

By selecting Accept you acknowledge these notices and agree to use this
software at your own risk. See LICENSE and docs/safety.md for details.

Privacy: Check for updates contacts GitHub Releases over the network.`;

  function native(msg) {
    const payload = typeof msg === "string" ? msg : JSON.stringify(msg);
    try {
      if (window.webkit && webkit.messageHandlers && webkit.messageHandlers.usbforge) {
        webkit.messageHandlers.usbforge.postMessage(payload);
        return true;
      }
    } catch (e) { /* fall through */ }
    console.warn("native bridge unavailable", msg);
    return false;
  }

  function setProgress(pct, klass) {
    const bar = $("#progress-bar");
    bar.style.width = Math.max(0, Math.min(100, pct)) + "%";
    bar.classList.remove("busy", "done", "fail");
    if (klass) bar.classList.add(klass);
  }

  function appendLog(line) {
    const log = $("#log");
    log.textContent += (log.textContent ? "\n" : "") + line;
    log.scrollTop = log.scrollHeight;
    $("#status").textContent = line;
  }

  function showBanner(ok, text) {
    const b = $("#banner");
    b.hidden = false;
    b.className = "banner " + (ok ? "ok" : "err");
    b.textContent = text;
  }

  function hideBanner() {
    const b = $("#banner");
    b.hidden = true;
    b.textContent = "";
    b.className = "banner";
  }

  function setSteps(active) {
    $$(".step").forEach((el) => {
      const i = Number(el.dataset.step);
      el.classList.toggle("on", i === active);
      el.classList.toggle("done", i < active);
    });
  }

  function syncModeUi() {
    const usb = state.mode === "usb";
    $("#opt-usb").classList.toggle("on", usb);
    $("#opt-iso").classList.toggle("on", !usb);
    $("#usb-block").hidden = !usb;
    $("#usb-warn").hidden = !usb;
    $("#iso-label").textContent = usb ? "ISO file" : "Save ISO as";
    $("#media-title").textContent = usb ? "Choose which media to use" : "Select an ISO file";
    $("#media-hint").textContent = usb
      ? "Select the ISO image and the USB flash drive you want to use."
      : "Choose where to save the new bootable ISO image.";
    $("#iso-path").placeholder = usb ? "Select an ISO file…" : "Choose ISO save location…";
  }

  function renderFooter() {
    const foot = $("#footer");
    const page = state.page;
    let html = "";

    if (page === "welcome") {
      html = `
        <div class="left">
          <button type="button" class="btn" id="f-cancel">Cancel</button>
          <button type="button" class="link" id="f-help">Help</button>
          <button type="button" class="link" id="f-updates">Check for updates</button>
        </div>
        <div class="spacer"></div>
        <button type="button" class="btn primary" id="f-accept">Accept</button>`;
    } else if (page === "mode") {
      html = `
        <div class="spacer"></div>
        <button type="button" class="btn" id="f-back">Back</button>
        <button type="button" class="btn primary" id="f-next">Next</button>`;
    } else if (page === "media") {
      html = `
        <div class="spacer"></div>
        <button type="button" class="btn" id="f-back">Back</button>
        <button type="button" class="btn primary" id="f-create">Create</button>`;
    } else if (page === "progress") {
      const finishLabel = state.lastFailed ? "Try again" : "Finish";
      html = `
        <div class="spacer"></div>
        <button type="button" class="btn" id="f-back" ${state.busy ? "disabled" : ""}>Back</button>
        <button type="button" class="btn primary" id="f-finish" ${state.busy ? "disabled" : ""}>${finishLabel}</button>`;
    } else if (page === "help") {
      html = `
        <div class="spacer"></div>
        <button type="button" class="btn primary" id="f-back">Back</button>`;
    }

    foot.innerHTML = html;
    wireFooter();
  }

  function showPage(name) {
    state.page = name;
    $$(".page").forEach((p) => p.classList.remove("active"));
    const el = $("#page-" + name);
    if (el) el.classList.add("active");

    const stepMap = { welcome: 0, mode: 1, media: 2, progress: 3, help: -1 };
    if (stepMap[name] >= 0) setSteps(stepMap[name]);
    renderFooter();
  }

  function wireFooter() {
    const on = (id, fn) => {
      const el = $("#" + id);
      if (el) el.addEventListener("click", fn);
    };
    on("f-cancel", () => native({ action: "quit" }));
    on("f-help", () => {
      native({ action: "load_help", topic: "getting-started.md" });
      showPage("help");
    });
    on("f-updates", () => native({ action: "check_updates" }));
    on("f-accept", () => showPage("mode"));
    on("f-next", () => {
      syncModeUi();
      native({ action: "scan_usb" });
      showPage("media");
    });
    on("f-back", () => {
      if (state.busy) return;
      if (state.page === "mode") showPage("welcome");
      else if (state.page === "media") showPage("mode");
      else if (state.page === "progress") showPage(state.lastFailed ? "media" : "mode");
      else if (state.page === "help") showPage("welcome");
    });
    on("f-create", () => startCreate());
    on("f-finish", () => {
      if (state.busy) return;
      if (state.lastFailed) showPage("media");
      else showPage("welcome");
    });
  }

  function startCreate() {
    const iso = $("#iso-path").value.trim();
    const usb = $("#usb-select").value;
    if (!iso) {
      appendLog(state.mode === "iso"
        ? "Choose where to save the ISO first."
        : "Choose an ISO file first.");
      return;
    }
    if (state.mode === "usb" && !usb) {
      appendLog("Select a USB flash drive first.");
      return;
    }

    state.busy = true;
    state.lastFailed = false;
    hideBanner();
    $("#log").textContent = "";
    $("#activity").hidden = false;
    $("#progress-title").textContent = state.mode === "usb"
      ? "Creating your USB flash drive"
      : "Creating your ISO file";
    $("#progress-sub").textContent =
      "This might take a while — keep this window open until the process finishes.";
    setProgress(4, "busy");
    showPage("progress");
    appendLog("Getting things ready…");

    native({
      action: "create",
      mode: state.mode,
      iso,
      usb: usb || "",
    });
  }

  function fillUsb(devices) {
    const sel = $("#usb-select");
    const prev = sel.value;
    sel.innerHTML = "";
    (devices || []).forEach((d) => {
      const opt = document.createElement("option");
      opt.value = d.path;
      opt.textContent = `${d.path}  —  ${d.model || d.name || "USB"} (${d.size || "?"})`;
      sel.appendChild(opt);
    });
    if (!sel.options.length) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "No removable USB drives found";
      sel.appendChild(opt);
    } else if ([...sel.options].some((o) => o.value === prev)) {
      sel.value = prev;
    }
  }

  /* Called from native C host */
  window.UF = {
    receive(raw) {
      let msg;
      try { msg = typeof raw === "string" ? JSON.parse(raw) : raw; }
      catch (e) { appendLog(String(raw)); return; }

      switch (msg.event) {
        case "ready":
          state.version = msg.version || "—";
          $("#version").textContent = "v" + state.version;
          break;
        case "usb_list":
          fillUsb(msg.devices || []);
          break;
        case "iso_chosen":
          if (msg.path) $("#iso-path").value = msg.path;
          break;
        case "log":
          if (msg.text) appendLog(msg.text);
          break;
        case "progress":
          if (typeof msg.percent === "number") {
            setProgress(msg.percent, msg.klass || (state.busy ? "busy" : ""));
          }
          break;
        case "job_done": {
          state.busy = false;
          $("#activity").hidden = true;
          state.lastFailed = !msg.ok;
          if (msg.ok) {
            setProgress(100, "done");
            $("#progress-title").textContent = "Your media is ready";
            $("#progress-sub").textContent =
              "You can remove the USB drive safely, or create another.";
            showBanner(true, msg.message || "Success.");
            setSteps(3);
          } else {
            setProgress(15, "fail");
            $("#progress-title").textContent = "Something went wrong";
            $("#progress-sub").textContent =
              "Check the details below, then go Back and try again.";
            showBanner(false, msg.message || "Failed.");
          }
          renderFooter();
          break;
        }
        case "job_cancelled":
          state.busy = false;
          $("#activity").hidden = true;
          showPage("media");
          break;
        case "help":
          $("#help-text").textContent = msg.text || "(empty)";
          break;
        case "update_result":
          alert(msg.message || "Update check finished.");
          if (msg.open_url) native({ action: "open_url", url: msg.open_url });
          break;
        case "error":
          appendLog(msg.message || "Error");
          if (state.busy) {
            state.busy = false;
            state.lastFailed = true;
            $("#activity").hidden = true;
            setProgress(10, "fail");
            showBanner(false, msg.message || "Error");
            renderFooter();
          }
          break;
        default:
          break;
      }
    },
  };

  function init() {
    $("#license-text").textContent = LICENSE;
    syncModeUi();
    showPage("welcome");

    $("#opt-usb").addEventListener("click", () => {
      state.mode = "usb";
      syncModeUi();
    });
    $("#opt-iso").addEventListener("click", () => {
      state.mode = "iso";
      syncModeUi();
    });
    $("#btn-browse").addEventListener("click", () => {
      native({ action: "browse_iso", mode: state.mode === "iso" ? "save" : "open" });
    });
    $("#btn-refresh").addEventListener("click", () => native({ action: "scan_usb" }));
    $("#btn-verify").addEventListener("click", () => {
      const iso = $("#iso-path").value.trim();
      if (!iso) { appendLog("Choose an ISO file first."); return; }
      state.busy = true;
      state.lastFailed = false;
      hideBanner();
      $("#log").textContent = "";
      $("#activity").hidden = false;
      $("#progress-title").textContent = "Verifying ISO file";
      $("#progress-sub").textContent = "Reading the ISO structure…";
      setProgress(8, "busy");
      showPage("progress");
      native({ action: "verify_iso", iso });
    });
    $$("[data-topic]").forEach((btn) => {
      btn.addEventListener("click", () => {
        native({ action: "load_help", topic: btn.dataset.topic });
      });
    });

    native({ action: "ready" });
    native({ action: "scan_usb" });
  }

  document.addEventListener("DOMContentLoaded", init);
})();
