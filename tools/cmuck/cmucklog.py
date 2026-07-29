"""Compact Rich-backed logging for cmuck."""

from __future__ import annotations

import logging
import os
import shlex
import subprocess
from collections.abc import Sequence

from rich.console import Console
from rich.logging import RichHandler


LOGGER_NAME = "cmuck"


def configure_logging(*, verbose: bool = False, quiet: bool = False) -> logging.Logger:
    """Configure cmuck's logger without changing the root logger."""

    level = logging.DEBUG if verbose else logging.WARNING if quiet else logging.INFO
    logger = logging.getLogger(LOGGER_NAME)
    logger.handlers.clear()
    logger.setLevel(level)
    logger.propagate = False

    handler = RichHandler(
        console=Console(stderr=True),
        show_time=False,
        show_level=True,
        show_path=False,
        highlighter=None,
        markup=False,
        rich_tracebacks=verbose,
    )
    handler.setLevel(level)
    handler.setFormatter(logging.Formatter("%(message)s"))
    logger.addHandler(handler)
    return logger


def format_command(arguments: Sequence[str]) -> str:
    """Format a subprocess command using native path and quoting conventions."""

    if os.name == "nt":
        return subprocess.list2cmdline(list(arguments))
    return shlex.join(arguments)
