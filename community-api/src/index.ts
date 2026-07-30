/**
 * USBForge Companion API — auth, community posts, feedback, release updates.
 */
export interface Env {
  DB: D1Database;
  JWT_SECRET: string;
  APP_NAME: string;
  APP_VERSION: string;
  GITHUB_REPO: string;
  ANDROID_LATEST_TAG: string;
}

type UserRow = {
  id: string;
  email: string;
  display_name: string;
  password_hash: string;
  salt: string;
  created_at: string;
};

const cors: Record<string, string> = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Content-Type": "application/json",
};

function json(data: unknown, status = 200): Response {
  return new Response(JSON.stringify(data), { status, headers: cors });
}

function bad(message: string, status = 400): Response {
  return json({ ok: false, error: message }, status);
}

async function readJson<T>(req: Request): Promise<T | null> {
  try {
    return (await req.json()) as T;
  } catch {
    return null;
  }
}

function b64url(buf: ArrayBuffer | Uint8Array): string {
  const bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function b64urlDecode(s: string): Uint8Array {
  const pad = "=".repeat((4 - (s.length % 4)) % 4);
  const b64 = (s + pad).replace(/-/g, "+").replace(/_/g, "/");
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

async function pbkdf2(password: string, saltB64: string): Promise<string> {
  const enc = new TextEncoder();
  const key = await crypto.subtle.importKey("raw", enc.encode(password), "PBKDF2", false, ["deriveBits"]);
  const salt = b64urlDecode(saltB64);
  const bits = await crypto.subtle.deriveBits(
    { name: "PBKDF2", hash: "SHA-256", salt, iterations: 100000 },
    key,
    256
  );
  return b64url(bits);
}

function randomId(): string {
  return crypto.randomUUID();
}

function randomSalt(): string {
  const a = new Uint8Array(16);
  crypto.getRandomValues(a);
  return b64url(a);
}

async function signJwt(payload: Record<string, unknown>, secret: string): Promise<string> {
  const header = { alg: "HS256", typ: "JWT" };
  const enc = new TextEncoder();
  const h = b64url(enc.encode(JSON.stringify(header)));
  const p = b64url(enc.encode(JSON.stringify(payload)));
  const data = `${h}.${p}`;
  const key = await crypto.subtle.importKey(
    "raw",
    enc.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"]
  );
  const sig = await crypto.subtle.sign("HMAC", key, enc.encode(data));
  return `${data}.${b64url(sig)}`;
}

async function verifyJwt(token: string, secret: string): Promise<Record<string, unknown> | null> {
  const parts = token.split(".");
  if (parts.length !== 3) return null;
  const [h, p, s] = parts;
  const enc = new TextEncoder();
  const key = await crypto.subtle.importKey(
    "raw",
    enc.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["verify"]
  );
  const ok = await crypto.subtle.verify("HMAC", key, b64urlDecode(s), enc.encode(`${h}.${p}`));
  if (!ok) return null;
  try {
    const json = new TextDecoder().decode(b64urlDecode(p));
    const payload = JSON.parse(json) as Record<string, unknown>;
    if (typeof payload.exp === "number" && Date.now() / 1000 > payload.exp) return null;
    return payload;
  } catch {
    return null;
  }
}

async function authUser(req: Request, env: Env): Promise<UserRow | null> {
  const hdr = req.headers.get("Authorization") || "";
  const m = hdr.match(/^Bearer\s+(.+)$/i);
  if (!m) return null;
  const payload = await verifyJwt(m[1], env.JWT_SECRET || "dev-insecure-secret");
  if (!payload || typeof payload.sub !== "string") return null;
  const row = await env.DB.prepare("SELECT * FROM users WHERE id = ?").bind(payload.sub).first<UserRow>();
  return row || null;
}

async function handleSignup(req: Request, env: Env): Promise<Response> {
  const body = await readJson<{ email?: string; password?: string; display_name?: string }>(req);
  if (!body?.email || !body?.password || !body?.display_name) return bad("email, password, and display_name required");
  const email = body.email.trim().toLowerCase();
  const name = body.display_name.trim().slice(0, 64);
  if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) return bad("invalid email");
  if (body.password.length < 8) return bad("password must be at least 8 characters");
  if (name.length < 2) return bad("display_name too short");

  const existing = await env.DB.prepare("SELECT id FROM users WHERE email = ?").bind(email).first();
  if (existing) return bad("email already registered", 409);

  const id = randomId();
  const salt = randomSalt();
  const password_hash = await pbkdf2(body.password, salt);
  const created_at = new Date().toISOString();
  await env.DB.prepare(
    "INSERT INTO users (id, email, display_name, password_hash, salt, created_at) VALUES (?, ?, ?, ?, ?, ?)"
  )
    .bind(id, email, name, password_hash, salt, created_at)
    .run();

  const token = await signJwt(
    { sub: id, email, name, exp: Math.floor(Date.now() / 1000) + 60 * 60 * 24 * 30 },
    env.JWT_SECRET || "dev-insecure-secret"
  );
  return json({ ok: true, token, user: { id, email, display_name: name } });
}

async function handleSignin(req: Request, env: Env): Promise<Response> {
  const body = await readJson<{ email?: string; password?: string }>(req);
  if (!body?.email || !body?.password) return bad("email and password required");
  const email = body.email.trim().toLowerCase();
  const user = await env.DB.prepare("SELECT * FROM users WHERE email = ?").bind(email).first<UserRow>();
  if (!user) return bad("invalid email or password", 401);
  const hash = await pbkdf2(body.password, user.salt);
  if (hash !== user.password_hash) return bad("invalid email or password", 401);
  const token = await signJwt(
    {
      sub: user.id,
      email: user.email,
      name: user.display_name,
      exp: Math.floor(Date.now() / 1000) + 60 * 60 * 24 * 30,
    },
    env.JWT_SECRET || "dev-insecure-secret"
  );
  return json({
    ok: true,
    token,
    user: { id: user.id, email: user.email, display_name: user.display_name },
  });
}

async function handleMe(req: Request, env: Env): Promise<Response> {
  const user = await authUser(req, env);
  if (!user) return bad("unauthorized", 401);
  return json({
    ok: true,
    user: { id: user.id, email: user.email, display_name: user.display_name },
  });
}

async function handleListPosts(env: Env): Promise<Response> {
  const rows = await env.DB.prepare(
    `SELECT p.id, p.title, p.body, p.kind, p.created_at, u.display_name AS author
     FROM posts p JOIN users u ON u.id = p.user_id
     ORDER BY p.created_at DESC LIMIT 100`
  ).all();
  return json({ ok: true, posts: rows.results || [] });
}

async function handleCreatePost(req: Request, env: Env): Promise<Response> {
  const user = await authUser(req, env);
  if (!user) return bad("sign in to post", 401);
  const body = await readJson<{ title?: string; body?: string; kind?: string }>(req);
  if (!body?.title || !body?.body) return bad("title and body required");
  const kind = (body.kind || "share").toLowerCase();
  if (!["share", "tip", "question", "feedback"].includes(kind)) return bad("invalid kind");
  const id = randomId();
  const created_at = new Date().toISOString();
  await env.DB.prepare(
    "INSERT INTO posts (id, user_id, title, body, kind, created_at) VALUES (?, ?, ?, ?, ?, ?)"
  )
    .bind(id, user.id, body.title.trim().slice(0, 120), body.body.trim().slice(0, 4000), kind, created_at)
    .run();
  return json({
    ok: true,
    post: {
      id,
      title: body.title.trim().slice(0, 120),
      body: body.body.trim().slice(0, 4000),
      kind,
      created_at,
      author: user.display_name,
    },
  });
}

async function handleFeedback(req: Request, env: Env): Promise<Response> {
  const user = await authUser(req, env);
  const body = await readJson<{ message?: string; rating?: number; email?: string }>(req);
  if (!body?.message || body.message.trim().length < 3) return bad("message required");
  const id = randomId();
  const created_at = new Date().toISOString();
  const rating = typeof body.rating === "number" ? Math.max(1, Math.min(5, Math.round(body.rating))) : null;
  await env.DB.prepare(
    "INSERT INTO feedback (id, user_id, email, message, rating, created_at) VALUES (?, ?, ?, ?, ?, ?)"
  )
    .bind(
      id,
      user?.id || null,
      user?.email || body.email?.trim().toLowerCase() || null,
      body.message.trim().slice(0, 4000),
      rating,
      created_at
    )
    .run();
  return json({ ok: true, id });
}

async function handleUpdates(env: Env, clientVersion?: string | null): Promise<Response> {
  let github: unknown = null;
  try {
    const res = await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/releases/latest`, {
      headers: {
        Accept: "application/vnd.github+json",
        "User-Agent": `USBForge-Companion/${env.APP_VERSION}`,
      },
    });
    if (res.ok) github = await res.json();
  } catch {
    github = null;
  }

  const g = github as {
    tag_name?: string;
    name?: string;
    body?: string;
    html_url?: string;
    published_at?: string;
    assets?: Array<{ name: string; browser_download_url: string; size: number }>;
  } | null;

  const desktopLatest = g?.tag_name || null;
  const androidLatest = env.ANDROID_LATEST_TAG || "android-1.0.0";
  const client = (clientVersion || "").replace(/^v/, "");
  const androidVer = androidLatest.replace(/^android-/, "");
  const updateAvailable = !!client && compareVer(client, androidVer) < 0;

  const androidAsset =
    g?.assets?.find((a) => /usbforge.*\.apk$/i.test(a.name) || /companion.*\.apk$/i.test(a.name)) ||
    null;

  return json({
    ok: true,
    app: {
      name: env.APP_NAME,
      version: env.APP_VERSION,
      android_latest: androidVer,
      update_available: updateAvailable,
      download_url:
        androidAsset?.browser_download_url ||
        `https://github.com/${env.GITHUB_REPO}/releases`,
    },
    desktop: {
      tag: desktopLatest,
      name: g?.name || null,
      notes: g?.body || null,
      url: g?.html_url || `https://github.com/${env.GITHUB_REPO}/releases`,
      published_at: g?.published_at || null,
      assets: (g?.assets || []).map((a) => ({
        name: a.name,
        url: a.browser_download_url,
        size: a.size,
      })),
    },
  });
}

