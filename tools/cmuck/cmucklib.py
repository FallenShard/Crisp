"""Core preset, CMake File API, and CTest support for cmuck."""

from __future__ import annotations

import json
import logging
import os
import platform
import re
import shutil
import subprocess
import time
from dataclasses import dataclass
from difflib import get_close_matches
from pathlib import Path
from typing import Any, Iterable, Sequence, cast

from cmucklog import format_command


CMUCK_VENDOR_KEY = "crisp.dev/cmuck/1.0"
logger = logging.getLogger("cmuck")


class CmuckError(RuntimeError):
    """An actionable user-facing cmuck error."""


def find_project_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()
    for candidate in (current, *current.parents):
        if (candidate / "CMakePresets.json").is_file():
            return candidate
    raise CmuckError("could not find CMakePresets.json in this directory or any parent")


def _deep_merge(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    result = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _deep_merge(
                cast(dict[str, Any], result[key]),
                cast(dict[str, Any], value),
            )
        else:
            result[key] = value
    return result


def _expand_macros(value: str, root: Path, preset_name: str) -> str:
    replacements = {
        "${sourceDir}": str(root),
        "${sourceParentDir}": str(root.parent),
        "${sourceDirName}": root.name,
        "${presetName}": preset_name,
    }
    for macro, replacement in replacements.items():
        value = value.replace(macro, replacement)
    value = re.sub(r"\$(?:p?env)\{([^}]+)\}", lambda match: os.environ.get(match.group(1), ""), value)
    return value.replace("$$", "$")


@dataclass(frozen=True)
class PresetSelection:
    configure_name: str
    configure: dict[str, Any]
    build_name: str | None
    test_name: str | None
    build_dir: Path

    @property
    def configuration(self) -> str | None:
        cache_variables = self.configure.get("cacheVariables", {})
        if not isinstance(cache_variables, dict):
            return None
        value = cast(dict[str, Any], cache_variables).get("CMAKE_BUILD_TYPE")
        if isinstance(value, dict):
            value = cast(dict[str, Any], value).get("value")
        return str(value) if value else None


class PresetCatalog:
    def __init__(
        self,
        root: Path,
        configure: dict[str, dict[str, Any]],
        build: dict[str, dict[str, Any]],
        test: dict[str, dict[str, Any]],
        mode_aliases: dict[str, str],
    ) -> None:
        self.root = root
        self.configure = configure
        self.build = build
        self.test = test
        self.mode_aliases = mode_aliases

    @classmethod
    def load(cls, root: Path) -> "PresetCatalog":
        raw: dict[str, dict[str, dict[str, Any]]] = {
            "configurePresets": {},
            "buildPresets": {},
            "testPresets": {},
        }
        mode_aliases: dict[str, str] = {}
        visited: set[Path] = set()

        def load_file(path: Path) -> None:
            path = path.resolve()
            if path in visited or not path.is_file():
                return
            visited.add(path)
            try:
                document = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError) as error:
                raise CmuckError(f"could not read preset file {path}: {error}") from error
            if path == (root / "CMakePresets.json").resolve():
                aliases = document.get("vendor", {}).get(CMUCK_VENDOR_KEY, {}).get("modeAliases", {})
                if not isinstance(aliases, dict):
                    raise CmuckError(f"{CMUCK_VENDOR_KEY} modeAliases must map strings to preset names")
                alias_map = cast(dict[object, object], aliases)
                if not all(
                    isinstance(alias, str) and isinstance(target, str)
                    for alias, target in alias_map.items()
                ):
                    raise CmuckError(f"{CMUCK_VENDOR_KEY} modeAliases must map strings to preset names")
                mode_aliases.update(cast(dict[str, str], alias_map))
            includes = document.get("include", [])
            if isinstance(includes, str):
                includes = [includes]
            for include in includes:
                include_path = Path(_expand_macros(str(include), root, ""))
                if not include_path.is_absolute():
                    include_path = path.parent / include_path
                load_file(include_path)
            for kind in raw:
                for preset in document.get(kind, []):
                    name = preset.get("name")
                    if not name:
                        raise CmuckError(f"unnamed preset in {path}")
                    if name in raw[kind]:
                        raise CmuckError(f"duplicate {kind} name {name!r}")
                    raw[kind][name] = preset

        load_file(root / "CMakePresets.json")
        load_file(root / "CMakeUserPresets.json")

        def resolve_all(kind: str) -> dict[str, dict[str, Any]]:
            resolved: dict[str, dict[str, Any]] = {}
            resolving: set[str] = set()

            def resolve(name: str) -> dict[str, Any]:
                if name in resolved:
                    return resolved[name]
                if name in resolving:
                    raise CmuckError(f"cyclic preset inheritance involving {name!r}")
                try:
                    current = raw[kind][name]
                except KeyError as error:
                    raise CmuckError(f"preset {name!r} inherits an unknown {kind} preset") from error
                resolving.add(name)
                parents = current.get("inherits", [])
                if isinstance(parents, str):
                    parents = [parents]
                merged: dict[str, Any] = {}
                # CMake gives earlier parents precedence over later parents.
                for parent in reversed(parents):
                    inherited = {
                        key: value
                        for key, value in resolve(parent).items()
                        if key not in {"name", "hidden", "inherits", "description", "displayName"}
                    }
                    merged = _deep_merge(merged, inherited)
                merged = _deep_merge(merged, current)
                resolving.remove(name)
                resolved[name] = merged
                return merged

            for preset_name in raw[kind]:
                resolve(preset_name)
            return resolved

        return cls(
            root,
            resolve_all("configurePresets"),
            resolve_all("buildPresets"),
            resolve_all("testPresets"),
            mode_aliases,
        )

    def configure_names(self) -> list[str]:
        return sorted(name for name, preset in self.configure.items() if not preset.get("hidden", False))

    def select(self, requested: str | None) -> PresetSelection:
        names = self.configure_names()
        if not names:
            raise CmuckError("CMakePresets.json contains no visible configure presets")
        name = requested or os.environ.get("CMUCK_PRESET")
        if name is None:
            name = "x64-debug" if "x64-debug" in names else names[0]
        if name not in self.configure or self.configure[name].get("hidden", False):
            suggestions = get_close_matches(name, names, n=3)
            suffix = f"; did you mean {', '.join(suggestions)}?" if suggestions else f"; available: {', '.join(names)}"
            raise CmuckError(f"unknown configure preset {name!r}{suffix}")
        preset = self.configure[name]
        binary_dir = preset.get("binaryDir")
        if not binary_dir:
            raise CmuckError(f"configure preset {name!r} has no binaryDir")
        expanded = _expand_macros(str(binary_dir), self.root, name)
        build_dir = Path(expanded)
        if not build_dir.is_absolute():
            build_dir = self.root / build_dir

        def associated(presets: dict[str, dict[str, Any]]) -> str | None:
            matches = [
                preset_name
                for preset_name, candidate in presets.items()
                if not candidate.get("hidden", False) and candidate.get("configurePreset") == name
            ]
            return sorted(matches)[0] if matches else None

        selection = PresetSelection(
            name,
            preset,
            associated(self.build),
            associated(self.test),
            build_dir.resolve(),
        )
        logger.debug("Configure preset: %s", selection.configure_name)
        logger.debug("Build directory: %s", selection.build_dir)
        return selection


