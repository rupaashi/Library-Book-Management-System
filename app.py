"""Vercel entry point.

Vercel looks for a Flask object named `app` in a file called app.py at the
project root. The real application lives in backend/app.py, so this small
file just makes backend/ importable and re-exports that app. Nothing in the
backend folder needs to change.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "backend"))

from backend.app import app  # noqa: E402,F401