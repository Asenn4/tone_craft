#!/usr/bin/env python3
"""
Official PedalBoard ModelPacks Fetcher (TONE3000 OAuth 2.0 PKCE + REST API v1).

Reads Daftar_Effect_GP-200_Checklist_Gratis.xlsx, authenticates with your own
TONE3000 Publishable Key (t3k_pub_...) via standard RFC 8252 loopback OAuth 2.0
PKCE (S256), and downloads matching .nam / .wav captures into:
  assets/modelpacks/{pedals,amps,cabs,reverbs}/{NNN}_{ModelName}.{nam|wav}

Features:
  - Resumable: skips any Excel ID that already has a capture in assets/modelpacks/.
  - Deduplicates shared PRE (#4..#10) <-> DST (#52..#58) boost aliases.
  - Polite rate limiting + automatic HTTP 429 backoff + token refresh.
  - Pure Python stdlib (zero external packages required).
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import http.server
import json
import os
import pathlib
import re
import secrets
import socketserver
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
import xml.etree.ElementTree as ET
import zipfile


API_ORIGIN = "https://www.tone3000.com"
TOKEN_ENDPOINT = f"{API_ORIGIN}/api/v1/oauth/token"
AUTHORIZE_ENDPOINT = f"{API_ORIGIN}/api/v1/oauth/authorize"
USER_AGENT = "PedalBoard-ModelPackFetcher/1.0"


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).decode("ascii").rstrip("=")


def sanitize_stem(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9]+", "_", name.strip()).strip("_")
    return cleaned[:42] if cleaned else "Model"


def target_subdir(excel_no: int) -> str:
    if 59 <= excel_no <= 129:
        return "amps"
    if 132 <= excel_no <= 201:
        return "cabs"
    if 252 <= excel_no <= 260:
        return "reverbs"
    return "pedals"


def shared_alias_no(excel_no: int) -> int | None:
    if 52 <= excel_no <= 58:
        return excel_no - 48  # shares PRE #4..#10
    return None


def parse_excel_targets(xlsx_path: pathlib.Path) -> list[dict]:
    ns = {
        "m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
        "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    }
    rows_out: list[dict] = []
    with zipfile.ZipFile(xlsx_path, "r") as zf:
        sst: list[str] = []
        if "xl/sharedStrings.xml" in zf.namelist():
            sroot = ET.fromstring(zf.read("xl/sharedStrings.xml"))
            for si in sroot.findall("m:si", ns):
                sst.append(
                    "".join(
                        t.text or ""
                        for t in si.iter(
                            "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}t"
                        )
                    )
                )

        rel_map: dict[str, str] = {}
        rroot = ET.fromstring(zf.read("xl/worksheets/_rels/sheet1.xml.rels"))
        for rel in rroot:
            rel_map[rel.attrib["Id"]] = rel.attrib.get("Target", "")

        ws = ET.fromstring(zf.read("xl/worksheets/sheet1.xml"))
        hlinks: dict[str, str] = {}
        for hl in ws.findall(".//m:hyperlinks/m:hyperlink", ns):
            ref = hl.attrib.get("ref", "")
            rid = hl.attrib.get(
                "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id",
                "",
            )
            if rid in rel_map:
                hlinks[ref] = rel_map[rid]

        for row in ws.findall(".//m:sheetData/m:row", ns):
            rnum = int(row.attrib.get("r", "0"))
            if rnum < 2:
                continue
            vals: dict[str, str] = {}
            url = ""
            for c in row.findall("m:c", ns):
                ref = c.attrib.get("r", "")
                col = "".join(ch for ch in ref if ch.isalpha())
                t = c.attrib.get("t", "")
                if t == "s":
                    v_el = c.find("m:v", ns)
                    val = sst[int(v_el.text)] if (v_el is not None and v_el.text) else ""
                elif t == "inlineStr":
                    val = "".join(
                        el.text or ""
                        for el in c.iter(
                            "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}t"
                        )
                    )
                else:
                    v_el = c.find("m:v", ns)
                    val = v_el.text if (v_el is not None and v_el.text) else ""
                vals[col] = val
                if ref in hlinks:
                    url = hlinks[ref]

            if not url:
                continue
            a_str = vals.get("A", "").strip()
            if not a_str.isdigit():
                continue
            excel_no = int(a_str)
            mod = vals.get("B", "").strip()
            name = vals.get("C", "").strip()
            orig_model = vals.get("E", "").strip()
            parsed = urllib.parse.urlparse(url)
            qs = urllib.parse.parse_qs(parsed.query)
            query_str = qs.get("q", [""])[0].strip()
            gears = qs.get("gears", [""])[0].strip()
            fmt = qs.get("format", [""])[0].strip()
            is_ir = (
                fmt.lower() == "ir"
                or (132 <= excel_no <= 201)
                or excel_no == 11
                or (252 <= excel_no <= 260)
            )
            rows_out.append(
                {
                    "excel_no": excel_no,
                    "module": mod,
                    "name": name,
                    "orig_model": orig_model,
                    "url": url,
                    "is_t3k": "tone3000.com" in parsed.netloc.lower(),
                    "query": query_str,
                    "gears": gears,
                    "format": "ir" if is_ir else "nam",
                    "is_ir": is_ir,
                }
            )
    return rows_out


def existing_excel_ids(modelpacks_dir: pathlib.Path) -> dict[int, pathlib.Path]:
    found: dict[int, pathlib.Path] = {}
    if not modelpacks_dir.is_dir():
        return found
    for f in modelpacks_dir.rglob("*"):
        if not f.is_file():
            continue
        if f.suffix.lower() not in {".nam", ".wav", ".aif", ".aiff"}:
            continue
        m = re.match(r"^(\d{1,3})(?:[^0-9]|$)", f.stem)
        if m:
            num = int(m.group(1))
            if 1 <= num <= 265:
                found[num] = f
    return found


def token_cache_path() -> pathlib.Path:
    appdata = os.environ.get("APPDATA")
    if appdata:
        base = pathlib.Path(appdata) / "PedalBoard"
    else:
        base = pathlib.Path.home() / ".pedalboard"
    base.mkdir(parents=True, exist_ok=True)
    return base / "t3k_fetcher_session.json"


def http_json(
    url: str,
    method: str = "GET",
    headers: dict[str, str] | None = None,
    data: bytes | None = None,
    timeout: int = 30,
) -> tuple[int, dict | list | None, str]:
    req_headers = {"User-Agent": USER_AGENT, "Accept": "application/json"}
    if headers:
        req_headers.update(headers)
    req = urllib.request.Request(url, data=data, headers=req_headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
            try:
                return resp.status, json.loads(raw), raw
            except json.JSONDecodeError:
                return resp.status, None, raw
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", errors="replace") if e.fp else str(e)
        parsed = None
        try:
            parsed = json.loads(raw)
        except Exception:
            pass
        return e.code, parsed, raw
    except Exception as e:
        return 0, None, str(e)


def http_bytes(
    url: str, headers: dict[str, str] | None = None, timeout: int = 60
) -> tuple[int, bytes, str]:
    req_headers = {"User-Agent": USER_AGENT}
    if headers:
        req_headers.update(headers)
    req = urllib.request.Request(url, headers=req_headers, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read(), ""
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", errors="replace") if e.fp else str(e)
        return e.code, b"", raw
    except Exception as e:
        return 0, b"", str(e)


class T3KSession:
    def __init__(self, client_id: str, cache_file: pathlib.Path):
        self.client_id = client_id.strip()
        self.cache_file = cache_file
        self.access_token = ""
        self.refresh_token = ""
        self.expires_at = 0.0
        self._load_cache()

    def _load_cache(self) -> None:
        if not self.cache_file.is_file():
            return
        try:
            obj = json.loads(self.cache_file.read_text(encoding="utf-8"))
            if not self.client_id:
                self.client_id = obj.get("client_id", "")
            if obj.get("client_id") == self.client_id:
                self.access_token = obj.get("access_token", "")
                self.refresh_token = obj.get("refresh_token", "")
                self.expires_at = float(obj.get("expires_at", 0.0))
        except Exception:
            pass

    def _save_cache(self) -> None:
        obj = {
            "client_id": self.client_id,
            "access_token": self.access_token,
            "refresh_token": self.refresh_token,
            "expires_at": self.expires_at,
        }
        self.cache_file.write_text(json.dumps(obj, indent=2), encoding="utf-8")

    def ensure_authenticated(self, force_login: bool = False) -> bool:
        if not force_login and self.access_token and time.time() < self.expires_at - 60:
            return True
        if not force_login and self.refresh_token:
            if self._refresh():
                return True
        return self._oauth_pkce_login()

    def _refresh(self) -> bool:
        body = urllib.parse.urlencode(
            {
                "grant_type": "refresh_token",
                "refresh_token": self.refresh_token,
                "client_id": self.client_id,
            }
        ).encode("utf-8")
        status, js, _ = http_json(
            TOKEN_ENDPOINT,
            method="POST",
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            data=body,
        )
        if status == 200 and isinstance(js, dict) and js.get("access_token"):
            self.access_token = js["access_token"]
            if js.get("refresh_token"):
                self.refresh_token = js["refresh_token"]
            self.expires_at = time.time() + float(js.get("expires_in", 3600))
            self._save_cache()
            return True
        return False

    def _oauth_pkce_login(self) -> bool:
        if not self.client_id:
            print(
                "[ERROR] Publishable Key (t3k_pub_...) tidak ditemukan.\n"
                "Dapatkan gratis di https://www.tone3000.com/settings lalu jalankan:\n"
                "  python tools/fetch_modelpacks.py --key t3k_pub_...",
                file=sys.stderr,
            )
            return False

        code_verifier = b64url(secrets.token_bytes(32))
        code_challenge = b64url(hashlib.sha256(code_verifier.encode("ascii")).digest())
        expected_state = b64url(secrets.token_bytes(16))

        result: dict[str, str] = {}
        done_evt = threading.Event()

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, format: str, *args) -> None:
                return

            def do_GET(self) -> None:
                parsed = urllib.parse.urlparse(self.path)
                if parsed.path not in ("/callback", "/"):
                    self.send_response(404)
                    self.end_headers()
                    return
                qs = urllib.parse.parse_qs(parsed.query)
                for k in ("code", "state", "error", "canceled"):
                    if k in qs and qs[k]:
                        result[k] = qs[k][0]
                html = (
                    "<!doctype html><html><body style='background:#0d1118;color:#f1f5f9;"
                    "font-family:Segoe UI,sans-serif;display:flex;align-items:center;"
                    "justify-content:center;height:100vh;margin:0'>"
                    "<div style='padding:32px 40px;border:1px solid #263144;border-radius:10px;"
                    "background:#141923;text-align:center'>"
                    "<h2 style='color:#38bdf8;margin:0 0 8px'>TONE3000 Connected</h2>"
                    "<p style='color:#94a3b8;margin:0'>Silakan tutup tab ini dan kembali ke terminal.</p>"
                    "</div></body></html>"
                ).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(html)))
                self.end_headers()
                self.wfile.write(html)
                done_evt.set()

        httpd = socketserver.TCPServer(("127.0.0.1", 0), Handler)
        port = httpd.server_address[1]
        redirect_uri = f"http://127.0.0.1:{port}/callback"
        srv_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        srv_thread.start()

        auth_url = (
            AUTHORIZE_ENDPOINT
            + "?"
            + urllib.parse.urlencode(
                {
                    "client_id": self.client_id,
                    "redirect_uri": redirect_uri,
                    "response_type": "code",
                    "code_challenge": code_challenge,
                    "code_challenge_method": "S256",
                    "state": expected_state,
                }
            )
        )
        print("\n[OAuth] Membuka browser untuk login & Authorize TONE3000...")
        print(f"[OAuth] Jika browser tidak terbuka otomatis, buka URL ini:\n  {auth_url}\n")
        webbrowser.open(auth_url)

        got_cb = done_evt.wait(timeout=300)
        httpd.shutdown()
        httpd.server_close()

        if not got_cb:
            print("[ERROR] Timeout menunggu login OAuth (5 menit).", file=sys.stderr)
            return False
        if result.get("state") != expected_state:
            print("[ERROR] OAuth state mismatch.", file=sys.stderr)
            return False
        if result.get("error"):
            print(f"[ERROR] TONE3000 OAuth error: {result['error']}", file=sys.stderr)
            return False
        code = result.get("code", "")
        if not code:
            print("[ERROR] Authorization code kosong / dibatalkan.", file=sys.stderr)
            return False

        body = urllib.parse.urlencode(
            {
                "grant_type": "authorization_code",
                "code": code,
                "code_verifier": code_verifier,
                "redirect_uri": redirect_uri,
                "client_id": self.client_id,
            }
        ).encode("utf-8")
        status, js, raw = http_json(
            TOKEN_ENDPOINT,
            method="POST",
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            data=body,
        )
        if status == 200 and isinstance(js, dict) and js.get("access_token"):
            self.access_token = js["access_token"]
            self.refresh_token = js.get("refresh_token", "")
            self.expires_at = time.time() + float(js.get("expires_in", 3600))
            self._save_cache()
            print("[OAuth] Berhasil terhubung ke TONE3000!")
            return True

        print(f"[ERROR] Token exchange gagal (HTTP {status}): {raw}", file=sys.stderr)
        return False

    def api_get(self, path_and_query: str) -> tuple[int, dict | list | None, str]:
        if time.time() >= self.expires_at - 60 and self.refresh_token:
            self._refresh()
        url = API_ORIGIN + path_and_query
        for attempt in range(4):
            status, js, raw = http_json(
                url, headers={"Authorization": f"Bearer {self.access_token}"}
            )
            if status == 401 and self.refresh_token and attempt == 0:
                if self._refresh():
                    continue
            if status == 429:
                wait_s = min(30, 3 * (2**attempt))
                print(f"  [RateLimit 429] menunggu {wait_s}s...")
                time.sleep(wait_s)
                continue
            return status, js, raw
        return 429, None, "Rate limit exceeded"

    def download_url(self, model_url: str) -> tuple[int, bytes, str]:
        parsed = urllib.parse.urlparse(model_url)
        is_t3k_host = "tone3000.com" in parsed.netloc.lower() or not parsed.netloc
        full_url = model_url if parsed.scheme else (API_ORIGIN + model_url)
        headers = (
            {"Authorization": f"Bearer {self.access_token}"} if is_t3k_host else {}
        )
        for attempt in range(4):
            status, data, err = http_bytes(full_url, headers=headers)
            if status == 401 and is_t3k_host and self.refresh_token and attempt == 0:
                if self._refresh():
                    headers = {"Authorization": f"Bearer {self.access_token}"}
                    continue
            if status == 429:
                wait_s = min(30, 3 * (2**attempt))
                print(f"  [RateLimit 429] menunggu {wait_s}s...")
                time.sleep(wait_s)
                continue
            return status, data, err
        return 429, b"", "Rate limit exceeded"


def choose_best_model(models: list[dict], want_ir: bool) -> dict | None:
    if not models:
        return None

    def ext_of(m: dict) -> str:
        u = urllib.parse.urlparse(m.get("model_url", "")).path.lower()
        for ext in (".wav", ".aiff", ".aif", ".nam"):
            if u.endswith(ext):
                return ext
        return ".nam"

    def score(m: dict) -> tuple[int, int, int]:
        ext = ext_of(m)
        is_ir = ext in (".wav", ".aif", ".aiff")
        if want_ir != is_ir:
            return (-100, 0, 0)
        name = (m.get("name") or "").lower()
        arch = str(m.get("architecture_version") or "")
        size = (m.get("size") or "").lower()
        # Prefer standard A1/standard size for maximum PedalBoard NamModelRunner fidelity
        arch_score = 3 if arch in ("1", "") else (2 if arch == "2" else 1)
        size_score = 3 if "standard" in size else (2 if "lite" in size or "feather" in size else 1)
        # Mild preference for balanced/neutral capture names (SM57 / mid / balanced / normal)
        name_bonus = 0
        for kw in ("sm57", "balanced", "mid", "normal", "stock", "clean", "crunch", "lead"):
            if kw in name:
                name_bonus += 1
        return (arch_score, size_score, name_bonus)

    ranked = sorted(models, key=score, reverse=True)
    best = ranked[0]
    if score(best)[0] < 0:
        return None
    return best


def query_models_for_tone(session: T3KSession, tone_id: int) -> list[dict]:
    models: list[dict] = []
    seen_ids: set[int] = set()
    for arch in (0, 2):
        params = {"tone_id": str(tone_id), "page_size": "100"}
        if arch > 0:
            params["architecture"] = str(arch)
        status, js, _ = session.api_get(
            "/api/v1/models?" + urllib.parse.urlencode(params)
        )
        if status != 200 or js is None:
            continue
        arr = js.get("data", []) if isinstance(js, dict) else js
        if isinstance(arr, list):
            for item in arr:
                mid = int(item.get("id", 0))
                if mid not in seen_ids:
                    seen_ids.add(mid)
                    models.append(item)
        if models and arch == 0:
            break
    return models


def search_and_download_t3k(
    session: T3KSession,
    rows: list[dict],
    out_dir: pathlib.Path,
    delay_sec: float,
    limit: int | None,
) -> tuple[int, int, list[dict]]:
    existing = existing_excel_ids(out_dir)
    downloaded = 0
    skipped = 0
    failures: list[dict] = []

    t3k_rows = [r for r in rows if r["is_t3k"]]
    # Deduplicate shared PRE (#4..#10) <-> DST (#52..#58)
    todo: list[dict] = []
    for r in t3k_rows:
        eno = r["excel_no"]
        alias = shared_alias_no(eno)
        if eno in existing or (alias is not None and alias in existing):
            skipped += 1
            continue
        if alias is not None:
            # Will share file with #4..#10 once #4..#10 is downloaded
            skipped += 1
            continue
        todo.append(r)

    if limit is not None:
        todo = todo[:limit]

    print(
        f"[TONE3000] Total T3K target={len(t3k_rows)} | Sudah ada/alias={skipped} | Akan diunduh={len(todo)}"
    )

    for idx, r in enumerate(todo, 1):
        eno = r["excel_no"]
        query = r["query"] or r["orig_model"] or r["name"]
        want_ir = r["is_ir"]

        search_attempts = [
            {
                "query": query,
                "sort": "downloads-all-time",
                "page": "1",
                "page_size": "12",
                **({"gears": r["gears"]} if r["gears"] else {}),
                **({"format": "ir"} if want_ir else {}),
            },
            {
                "query": query,
                "sort": "best-match",
                "page": "1",
                "page_size": "12",
                **({"format": "ir"} if want_ir else {}),
            },
        ]

        tones: list[dict] = []
        last_status = 0
        last_err = ""
        for params in search_attempts:
            status, js, raw = session.api_get(
                "/api/v1/tones/search?" + urllib.parse.urlencode(params)
            )
            last_status = status
            last_err = raw[:200]
            if status == 403:
                print(
                    "\n[STOP - HTTP 403] API key TONE3000 Anda berada di tier yang membatasi endpoint "
                    "/api/v1/tones/search.\n"
                    "Silakan gunakan Jalur 2B (login langsung di Chrome) atau upgrade/cek izin API key Anda.",
                    file=sys.stderr,
                )
                return downloaded, skipped, [
                    {"excel_no": eno, "name": r["name"], "reason": f"HTTP 403: {last_err}"}
                ]
            if status == 200 and js is not None:
                arr = js.get("data", []) if isinstance(js, dict) else js
                if isinstance(arr, list) and arr:
                    tones = arr
                    break
            time.sleep(0.4)

        if not tones:
            print(f"[{idx}/{len(todo)}] MISS #{eno:03d} {r['name']} (q='{query}', status={last_status})")
            failures.append({"excel_no": eno, "name": r["name"], "query": query, "reason": f"No tones (HTTP {last_status})"})
            continue

        saved = False
        for tone in tones[:5]:
            tid = int(tone.get("id", 0))
            if tid <= 0:
                continue
            models = query_models_for_tone(session, tid)
            chosen = choose_best_model(models, want_ir=want_ir)
            if not chosen or not chosen.get("model_url"):
                continue

            d_status, blob, d_err = session.download_url(chosen["model_url"])
            if d_status != 200 or len(blob) < 64:
                continue

            url_path = urllib.parse.urlparse(chosen["model_url"]).path.lower()
            ext = ".wav" if want_ir else ".nam"
            for cand_ext in (".wav", ".aiff", ".aif", ".nam"):
                if url_path.endswith(cand_ext):
                    ext = cand_ext
                    break

            sub = out_dir / target_subdir(eno)
            sub.mkdir(parents=True, exist_ok=True)
            out_path = sub / f"{eno:03d}_{sanitize_stem(r['name'])}{ext}"
            out_path.write_bytes(blob)
            downloaded += 1
            saved = True
            print(
                f"[{idx}/{len(todo)}] OK   #{eno:03d} {r['name']} -> {out_path.relative_to(out_dir)} "
                f"({len(blob)/1024:.1f} KB | tone='{tone.get('title','')[:40]}')"
            )
            break

        if not saved:
            print(f"[{idx}/{len(todo)}] FAIL #{eno:03d} {r['name']} (tidak ada model cocok di top 5 tone)")
            failures.append({"excel_no": eno, "name": r["name"], "query": query, "reason": "No downloadable matching model"})

        if delay_sec > 0:
            time.sleep(delay_sec)

    return downloaded, skipped, failures


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch GP-200 ModelPacks via official TONE3000 OAuth API")
    parser.add_argument("--key", default=os.environ.get("T3K_PUBLISHABLE_KEY", ""), help="TONE3000 Publishable Key (t3k_pub_...)")
    parser.add_argument("--xlsx", default="Daftar_Effect_GP-200_Checklist_Gratis.xlsx")
    parser.add_argument("--out", default="assets/modelpacks")
    parser.add_argument("--delay", type=float, default=0.6, help="Delay seconds between API calls")
    parser.add_argument("--limit", type=int, default=None, help="Optional max count for testing")
    parser.add_argument("--reauth", action="store_true", help="Force fresh OAuth login")
    parser.add_argument("--probe-only", action="store_true", help="Verify login and /api/v1/tones/search access only")
    args = parser.parse_args()

    xlsx_path = pathlib.Path(args.xlsx).resolve()
    out_dir = pathlib.Path(args.out).resolve()
    if not xlsx_path.is_file():
        print(f"[ERROR] Excel tidak ditemukan: {xlsx_path}", file=sys.stderr)
        return 1

    rows = parse_excel_targets(xlsx_path)
    session = T3KSession(args.key, token_cache_path())
    if not session.ensure_authenticated(force_login=args.reauth):
        return 2

    st_u, js_u, raw_u = session.api_get("/api/v1/user")
    if st_u == 200 and isinstance(js_u, dict):
        uname = js_u.get("display_name") or js_u.get("username") or "unknown"
        print(f"[TONE3000] Signed in as: {uname}")
    else:
        print(f"[WARN] /api/v1/user returned HTTP {st_u}: {raw_u[:120]}")

    # Probe search endpoint first so we fail fast if key tier restricts search
    st_s, js_s, raw_s = session.api_get("/api/v1/tones/search?query=TS808&page=1&page_size=2")
    print(f"[TONE3000] Search API probe: HTTP {st_s}")
    if st_s == 403:
        print(
            "[ERROR] Endpoint /api/v1/tones/search dibatasi (HTTP 403) pada tier API key ini.\n"
            f"Detail: {raw_s[:200]}",
            file=sys.stderr,
        )
        return 3
    if args.probe_only:
        return 0 if st_s == 200 else 4

    downloaded, skipped, failures = search_and_download_t3k(
        session, rows, out_dir, delay_sec=args.delay, limit=args.limit
    )
    report_path = out_dir / "_fetch_report.json"
    report_path.write_text(
        json.dumps(
            {"downloaded": downloaded, "skipped": skipped, "failures": failures},
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"\n[DONE] Downloaded={downloaded}, Skipped={skipped}, Failed={len(failures)}")
    print(f"[DONE] Report saved to: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