function compareVer(a: string, b: string): number {
  const pa = a.split(".").map((x) => parseInt(x.replace(/\D/g, ""), 10) || 0);
  const pb = b.split(".").map((x) => parseInt(x.replace(/\D/g, ""), 10) || 0);
  const n = Math.max(pa.length, pb.length);
  for (let i = 0; i < n; i++) {
    const d = (pa[i] || 0) - (pb[i] || 0);
    if (d) return d < 0 ? -1 : 1;
  }
  return 0;
}

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: cors });

    const url = new URL(req.url);
    const path = url.pathname.replace(/\/+$/, "") || "/";

    try {
      if (req.method === "GET" && path === "/health") {
        return json({ ok: true, service: "usbforge-api", version: env.APP_VERSION });
      }
      if (req.method === "POST" && path === "/auth/signup") return handleSignup(req, env);
      if (req.method === "POST" && path === "/auth/signin") return handleSignin(req, env);
      if (req.method === "GET" && path === "/auth/me") return handleMe(req, env);
      if (req.method === "GET" && path === "/posts") return handleListPosts(env);
      if (req.method === "POST" && path === "/posts") return handleCreatePost(req, env);
      if (req.method === "POST" && path === "/feedback") return handleFeedback(req, env);
      if (req.method === "GET" && path === "/updates") {
        return handleUpdates(env, url.searchParams.get("v"));
      }
      return bad("not found", 404);
    } catch (e) {
      const msg = e instanceof Error ? e.message : "server error";
      return bad(msg, 500);
    }
  },
};