_build_environment: dict[str, str] | None = None


def get_build_environment() -> dict[str, str]:
    global _build_environment
    if _build_environment is not None:
        logger.debug("Reusing the cached build environment")
        return _build_environment
    environment = dict(os.environ)
    if platform.system() != "Windows" or environment.get("VSCMD_ARG_HOST_ARCH"):
        logger.debug("Using the current process build environment")
        _build_environment = environment
        return environment
    vswhere = shutil.which("vswhere")
    if not vswhere:
        program_files = environment.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        candidate = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        vswhere = str(candidate) if candidate.is_file() else None
    if not vswhere:
        logger.debug("vswhere was not found; using the current process environment")
        _build_environment = environment
        return environment
    logger.debug("Visual Studio discovery: %s", vswhere)
    result = subprocess.run(
        [vswhere, "-latest", "-property", "installationPath"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        logger.debug("Visual Studio discovery returned no installation")
        _build_environment = environment
        return environment
    vcvars = Path(result.stdout.strip()) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not vcvars.is_file():
        logger.debug("Visual Studio environment script was not found: %s", vcvars)
        _build_environment = environment
        return environment
    logger.info("Activating the MSVC x64 environment")
    command = f'call "{vcvars}" x64 >nul && set'
    invocation = f"cmd.exe /d /s /c {command}"
    logger.debug("Command: %s", invocation)
    result = subprocess.run(
        invocation,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        suffix = f": {detail}" if detail else ""
        raise CmuckError(
            f"Visual Studio environment setup failed with exit code {result.returncode}{suffix}"
        )
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if separator:
            environment[key] = value
    _build_environment = environment
    return environment


def write_file_api_query(build_dir: Path) -> None:
    query_dir = build_dir / ".cmake" / "api" / "v1" / "query" / "client-cmuck"
    query_dir.mkdir(parents=True, exist_ok=True)
    query = {"requests": [{"kind": "codemodel", "version": 2}]}
    (query_dir / "query.json").write_text(json.dumps(query, indent=2) + "\n", encoding="utf-8")


@dataclass(frozen=True)
class Target:
    name: str
    type: str
    source_dir: Path
    build_dir: Path
    artifacts: tuple[Path, ...]
    folder: str
    name_on_disk: str | None

    @property
    def category(self) -> str:
        if self.folder.startswith("Crisp/Tests"):
            return "test"
        if self.folder.startswith("Crisp/Benchmarks"):
            return "benchmark"
        if self.type == "EXECUTABLE":
            return "application"
        if self.type.endswith("_LIBRARY"):
            return "library"
        return "other"

    @property
    def executable(self) -> Path | None:
        if self.type != "EXECUTABLE" or not self.artifacts:
            return None
        if self.name_on_disk:
            for artifact in self.artifacts:
                if artifact.name.casefold() == self.name_on_disk.casefold():
                    return artifact
        ignored = {".pdb", ".lib", ".exp", ".ilk", ".dSYM"}
        return next((artifact for artifact in self.artifacts if artifact.suffix.lower() not in ignored), self.artifacts[0])


class TransientSnapshotError(RuntimeError):
    pass


def _newest_index(reply_dir: Path) -> tuple[Path, tuple[str, int, int]]:
    indexes = list(reply_dir.glob("index-*.json"))
    if not indexes:
        raise CmuckError(f"no CMake File API index in {reply_dir}; run 'cmuck configure'")
    index = max(indexes, key=lambda path: (path.stat().st_mtime_ns, path.name))
    stat = index.stat()
    logger.debug("CMake File API snapshot: %s", index)
    return index, (index.name, stat.st_mtime_ns, stat.st_size)


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise TransientSnapshotError(str(path)) from error
    except json.JSONDecodeError as error:
        raise TransientSnapshotError(f"invalid transient JSON in {path}: {error}") from error


def _read_file_api_once(root: Path, build_dir: Path) -> tuple[list[Target], tuple[str, int, int]]:
    reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply"
    index_path, marker = _newest_index(reply_dir)
    index = _read_json(index_path)
    codemodel_ref = next((item for item in index.get("objects", []) if item.get("kind") == "codemodel"), None)
    if not codemodel_ref:
        raise CmuckError("the newest CMake File API snapshot has no codemodel; run 'cmuck configure'")
    codemodel = _read_json(reply_dir / codemodel_ref["jsonFile"])
    configurations = codemodel.get("configurations", [])
    if not configurations:
        raise CmuckError("the CMake File API codemodel contains no configurations")
    configuration = configurations[0]
    projects = configuration.get("projects", [])
    root_projects = [project for project in projects if "parentIndex" not in project]
    project = next((item for item in root_projects if item.get("name") == root.name), None)
    project = project or (root_projects[0] if root_projects else None)
    if not project:
        raise CmuckError("the CMake File API codemodel contains no root project")
    project_indexes = set(project.get("targetIndexes", []))
    source_base = Path(codemodel["paths"]["source"])
    build_base = Path(codemodel["paths"]["build"])
    targets: list[Target] = []
    for target_index, target_ref in enumerate(configuration.get("targets", [])):
        if target_index not in project_indexes:
            continue
        data = _read_json(reply_dir / target_ref["jsonFile"])
        source_path = Path(data.get("paths", {}).get("source", "."))
        target_build_path = Path(data.get("paths", {}).get("build", "."))
        targets.append(
            Target(
                name=data["name"],
                type=data["type"],
                source_dir=(source_base / source_path).resolve(),
                build_dir=(build_base / target_build_path).resolve(),
                artifacts=tuple((build_base / artifact["path"]).resolve() for artifact in data.get("artifacts", [])),
                folder=data.get("folder", {}).get("name", ""),
                name_on_disk=data.get("nameOnDisk"),
            )
        )
    _, final_marker = _newest_index(reply_dir)
    if marker != final_marker:
        raise TransientSnapshotError("the File API index changed during the read")
    return targets, marker


def read_file_api(root: Path, build_dir: Path, retries: int = 3) -> list[Target]:
    last_error: Exception | None = None
    for attempt in range(retries):
        try:
            targets, _ = _read_file_api_once(root, build_dir)
            return targets
        except TransientSnapshotError as error:
            last_error = error
            if attempt + 1 < retries:
                time.sleep(0.05)
    detail = f": {last_error}" if last_error else ""
    raise CmuckError(
        "CMake File API replies changed or disappeared while being read"
        f"{detail}. Stop concurrent configure/build operations and run 'cmuck configure'."
    )


@dataclass(frozen=True)
class CTestCase:
    name: str
    command: tuple[str, ...]
    working_directory: Path | None
    labels: tuple[str, ...]


def parse_ctest_document(document: dict[str, Any]) -> list[CTestCase]:
    cases: list[CTestCase] = []
    for test in document.get("tests", []):
        properties = {item.get("name"): item.get("value") for item in test.get("properties", [])}
        labels = properties.get("LABELS", [])
        if isinstance(labels, str):
            labels = [labels]
        working_directory = properties.get("WORKING_DIRECTORY")
        cases.append(
            CTestCase(
                name=test["name"],
                command=tuple(str(item) for item in test.get("command", [])),
                working_directory=Path(working_directory) if working_directory else None,
                labels=tuple(str(item) for item in labels),
            )
        )
    return cases


def read_ctest(build_dir: Path) -> list[CTestCase]:
    command = ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"]
    logger.debug("Command: %s", format_command(command))
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise CmuckError(f"CTest metadata discovery failed: {detail}")
    try:
        return parse_ctest_document(json.loads(result.stdout))
    except json.JSONDecodeError as error:
        raise CmuckError(f"CTest returned invalid JSON: {error}") from error


def _normalized_path(path: str | Path) -> str:
    return os.path.normcase(os.path.normpath(str(Path(path).resolve())))


def target_for_case(case: CTestCase, targets: Sequence[Target]) -> Target | None:
    artifacts = {
        _normalized_path(target.executable): target
        for target in targets
        if target.executable is not None
    }
    for argument in case.command:
        target = artifacts.get(_normalized_path(argument))
        if target:
            return target
    return None


def resolve_target(query: str, targets: Sequence[Target], description: str = "target") -> Target:
    exact = [target for target in targets if target.name == query]
    if exact:
        return exact[0]
    folded = [target for target in targets if target.name.casefold() == query.casefold()]
    if len(folded) == 1:
        return folded[0]
    partial = [target for target in targets if query.casefold() in target.name.casefold()]
    if len(partial) == 1:
        return partial[0]
    if len(partial) > 1:
        names = ", ".join(target.name for target in partial[:12])
        raise CmuckError(f"ambiguous {description} {query!r}; matches: {names}")
    names = [target.name for target in targets]
    suggestions = get_close_matches(query, names, n=5, cutoff=0.35)
    suffix = f"; suggestions: {', '.join(suggestions)}" if suggestions else ""
    raise CmuckError(f"unknown {description} {query!r}{suffix}")


def exact_ctest_regex(names: Iterable[str]) -> str:
    escaped = [re.sub(r"([][.^$*+?(){}|\\])", r"\\\1", name) for name in names]
    if not escaped:
        raise CmuckError("cannot construct a CTest filter for an empty test set")
    return f"^({('|'.join(escaped))})$" if len(escaped) > 1 else f"^{escaped[0]}$"


def require_configured(selection: PresetSelection) -> None:
    if not (selection.build_dir / "CMakeCache.txt").is_file():
        raise CmuckError(
            f"build tree {selection.build_dir} is not configured; "
            f"run 'cmuck configure --preset {selection.configure_name}'"
        )


def build_targets(root: Path, selection: PresetSelection, targets: Sequence[str], extra: Sequence[str] = ()) -> int:
    if not targets:
        raise CmuckError("no build targets were selected")
    if selection.build_name:
        command = ["cmake", "--build", "--preset", selection.build_name]
    else:
        command = ["cmake", "--build", str(selection.build_dir)]
    command.extend(["--target", *targets])
    command.extend(extra)
    logger.info("Building %s", ", ".join(targets))
    logger.debug("Command: %s", format_command(command))
    return subprocess.run(command, cwd=root, env=get_build_environment(), check=False).returncode


def run_program(path: Path, arguments: Sequence[str], cwd: Path) -> int:
    if not path.is_file():
        raise CmuckError(f"built artifact does not exist: {path}")
    command = [str(path), *arguments]
    logger.info("Running %s", path.name)
    logger.debug("Command: %s", format_command(command))
    return subprocess.run(command, cwd=cwd, check=False).returncode
