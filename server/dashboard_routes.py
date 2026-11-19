"""Adicione register_dashboard(app) ao app.py existente, após criar app."""
from pathlib import Path
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

STATIC = Path(__file__).resolve().parent / "dashboard_static"

def register_dashboard(app: FastAPI) -> None:
    """Registra dashboard sem substituir as rotas de API existentes."""
    app.mount("/estufa-assets", StaticFiles(directory=str(STATIC)), name="estufa-assets")

    @app.get("/dashboard", include_in_schema=False)
    def estufa_dashboard():
        return FileResponse(STATIC / "index.html", media_type="text/html")
