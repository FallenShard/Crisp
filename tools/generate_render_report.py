#!/usr/bin/env python3
"""Generate a self-contained HTML report from evaluate_renderings.py JSON output."""

from __future__ import annotations

import argparse
import base64
import html
import json
import mimetypes
from datetime import UTC, datetime
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, help="JSON report produced by evaluate_renderings.py")
    parser.add_argument("--output", type=Path, help="Output HTML path; defaults to the report path with .html")
    parser.add_argument("--title", default="Crisp rendering evaluation", help="HTML report title")
    return parser.parse_args()


def format_number(value: Any, digits: int = 5) -> str:
    if value is None:
        return "--"
    return f"{float(value):.{digits}g}"


def resolve_artifact(path_value: str, report_path: Path) -> Path:
    path = Path(path_value)
    return path if path.is_absolute() else report_path.parent / path


def image_data_uri(path_value: str | None, report_path: Path) -> str | None:
    if not path_value:
        return None
    path = resolve_artifact(path_value, report_path)
    if not path.is_file():
        return None
    mime_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:{mime_type};base64,{encoded}"


def image_cell(
    artifact_path: str | None,
    report_path: Path,
    label: str,
    caption: str | None = None,
) -> str:
    uri = image_data_uri(artifact_path, report_path)
    if uri is None:
        return '<div class="missing">Not generated</div>'
    caption_html = f'<div class="caption">{html.escape(caption)}</div>' if caption else ""
    return (
        f'<img src="{uri}" alt="{html.escape(label)}" loading="lazy" '
        f'title="Open {html.escape(label)}" onclick="window.open(this.src, \'_blank\')">{caption_html}'
    )


def metric_rows(result: dict[str, Any]) -> str:
    flip = result.get("flip", {})
    rows = (
        ("Mean FLIP", flip.get("mean")),
        ("FLIP P95", flip.get("p95")),
        ("Relative L2", result.get("relativeL2")),
        ("HDR PSNR", result.get("psnrDb"), " dB"),
        ("MAE", result.get("meanAbsoluteError")),
        ("RMSE", result.get("rootMeanSquaredError")),
        ("Abs. error P95", result.get("absoluteErrorP95")),
        ("Abs. error P99", result.get("absoluteErrorP99")),
        ("Luminance bias", result.get("signedRelativeMeanLuminanceError"), "%"),
    )
    rendered = []
    for row in rows:
        label, value = row[0], row[1]
        suffix = row[2] if len(row) > 2 else ""
        digits = row[3] if len(row) > 3 else 5
        if suffix == "%" and value is not None:
            value = float(value) * 100.0
        rendered.append(
            f"<tr><th>{html.escape(label)}</th><td>{format_number(value, digits)}{html.escape(suffix)}</td></tr>"
        )
    if "error" in flip:
        rendered.append(f'<tr><th>FLIP error</th><td class="error">{html.escape(str(flip["error"]))}</td></tr>')
    return "".join(rendered)


def scene_row(
    relative_path: str,
    result: dict[str, Any],
    candidate_directory: str,
    report_path: Path,
) -> str:
    if "error" in result:
        return (
            f'<tr><th class="scene">{html.escape(relative_path)}</th>'
            f'<td colspan="5" class="error">{html.escape(str(result["error"]))}</td></tr>'
        )

    artifacts = result.get("artifacts", {})
    diff_info = result.get("absoluteDifferenceHeatmap", {})
    diff_caption = None
    if diff_info.get("scaleMax") is not None:
        diff_caption = (
            f"Inferno; top at P{format_number(diff_info.get('scalePercentile'), 4)} "
            f"= {format_number(diff_info.get('scaleMax'))}"
        )
    flip_parameters = result.get("flip", {}).get("parameters", {})
    flip_caption = None
    if flip_parameters:
        flip_caption = (
            f"HDR-FLIP; {format_number(flip_parameters.get('ppd'), 4)} PPD; "
            f"{flip_parameters.get('tonemapper', 'ACES')}"
        )

    return (
        "<tr>"
        f'<th class="scene"><div>{html.escape(Path(relative_path).stem)}</div>'
        f'<small>{html.escape(relative_path)}</small><small>{html.escape(candidate_directory)}</small></th>'
        f'<td>{image_cell(artifacts.get("referencePreview"), report_path, "Reference image")}</td>'
        f'<td>{image_cell(artifacts.get("candidatePreview"), report_path, "Candidate image")}</td>'
        f'<td>{image_cell(artifacts.get("absoluteDifferenceHeatmap"), report_path, "Absolute difference", diff_caption)}</td>'
        f'<td>{image_cell(artifacts.get("flipHeatmap"), report_path, "FLIP heatmap", flip_caption)}</td>'
        f'<td><table class="metrics">{metric_rows(result)}</table></td>'
        "</tr>"
    )


