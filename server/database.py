
import os

from dotenv import load_dotenv
from sqlalchemy import create_engine, text

load_dotenv()

DATABASE_URL = os.getenv("DATABASE_URL")

if not DATABASE_URL:
    raise RuntimeError(
        "DATABASE_URL nao encontrada no arquivo .env"
    )

# Utilizar driver PostgreSQL Psycopg 3
DATABASE_URL = DATABASE_URL.replace(
    "postgresql://",
    "postgresql+psycopg://",
    1
)

engine = create_engine(
    DATABASE_URL,
    pool_pre_ping=True,
    pool_size=2,
    max_overflow=0,
    connect_args={"connect_timeout": 10}
)

def testar_conexao():
    with engine.connect() as connection:
        resultado = connection.execute(
            text("SELECT current_database()")
        )
        return resultado.scalar_one()
