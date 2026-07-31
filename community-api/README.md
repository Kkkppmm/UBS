# USBForge Companion API

Cloudflare Worker + D1 backend for the Android Companion app.

## Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/health` | no | Health check |
| POST | `/auth/signup` | no | `{ email, password, display_name }` |
| POST | `/auth/signin` | no | `{ email, password }` |
| GET | `/auth/me` | Bearer | Current user |
| GET | `/posts` | no | Community feed |
| POST | `/posts` | Bearer | `{ title, body, kind }` |
| POST | `/feedback` | optional | `{ message, rating?, email? }` |
| GET | `/updates?v=` | no | App + desktop release info |

## Deploy

```bash
npm install
npx wrangler login
npx wrangler secret put JWT_SECRET
npm run db:migrate
npm run deploy
```

D1 database: `usbforge-community` (see `wrangler.jsonc`).

Point the Android app at the Worker URL via `android/app/src/main/assets/www/config.js` → `API_BASE`.