def build_html(report: dict[str, Any], report_path: Path, title: str) -> str:
    rows = []
    for run in report.get("candidateRuns", []):
        candidate_directory = str(run.get("directory", ""))
        for relative_path, result in run.get("images", {}).items():
            rows.append(scene_row(relative_path, result, candidate_directory, report_path))

    settings = report.get("settings", {})
    subtitle = (
        f"Reference: {report.get('referenceDirectory', 'unknown')} | "
        f"FLIP: {'enabled' if settings.get('flipEnabled') else 'disabled'} | "
        f"Generated {datetime.now(UTC).strftime('%Y-%m-%d %H:%M UTC')}"
    )
    table_body = "".join(rows) or '<tr><td colspan="6" class="missing">No image comparisons found.</td></tr>'
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<style>
:root {{ color-scheme: dark; --bg:#111318; --panel:#191c23; --line:#343945; --muted:#aeb5c3; --accent:#8ab4ff; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; padding:28px; background:var(--bg); color:#eef1f6; font:14px/1.45 system-ui,sans-serif; }}
h1 {{ margin:0 0 4px; font-size:26px; }}
.subtitle {{ margin-bottom:24px; color:var(--muted); overflow-wrap:anywhere; }}
.table-wrap {{ overflow-x:auto; border:1px solid var(--line); border-radius:10px; background:var(--panel); }}
table.report {{ width:100%; min-width:1500px; border-collapse:collapse; table-layout:fixed; }}
.report > thead th {{ position:sticky; top:0; z-index:2; background:#20242d; text-align:left; color:var(--muted); }}
.report > thead th:first-child {{ width:210px; }}
.report > thead th:last-child {{ width:260px; }}
.report > thead th, .report > tbody > tr > th, .report > tbody > tr > td {{ padding:12px; border:1px solid var(--line); vertical-align:top; }}
.scene {{ text-align:left; overflow-wrap:anywhere; background:#171a20; }}
.scene small {{ display:block; margin-top:8px; color:var(--muted); font-weight:400; }}
img {{ display:block; width:100%; height:auto; border-radius:5px; background:#050607; cursor:zoom-in; }}
.caption {{ margin-top:8px; color:var(--muted); font-size:12px; }}
.metrics {{ width:100%; border-collapse:collapse; }}
.metrics th, .metrics td {{ padding:4px 0; border-bottom:1px solid #2b303a; text-align:left; }}
.metrics th {{ color:var(--muted); font-weight:500; }}
.metrics td {{ text-align:right; font-variant-numeric:tabular-nums; }}
.missing {{ color:var(--muted); padding:30px 4px; text-align:center; }}
.error {{ color:#ff9b9b; overflow-wrap:anywhere; }}
@media (max-width:900px) {{ body {{ padding:14px; }} }}
</style>
</head>
<body>
<h1>{html.escape(title)}</h1>
<div class="subtitle">{html.escape(subtitle)}</div>
<div class="table-wrap">
<table class="report">
<thead><tr><th>Scene</th><th>Mitsuba reference</th><th>Crisp candidate</th><th>Absolute difference</th><th>HDR-FLIP</th><th>Metrics</th></tr></thead>
<tbody>{table_body}</tbody>
</table>
</div>
</body>
</html>
"""


def main() -> int:
    args = parse_args()
    report_path = args.report.resolve()
    if not report_path.is_file():
        raise SystemExit(f"Metrics report does not exist: {args.report}")
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"Could not read metrics report: {error}") from error

    output_path = (args.output or args.report.with_suffix(".html")).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(build_html(report, report_path, args.title), encoding="utf-8")
    print(f"Wrote self-contained HTML report to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
