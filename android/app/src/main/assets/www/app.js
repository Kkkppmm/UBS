(() => {
  "use strict";

  const CFG = window.UF_CONFIG || {};
  const APP_VERSION = CFG.APP_VERSION || "1.0.0";
  const REPO = CFG.GITHUB_REPO || "Kkkppmm/UBS";
  const API = (CFG.API_BASE || "").replace(/\/$/, "");
  const LS = {
    token: "uf_token",
    user: "uf_user",
    users: "uf_local_users",
    posts: "uf_local_posts",
    feedback: "uf_local_feedback",
  };

  const state = {
    user: null,
    token: null,
    authMode: "signin",
    rating: 0,
    remote: false,
  };

  const $ = (s) => document.querySelector(s);
  const $$ = (s) => Array.from(document.querySelectorAll(s));

  function toast(msg) {
    const t = $("#toast");
    t.textContent = msg;
    t.hidden = false;
    requestAnimationFrame(() => t.classList.add("show"));
    clearTimeout(toast._t);
    toast._t = setTimeout(() => {
      t.classList.remove("show");
      setTimeout(() => { t.hidden = true; }, 280);
    }, 2600);
  }

  function moveTabIndicator(name) {
    const tabs = $$(".tab");
    const idx = Math.max(0, tabs.findIndex((t) => t.dataset.tab === name));
    const ind = $("#tab-indicator");
    if (ind) ind.style.transform = `translateX(${idx * 100}%)`;
  }

  function saveSession() {
    if (state.token) localStorage.setItem(LS.token, state.token);
    else localStorage.removeItem(LS.token);
    if (state.user) localStorage.setItem(LS.user, JSON.stringify(state.user));
    else localStorage.removeItem(LS.user);
    renderAccountChip();
  }

  function loadSession() {
    try {
      state.token = localStorage.getItem(LS.token);
      state.user = JSON.parse(localStorage.getItem(LS.user) || "null");
    } catch { state.user = null; state.token = null; }
  }

  function renderAccountChip() {
    $("#btn-account").textContent = state.user ? state.user.display_name : "Sign in";
    const out = $("#auth-signed-out");
    const inn = $("#auth-signed-in");
    if (state.user) {
      out.hidden = true;
      inn.hidden = false;
      $("#whoami").textContent = `${state.user.display_name} · ${state.user.email}`;
    } else {
      out.hidden = false;
      inn.hidden = true;
    }
  }

  async function api(path, opts = {}) {
    if (!API) throw new Error("no-api");
    const headers = Object.assign({ "Content-Type": "application/json" }, opts.headers || {});
    if (state.token) headers.Authorization = `Bearer ${state.token}`;
    const res = await fetch(API + path, { ...opts, headers });
    const data = await res.json().catch(() => ({}));
    if (!res.ok || data.ok === false) throw new Error(data.error || ("HTTP " + res.status));
    return data;
  }

  /* -------- local crypto helpers (Web Crypto) -------- */
  function b64(buf) {
    const u = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
    let s = "";
    for (let i = 0; i < u.length; i++) s += String.fromCharCode(u[i]);
    return btoa(s);
  }
  function unb64(s) {
    const bin = atob(s);
    const u = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i);
    return u;
  }
  async function hashPass(password, saltB64) {
    const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(password), "PBKDF2", false, ["deriveBits"]);
    const bits = await crypto.subtle.deriveBits(
      { name: "PBKDF2", hash: "SHA-256", salt: unb64(saltB64), iterations: 100000 },
      key,
      256
    );
    return b64(bits);
  }
  function uid() {
    return crypto.randomUUID ? crypto.randomUUID() : String(Date.now()) + Math.random().toString(16).slice(2);
  }

  function localUsers() {
    try { return JSON.parse(localStorage.getItem(LS.users) || "[]"); }
    catch { return []; }
  }
  function saveUsers(list) { localStorage.setItem(LS.users, JSON.stringify(list)); }

  function localPosts() {
    try { return JSON.parse(localStorage.getItem(LS.posts) || "[]"); }
    catch { return []; }
  }
  function savePosts(list) { localStorage.setItem(LS.posts, JSON.stringify(list)); }

  /* -------- Updates -------- */
  function compareVer(a, b) {
    const pa = String(a).replace(/^v/, "").split(".").map((x) => parseInt(x, 10) || 0);
    const pb = String(b).replace(/^v/, "").split(".").map((x) => parseInt(x, 10) || 0);
    const n = Math.max(pa.length, pb.length);
    for (let i = 0; i < n; i++) {
      const d = (pa[i] || 0) - (pb[i] || 0);
      if (d) return d < 0 ? -1 : 1;
    }
    return 0;
  }

  async function checkUpdates() {
    $("#app-update-title").textContent = "Checking…";
    $("#app-update-body").textContent = "Contacting GitHub Releases…";
    $("#btn-download-apk").hidden = true;

    let release = null;
    try {
      if (API) {
        const data = await api("/updates?v=" + encodeURIComponent(APP_VERSION));
        state.remote = true;
        applyUpdatePayload(data);
        return;
      }
    } catch (e) {
      /* fall through to GitHub */
    }

    try {
      const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
        headers: { Accept: "application/vnd.github+json", "User-Agent": "USBForge-Companion/" + APP_VERSION },
      });
      if (!res.ok) throw new Error("GitHub " + res.status);
      release = await res.json();
    } catch (e) {
      $("#app-update-title").textContent = "Could not check updates";
      $("#app-update-body").textContent = String(e.message || e);
      return;
    }

    const assets = release.assets || [];
    const apk = assets.find((a) => /\.apk$/i.test(a.name));
    let androidLatest = APP_VERSION;
    const tag = (release.tag_name || "").replace(/^v/, "");
    if (/^android-/i.test(release.tag_name || "")) {
      androidLatest = release.tag_name.replace(/^android-/i, "");
    } else if (apk) {
      const m = apk.name.match(/(\d+\.\d+\.\d+)/);
      if (m) androidLatest = m[1];
    }

    const need = compareVer(APP_VERSION, androidLatest) < 0;
    $("#app-update-title").textContent = need
      ? `Update available: v${androidLatest}`
      : `You're up to date (v${APP_VERSION})`;
    $("#app-update-body").textContent = need
      ? "A newer Companion APK is on GitHub Releases."
      : "Companion will keep checking GitHub for new USBForge and app releases.";
    if (apk) {
      const a = $("#btn-download-apk");
      a.hidden = false;
      a.href = apk.browser_download_url;
      a.textContent = need ? "Download update" : "Download APK";
    }

    $("#desktop-title").textContent = release.name || release.tag_name || "Latest release";
    $("#desktop-meta").textContent = `${release.tag_name || ""} · ${release.published_at ? new Date(release.published_at).toLocaleString() : ""}`;
    $("#desktop-notes").textContent = (release.body || "No notes.").slice(0, 4000);
    $("#btn-desktop-release").href = release.html_url || CFG.RELEASES_URL;
    const list = $("#asset-list");
    list.innerHTML = "";
    assets.slice(0, 12).forEach((a) => {
      const el = document.createElement("a");
      el.className = "asset";
      el.href = a.browser_download_url;
      el.target = "_blank";
      el.rel = "noopener";
      el.textContent = `${a.name} (${Math.round((a.size || 0) / 1024)} KB)`;
      list.appendChild(el);
    });
  }

  function applyUpdatePayload(data) {
    const app = data.app || {};
    const desk = data.desktop || {};
    const need = !!app.update_available;
    $("#app-update-title").textContent = need
      ? `Update available: v${app.android_latest}`
      : `You're up to date (v${APP_VERSION})`;
    $("#app-update-body").textContent = need
      ? "Download the latest Companion APK from releases."
      : "Connected to USBForge update service.";
    if (app.download_url) {
      const a = $("#btn-download-apk");
      a.hidden = false;
      a.href = app.download_url;
    }
    $("#desktop-title").textContent = desk.name || desk.tag || "Latest USBForge";
    $("#desktop-meta").textContent = `${desk.tag || ""} · ${desk.published_at ? new Date(desk.published_at).toLocaleString() : ""}`;
    $("#desktop-notes").textContent = (desk.notes || "No notes.").slice(0, 4000);
    $("#btn-desktop-release").href = desk.url || CFG.RELEASES_URL;
    const list = $("#asset-list");
    list.innerHTML = "";
    (desk.assets || []).forEach((a) => {
      const el = document.createElement("a");
      el.className = "asset";
      el.href = a.url;
      el.target = "_blank";
      el.rel = "noopener";
      el.textContent = `${a.name} (${Math.round((a.size || 0) / 1024)} KB)`;
      list.appendChild(el);
    });
  }

  /* -------- Community -------- */
  function renderPosts(posts) {
    const root = $("#posts");
    root.innerHTML = "";
    if (!posts.length) {
      root.innerHTML = `<div class="empty">No posts yet — be the first signal</div>`;
      return;
    }
    posts.forEach((p, i) => {
      const el = document.createElement("article");
      el.className = "post";
      el.style.animationDelay = `${Math.min(i, 8) * 40}ms`;
      el.innerHTML = `
        <span class="kind">${escapeHtml(p.kind || "share")}</span>
        <h3>${escapeHtml(p.title)}</h3>
        <p class="body">${escapeHtml(p.body)}</p>
        <div class="meta">${escapeHtml(p.author || "User")} · ${new Date(p.created_at).toLocaleString()}</div>`;
      root.appendChild(el);
    });
  }

  function escapeHtml(s) {
    return String(s || "")
      .replace(/&/g, "&amp;").replace(/</g, "&lt;")
      .replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }

  async function loadPosts() {
    try {
      if (API) {
        const data = await api("/posts");
        renderPosts(data.posts || []);
        return;
      }
    } catch (e) { /* local */ }
    renderPosts(localPosts());
  }

  async function createPost(title, body, kind) {
    if (!state.user) {
      toast("Sign in to post");
      showTab("account");
      return;
    }
    try {
      if (API) {
        await api("/posts", { method: "POST", body: JSON.stringify({ title, body, kind }) });
        toast("Published");
        await loadPosts();
        return;
      }
    } catch (e) {
      toast(e.message);
      return;
    }
    const posts = localPosts();
    posts.unshift({
      id: uid(),
      title,
      body,
      kind,
      author: state.user.display_name,
      created_at: new Date().toISOString(),
    });
    savePosts(posts.slice(0, 200));
    toast("Published (on this device)");
    renderPosts(localPosts());
  }

  /* -------- Auth -------- */
  async function signupLocal(email, password, name) {
    const users = localUsers();
    if (users.some((u) => u.email === email)) throw new Error("email already registered");
    const salt = b64(crypto.getRandomValues(new Uint8Array(16)));
    const password_hash = await hashPass(password, salt);
    const user = { id: uid(), email, display_name: name, password_hash, salt };
    users.push(user);
    saveUsers(users);
    return { id: user.id, email, display_name: name };
  }

  async function signinLocal(email, password) {
    const user = localUsers().find((u) => u.email === email);
    if (!user) throw new Error("invalid email or password");
    const hash = await hashPass(password, user.salt);
    if (hash !== user.password_hash) throw new Error("invalid email or password");
    return { id: user.id, email: user.email, display_name: user.display_name };
  }

  async function doAuth() {
    const email = $("#auth-email").value.trim().toLowerCase();
    const password = $("#auth-pass").value;
    const name = $("#auth-name").value.trim();
    $("#auth-status").textContent = "Working…";
    try {
      if (!email || !password) throw new Error("email and password required");
      if (state.authMode === "signup") {
        if (name.length < 2) throw new Error("display name required");
        if (password.length < 8) throw new Error("password must be 8+ characters");
        if (API) {
          const data = await api("/auth/signup", {
            method: "POST",
            body: JSON.stringify({ email, password, display_name: name }),
          });
          state.token = data.token;
          state.user = data.user;
        } else {
          state.user = await signupLocal(email, password, name);
          state.token = "local";
        }
      } else {
        if (API) {
          const data = await api("/auth/signin", {
            method: "POST",
            body: JSON.stringify({ email, password }),
          });
          state.token = data.token;
          state.user = data.user;
        } else {
          state.user = await signinLocal(email, password);
          state.token = "local";
        }
      }
      saveSession();
      $("#auth-status").textContent = "";
      toast("Welcome, " + state.user.display_name);
      showTab("community");
    } catch (e) {
      $("#auth-status").textContent = e.message || String(e);
    }
  }

  /* -------- Feedback -------- */
  async function sendFeedback() {
    const message = $("#feedback-msg").value.trim();
    const email = $("#feedback-email").value.trim();
    if (message.length < 3) { toast("Write a bit more feedback"); return; }
    try {
      if (API) {
        await api("/feedback", {
          method: "POST",
          body: JSON.stringify({ message, rating: state.rating || null, email: email || undefined }),
        });
      } else {
        const list = JSON.parse(localStorage.getItem(LS.feedback) || "[]");
        list.unshift({
          id: uid(),
          message,
          rating: state.rating || null,
          email: state.user?.email || email || null,
          created_at: new Date().toISOString(),
        });
        localStorage.setItem(LS.feedback, JSON.stringify(list.slice(0, 100)));
      }
      $("#feedback-msg").value = "";
      $("#feedback-status").textContent = "Thanks — feedback saved.";
      toast("Feedback sent");
    } catch (e) {
      $("#feedback-status").textContent = e.message;
    }
  }

  /* -------- Navigation -------- */
  function showTab(name) {
    $$(".tab").forEach((t) => t.classList.toggle("on", t.dataset.tab === name));
    $$(".panel").forEach((p) => p.classList.toggle("on", p.id === "panel-" + name));
    moveTabIndicator(name);
    if (name === "community") loadPosts();
    if (name === "updates") checkUpdates();
  }

  function setAuthMode(mode) {
    state.authMode = mode;
    $$(".seg-btn").forEach((b) => b.classList.toggle("on", b.dataset.mode === mode));
    $("#auth-title").textContent = mode === "signup" ? "Create account" : "Sign in";
    const nameField = $("#auth-name-field");
    if (nameField) nameField.hidden = mode !== "signup";
    $("#btn-auth").textContent = mode === "signup" ? "Sign up" : "Sign in";
  }

  function init() {
    loadSession();
    renderAccountChip();
    showTab("updates");
    requestAnimationFrame(() => moveTabIndicator("updates"));

    $$(".tab").forEach((t) => t.addEventListener("click", () => showTab(t.dataset.tab)));
    $("#btn-account").addEventListener("click", () => showTab("account"));
    $("#btn-check-update").addEventListener("click", checkUpdates);
    $("#btn-new-post").addEventListener("click", () => {
      if (!state.user) { toast("Sign in to post"); showTab("account"); return; }
      $("#post-composer").hidden = false;
    });
    $("#btn-cancel-post").addEventListener("click", () => { $("#post-composer").hidden = true; });
    $("#btn-submit-post").addEventListener("click", async () => {
      const title = $("#post-title").value.trim();
      const body = $("#post-body").value.trim();
      const kind = $("#post-kind").value;
      if (!title || !body) { toast("Title and body required"); return; }
      await createPost(title, body, kind);
      $("#post-title").value = "";
      $("#post-body").value = "";
      $("#post-composer").hidden = true;
    });
    $$(".seg-btn").forEach((b) => b.addEventListener("click", () => setAuthMode(b.dataset.mode)));
    $("#btn-auth").addEventListener("click", doAuth);
    $("#btn-signout").addEventListener("click", () => {
      state.user = null; state.token = null; saveSession(); toast("Signed out");
    });
    $("#btn-send-feedback").addEventListener("click", sendFeedback);
    $$("#stars button").forEach((b) => b.addEventListener("click", () => {
      state.rating = Number(b.dataset.r);
      $$("#stars button").forEach((x) => x.classList.toggle("on", Number(x.dataset.r) <= state.rating));
    }));

    if (window.USBForgeAndroid && USBForgeAndroid.getAppVersion) {
      try {
        const v = USBForgeAndroid.getAppVersion();
        if (v) CFG.APP_VERSION = v;
      } catch (_) {}
    }
  }

  document.addEventListener("DOMContentLoaded", init);
})();
