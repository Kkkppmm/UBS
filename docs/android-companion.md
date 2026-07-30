# USBForge Companion (Android)

Android APK for browsing USBForge releases, checking Companion updates, sending feedback, and joining a community feed (sign up / sign in to post).

## Features

- **Updates** — checks GitHub Releases for desktop packages and Companion APKs; own in-app update prompt
- **Community** — share tips, questions, and posts (requires account)
- **Feedback** — star rating + message (optional email)
- **Account** — sign up / sign in (local on-device store when API is unset; Cloudflare Worker + D1 when `API_BASE` is set)

## Build the APK

Requirements: JDK 17+, Android SDK (API 34), Gradle wrapper.

```bash
export ANDROID_HOME=/path/to/Android/Sdk
cd android
./gradlew assembleRelease   # or assembleDebug
```

Prebuilt (signed) APK: **`android/dist/USBForge-Companion-1.0.0.apk`**

Gradle outputs:

- Debug: `android/app/build/outputs/apk/debug/app-debug.apk`
- Release (unsigned): `android/app/build/outputs/apk/release/app-release-unsigned.apk`

Sign a release build with your own keystore (`keytool` + `apksigner`) before publishing updates. Bump `versionName` / `versionCode` in `android/app/build.gradle` and `APP_VERSION` in `config.js` when shipping a new Companion release (tag pattern: `android-x.y.z`).

## Wire the community API

1. Deploy `community-api/` (Cloudflare Worker + D1):

```bash
cd community-api
npm install
npx wrangler login
npx wrangler secret put JWT_SECRET
npm run db:migrate
npm run deploy
```

2. Set the Worker URL in the app:

```js
// android/app/src/main/assets/www/config.js
API_BASE: "https://usbforge-api.<account>.workers.dev"
```

Without `API_BASE`, auth/posts/feedback stay on-device (localStorage) and updates still use GitHub.

## Project layout

```
android/                 Gradle app (WebView host)
  app/src/main/assets/www/   HTML/CSS/JS UI
community-api/           Cloudflare Worker + D1 backend
```
