from pathlib import Path
from unittest.mock import patch

from scraper import fetch_7_days

HTML = Path('/home/ubuntu/upload/mares.html').read_text()


def test_parse_public_forecast_cards():
    class Response:
        text = HTML
        def raise_for_status(self):
            pass
    with patch('scraper.requests.get', return_value=Response()):
        days = fetch_7_days()
    assert len(days) >= 1
    assert days[0]['date']
    assert len(days[0]['tides']) >= 2
    assert {'type', 'time', 'height_m', 'coefficient'} <= days[0]['tides'][0].keys()
