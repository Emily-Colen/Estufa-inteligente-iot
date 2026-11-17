
from fastapi import FastAPI, HTTPException
from sqlalchemy import text
from sqlalchemy.exc import SQLAlchemyError

from database import engine, testar_conexao

app = FastAPI(
    title="Estufa Inteligente IoT",
    version="1.0.0",
    description="API para monitoramento da estufa"
)

@app.get("/")
def inicio():
    return {
        "projeto": "Estufa Inteligente IoT",
        "status": "API funcionando"
    }

@app.get("/api/health/database")
def verificar_banco():
    try:
        banco = testar_conexao()

        return {
            "status": "conectado",
            "database": banco
        }

    except SQLAlchemyError:
        raise HTTPException(
            status_code=503,
            detail="Banco de dados indisponivel"
        )

@app.get("/api/tabelas")
def listar_tabelas():
    consulta = text("""
        SELECT table_name
        FROM information_schema.tables
        WHERE table_schema = 'public'
        ORDER BY table_name
    """)

    try:
        with engine.connect() as connection:
            resultado = connection.execute(consulta)

            tabelas = [
                linha[0]
                for linha in resultado.fetchall()
            ]

        return {
            "quantidade": len(tabelas),
            "tabelas": tabelas
        }

    except SQLAlchemyError:
        raise HTTPException(
            status_code=503,
            detail="Erro ao consultar as tabelas"
        )


import os
import secrets

from datetime import datetime
from typing import Optional

from fastapi import Header, Query
from pydantic import BaseModel, Field
from sqlalchemy import text
from sqlalchemy.exc import SQLAlchemyError

from database import engine


# Chaves armazenadas no .env
DEVICE_KEY = os.getenv("DEVICE_KEY")
ADMIN_KEY = os.getenv("ADMIN_KEY")

if not DEVICE_KEY or not ADMIN_KEY:
    raise RuntimeError(
        "Configure DEVICE_KEY e ADMIN_KEY no .env"
    )


# Estrutura das leituras do ESP32
class TelemetryData(BaseModel):
    device_id: str = Field(min_length=1, max_length=100)
    temperature: float = Field(ge=-40, le=85)
    air_humidity: float = Field(ge=0, le=100)
    soil_moisture: float = Field(ge=0, le=100)
    luminosity: Optional[float] = Field(
        default=None, ge=0
    )
    reservoir_ok: bool
    fan_on: bool
    pump_on: bool


# Receber e armazenar leituras
@app.post("/api/telemetry", status_code=201)
def receber_telemetria(
    dados: TelemetryData,
    x_device_key: str = Header(...)
):
    if not secrets.compare_digest(
        x_device_key, DEVICE_KEY
    ):
        raise HTTPException(
            status_code=401,
            detail="Dispositivo nao autorizado"
        )

    consulta = text("""
        INSERT INTO public.telemetry (
            device_id,
            temperature,
            air_humidity,
            soil_moisture,
            luminosity,
            reservoir_ok,
            fan_on,
            pump_on
        )
        VALUES (
            :device_id,
            :temperature,
            :air_humidity,
            :soil_moisture,
            :luminosity,
            :reservoir_ok,
            :fan_on,
            :pump_on
        )
        RETURNING id, created_at
    """)

    try:
        with engine.begin() as conn:
            dispositivo = conn.execute(
                text("""
                    SELECT device_id
                    FROM public.devices
                    WHERE device_id = :device_id
                """),
                {"device_id": dados.device_id}
            ).first()

            if dispositivo is None:
                raise HTTPException(
                    status_code=404,
                    detail="Dispositivo nao cadastrado"
                )

            resultado = conn.execute(
                consulta,
                dados.model_dump()
            ).mappings().one()

            return {
                "status": "armazenado",
                "id": resultado["id"],
                "created_at": resultado[
                    "created_at"
                ].isoformat()
            }

    except SQLAlchemyError:
        raise HTTPException(
            status_code=503,
            detail="Falha ao armazenar leitura"
        )


# Consultar ultima leitura
@app.get("/api/telemetry/latest")
def ultima_telemetria(
    device_id: str = Query(default="estufa-01"),
    x_admin_key: str = Header(...)
):
    if not secrets.compare_digest(
        x_admin_key, ADMIN_KEY
    ):
        raise HTTPException(
            status_code=401,
            detail="Acesso nao autorizado"
        )

    consulta = text("""
        SELECT
            id,
            device_id,
            temperature,
            air_humidity,
            soil_moisture,
            luminosity,
            reservoir_ok,
            fan_on,
            pump_on,
            created_at
        FROM public.telemetry
        WHERE device_id = :device_id
        ORDER BY created_at DESC, id DESC
        LIMIT 1
    """)

    try:
        with engine.connect() as conn:
            resultado = conn.execute(
                consulta,
                {"device_id": device_id}
            ).mappings().first()

        if resultado is None:
            raise HTTPException(
                status_code=404,
                detail="Nenhuma medicao encontrada"
            )

        return dict(resultado)

    except SQLAlchemyError:
        raise HTTPException(
            status_code=503,
            detail="Falha ao consultar o banco"
        )


@app.get("/api/telemetry/history")
def historico_telemetria(
    device_id: str = Query(default="estufa-01"),
    limit: int = Query(default=100, ge=1, le=1000),
    x_admin_key: str = Header(...)
):
    if not secrets.compare_digest(
        x_admin_key, ADMIN_KEY
    ):
        raise HTTPException(
            status_code=401,
            detail="Acesso nao autorizado"
        )

    consulta = text("""
        SELECT
            id,
            device_id,
            temperature,
            air_humidity,
            soil_moisture,
            luminosity,
            reservoir_ok,
            fan_on,
            pump_on,
            created_at
        FROM public.telemetry
        WHERE device_id = :device_id
        ORDER BY created_at DESC, id DESC
        LIMIT :limit
    """)

    try:
        with engine.connect() as conn:
            registros = conn.execute(
                consulta,
                {
                    "device_id": device_id,
                    "limit": limit
                }
            ).mappings().all()

        return {
            "device_id": device_id,
            "total": len(registros),
            "data": [
                dict(registro)
                for registro in reversed(registros)
            ]
        }

    except SQLAlchemyError:
        raise HTTPException(
            status_code=503,
            detail="Erro ao consultar historico"
        )
