"""Coleta somente páginas públicas do tabuademares.com, com cache no chamador."""
from __future__ import annotations

import re
from datetime import date, datetime
from typing import Any

import requests
from bs4 import BeautifulSoup

BASE = "https://tabuademares.com/br/bahia/lauro-de-freitas"
HEADERS = {"User-Agent": "lauro-freitas-tide-api/1.0 (ESP32 personal dashboard)"}


def _num(value: str | None) -> float | None:
    if not value:
        return None
    value = value.replace(",", ".")
    m = re.search(r"-?\d+(?:\.\d+)?", value)
    return float(m.group()) if m else None


def _get(url: str) -> BeautifulSoup:
    r = requests.get(url, headers=HEADERS, timeout=20)
    r.raise_for_status()
    return BeautifulSoup(r.text, "html.parser")


def _parse_card(card: Any, year: int) -> dict[str, Any] | None:
    day = card.select_one(".f_circulo .dia")
    month = card.select_one(".f_circulo .mes")
    if not day or not month:
        return None
    months = {"JAN": 1, "FEV": 2, "MAR": 3, "ABR": 4, "MAI": 5, "JUN": 6,
              "JUL": 7, "AGO": 8, "SET": 9, "OUT": 10, "NOV": 11, "DEZ": 12}
    month_no = months.get(month.get_text(strip=True).upper())
    if not month_no:
        return None
    d = date(year, month_no, int(day.get_text(strip=True)))
    tides = []
    for row in card.select("table.f_tabla_mareas tr"):
        cells = row.select("td")
        if len(cells) < 4:
            continue
        kind = "high" if "azul" in (row.get("class") or []) else "low"
        time = cells[1].get_text(" ", strip=True)
        height = _num(cells[2].get_text(" ", strip=True))
        coefficient = _num(cells[3].get_text(" ", strip=True))
        if re.fullmatch(r"\d{1,2}:\d{2}", time):
            tides.append({"type": kind, "time": time, "height_m": height, "coefficient": int(coefficient) if coefficient is not None else None})
    coeff = card.select_one(".grafico_amplitud_barra_relleno")
    coeff_text = coeff.get_text(" ", strip=True) if coeff else ""
    coeffs = [int(x) for x in re.findall(r"\d+", coeff_text)]
    return {
        "date": d.isoformat(),
        "weekday": (card.select_one(".text_dia").get_text(strip=True) if card.select_one(".text_dia") else None),
        "coefficient_min": min(coeffs) if coeffs else None,
        "coefficient_max": max(coeffs) if coeffs else None,
        "tides": tides,
    }


def fetch_7_days() -> list[dict[str, Any]]:
    soup = _get(BASE + "/previsao/mares")
    result = []
    for card in soup.select(".ficha"):
        parsed = _parse_card(card, datetime.now().year)
        if parsed:
            result.append(parsed)
    return result


def _first_time(soup: BeautifulSoup, selectors: list[str]) -> str | None:
    for selector in selectors:
        node = soup.select_one(selector)
        if node:
            m = re.search(r"\b\d{1,2}:\d{2}(?::\d{2})?\b", node.get_text(" ", strip=True))
            if m:
                return m.group()
    return None


def fetch_detail() -> dict[str, Any]:
    """Extrai dados complementares de hoje; campos ausentes permanecem null."""
    soup = _get(BASE)
    text = soup.get_text(" ", strip=True)
    out: dict[str, Any] = {}
    # O gráfico usa os mesmos nós para a saída e o pôr do sol.
    solar = soup.select(".l_grafico_mareas_responsive_hora_salida_puesta_sol")
    if len(solar) >= 2:
        out["sunrise"] = solar[0].get_text(strip=True)
        out["sunset"] = solar[1].get_text(strip=True)
    else:
        out["sunrise"] = out["sunset"] = None
    phase = re.search(r"A fase lunar é\s+([^\.]+)", text, re.I)
    out["moon_phase"] = phase.group(1).strip() if phase else None
    age_node = soup.select_one("#grafico_edad_lunar_texto1")
    illumination_node = soup.select_one("#grafico_iluminacion_texto1")
    out["moon_age_days"] = _num(age_node.get_text(strip=True)) if age_node else None
    out["moon_illumination_percent"] = int(_num(illumination_node.get_text(strip=True))) if illumination_node else None
    activity = soup.select_one(".salida_puesta_luna_actividad_dia_titulo1 strong")
    out["solunar_activity"] = activity.get_text(" ", strip=True) if activity else None
    periods = []
    for node in soup.select(".salida_puesta_luna_periodo_datos"):
        raw = node.get_text(" ", strip=True)
        times = re.findall(r"\b\d{1,2}:\d{2}\b", raw)
        if len(times) >= 2:
            periods.append({"start": times[0], "end": times[1], "description": raw[:180]})
    out["solunar_periods"] = periods
    return out
