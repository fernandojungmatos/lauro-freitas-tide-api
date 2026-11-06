from __future__ import annotations

import os
import threading
import time
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware

from scraper import fetch_7_days, fetch_detail

app = FastAPI(title="API de Marés — Lauro de Freitas", version="1.0.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["GET"], allow_headers=["*"])

TTL_SECONDS = int(os.getenv("CACHE_TTL_SECONDS", "1800"))
_cache: dict[str, Any] = {"updated_at": 0.0, "days": [], "detail": {}}
_lock = threading.Lock()


def _refresh(force: bool = False) -> None:
    if not force and time.time() - _cache["updated_at"] < TTL_SECONDS and _cache["days"]:
        return
    with _lock:
        if not force and time.time() - _cache["updated_at"] < TTL_SECONDS and _cache["days"]:
            return
        days = fetch_7_days()
        if not days:
            raise RuntimeError("A fonte não retornou cartões de previsão")
        _cache.update({"updated_at": time.time(), "days": days})
        try:
            _cache["detail"] = fetch_detail()
        except Exception:
            _cache["detail"] = {}


def _payload(include_detail: bool = True) -> dict[str, Any]:
    fetched = datetime.fromtimestamp(_cache["updated_at"], tz=timezone.utc).isoformat()
    return {
        "location": {"name": "Lauro de Freitas", "state": "BA", "country": "BR", "timezone": "America/Bahia"},
        "source": "https://tabuademares.com/br/bahia/lauro-de-freitas",
        "fetched_at": fetched,
        "data_date": datetime.now().date().isoformat(),
        "days": _cache["days"],
        "today_detail": _cache["detail"] if include_detail else None,
    }


@app.get("/health")
def health() -> dict[str, Any]:
    return {"ok": True, "service": "lauro-freitas-tide-api", "cache_age_seconds": round(time.time() - _cache["updated_at"]) if _cache["updated_at"] else None}


@app.get("/api/v1/forecast")
def forecast(force_refresh: bool = Query(False, description="Ignora o cache; use com moderação")) -> dict[str, Any]:
    try:
        _refresh(force_refresh)
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"Falha ao consultar a fonte: {exc}") from exc
    return _payload(include_detail=True)


@app.get("/api/v1/day/{iso_date}")
def day(iso_date: str, force_refresh: bool = False) -> dict[str, Any]:
    try:
        requested = datetime.strptime(iso_date, "%Y-%m-%d").date().isoformat()
    except ValueError as exc:
        raise HTTPException(status_code=400, detail="Use a data no formato YYYY-MM-DD") from exc
    try:
        _refresh(force_refresh)
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"Falha ao consultar a fonte: {exc}") from exc
    found = next((d for d in _cache["days"] if d["date"] == requested), None)
    if not found:
        raise HTTPException(status_code=404, detail="A fonte disponibiliza somente a janela pública de previsão atual")
    result = _payload(include_detail=requested == datetime.now().date().isoformat())
    result["day"] = found
    result.pop("days", None)
    return result


@app.get("/api/v1/esp32")
def esp32() -> dict[str, Any]:
    """Resposta sem textos longos, ideal para um display pequeno."""
    try:
        _refresh(False)
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"Falha ao consultar a fonte: {exc}") from exc
    today = _cache["days"][0] if _cache["days"] else None
    return {"date": today["date"] if today else None, "location": "Lauro de Freitas", "tides": today["tides"] if today else [], "coefficient": {"min": today["coefficient_min"] if today else None, "max": today["coefficient_max"] if today else None}, "sun": {k: _cache["detail"].get(k) for k in ("sunrise", "sunset")}, "moon": {k: _cache["detail"].get(k) for k in ("moon_phase", "moon_age_days", "moon_illumination_percent")}, "activity": _cache["detail"].get("solunar_activity"), "updated_at": datetime.fromtimestamp(_cache["updated_at"], tz=timezone.utc).isoformat()}
