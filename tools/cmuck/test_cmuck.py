from __future__ import annotations

import json
import logging
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from click.testing import CliRunner

import cmuck
import cmucklib
from cmucklib import (
    CTestCase,
    CmuckError,
    PresetCatalog,
    PresetSelection,
    Target,
    TransientSnapshotError,
    exact_ctest_regex,
    parse_ctest_document,
    read_file_api,
    resolve_target,
    target_for_case,
)


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")


def temporary_directory(*, prefix: str = "tmp") -> tempfile.TemporaryDirectory[str]:
    return tempfile.TemporaryDirectory(
        prefix=prefix,
        dir=os.environ.get("CMUCK_TEST_TMPDIR"),
    )


def preset_document() -> dict[str, object]:
    return {
        "version": 8,
        "vendor": {
            cmucklib.CMUCK_VENDOR_KEY: {
                "modeAliases": {"dev": "x64-debug", "opt": "x64-release"}
            }
        },
        "configurePresets": [
            {
                "name": "base",
                "hidden": True,
                "generator": "Ninja",
                "binaryDir": "${sourceDir}/out with spaces/${presetName}",
                "cacheVariables": {"BASE": "yes"},
            },
            {
                "name": "x64-debug",
                "displayName": "Debug",
                "inherits": "base",
                "cacheVariables": {"CMAKE_BUILD_TYPE": "Debug"},
            },
            {
                "name": "x64-release",
                "inherits": "base",
                "cacheVariables": {"CMAKE_BUILD_TYPE": "Release"},
            },
        ],
        "buildPresets": [
            {"name": "build-base", "hidden": True, "jobs": 8},
            {
                "name": "x64-debug-build",
                "inherits": "build-base",
                "configurePreset": "x64-debug",
            },
        ],
        "testPresets": [
            {"name": "test-base", "hidden": True, "execution": {"jobs": 2}},
            {
                "name": "test-debug",
                "inherits": "test-base",
                "configurePreset": "x64-debug",
            },
        ],
    }


class PresetCatalogTest(unittest.TestCase):
    def test_inheritance_aliases_and_space_paths(self) -> None:
        with temporary_directory(prefix="cmuck project with spaces ") as directory:
            root = Path(directory)
            write_json(root / "CMakePresets.json", preset_document())
            catalog = PresetCatalog.load(root)
            self.assertEqual(catalog.configure_names(), ["x64-debug", "x64-release"])
            self.assertEqual(catalog.mode_aliases, {"dev": "x64-debug", "opt": "x64-release"})
            selection = catalog.select("x64-debug")
            self.assertEqual(selection.build_name, "x64-debug-build")
            self.assertEqual(selection.test_name, "test-debug")
            self.assertEqual(selection.configuration, "Debug")
            self.assertEqual(selection.build_dir, root / "out with spaces" / "x64-debug")
            self.assertNotIn("hidden", selection.configure)
            self.assertEqual(selection.configure["cacheVariables"]["BASE"], "yes")

    def test_unknown_preset_has_suggestion(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            write_json(root / "CMakePresets.json", preset_document())
            with self.assertRaisesRegex(CmuckError, "did you mean x64-debug"):
                PresetCatalog.load(root).select("x65-debug")


class BuildEnvironmentTest(unittest.TestCase):
    def test_msvc_activation_handles_a_path_with_spaces(self) -> None:
        commands: list[str | list[str]] = []

        def fake_run(command: str | list[str], **_: object) -> subprocess.CompletedProcess[str]:
            commands.append(command)
            if isinstance(command, list):
                return subprocess.CompletedProcess(
                    command,
                    0,
                    stdout="D:\\Programs\\Microsoft Visual Studio\\18\\Community\n",
                    stderr="",
                )
            return subprocess.CompletedProcess(
                command,
                0,
                stdout="VSCMD_ARG_HOST_ARCH=x64\nPATH=D:\\Tools\n",
                stderr="",
            )

        with (
            patch.object(cmucklib, "_build_environment", None),
            patch.object(cmucklib.platform, "system", return_value="Windows"),
            patch.object(
                cmucklib.shutil,
                "which",
                return_value=r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe",
            ),
            patch.object(cmucklib.Path, "is_file", return_value=True),
            patch.object(cmucklib.subprocess, "run", side_effect=fake_run),
            patch.dict(cmucklib.os.environ, {}, clear=True),
        ):
            environment = cmucklib.get_build_environment()

        self.assertEqual(environment["VSCMD_ARG_HOST_ARCH"], "x64")
        self.assertEqual(
            commands[1],
            'cmd.exe /d /s /c call "D:\\Programs\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul && set',
        )

    def test_msvc_activation_failure_includes_batch_error(self) -> None:
        def fake_run(command: str | list[str], **_: object) -> subprocess.CompletedProcess[str]:
            if isinstance(command, list):
                return subprocess.CompletedProcess(
                    command,
                    0,
                    stdout="D:\\Programs\\Microsoft Visual Studio\\18\\Community\n",
                    stderr="",
                )
            return subprocess.CompletedProcess(command, 1, stdout="", stderr="setup failed clearly")

        with (
            patch.object(cmucklib, "_build_environment", None),
            patch.object(cmucklib.platform, "system", return_value="Windows"),
            patch.object(cmucklib.shutil, "which", return_value=r"C:\Tools\vswhere.exe"),
            patch.object(cmucklib.Path, "is_file", return_value=True),
            patch.object(cmucklib.subprocess, "run", side_effect=fake_run),
            patch.dict(cmucklib.os.environ, {}, clear=True),
            self.assertRaisesRegex(CmuckError, "setup failed clearly"),
        ):
            cmucklib.get_build_environment()


class FileApiTest(unittest.TestCase):
    def create_reply(self, root: Path) -> Path:
        build = root / "build tree with spaces"
        reply = build / ".cmake" / "api" / "v1" / "reply"
        write_json(
            reply / "index-001.json",
            {
                "objects": [
                    {"kind": "codemodel", "version": {"major": 2, "minor": 7}, "jsonFile": "model.json"}
                ]
            },
        )
        write_json(
            reply / "model.json",
            {
                "paths": {"source": str(root), "build": str(build)},
                "configurations": [
                    {
                        "projects": [
                            {"name": root.name, "targetIndexes": [0]},
                            {"name": "dependency", "parentIndex": 0, "targetIndexes": [1]},
                        ],
                        "targets": [
                            {"name": "Crisp App", "jsonFile": "app.json"},
                            {"name": "third_party", "jsonFile": "dependency.json"},
                        ],
                    }
                ],
            },
        )
        write_json(
            reply / "app.json",
            {
                "name": "Crisp App",
                "type": "EXECUTABLE",
                "nameOnDisk": "Crisp App.exe",
                "folder": {"name": "Crisp/Applications"},
                "paths": {"source": "Crisp", "build": "Crisp"},
                "artifacts": [{"path": "Crisp/Crisp App.exe"}, {"path": "Crisp/Crisp App.pdb"}],
            },
        )
        write_json(
            reply / "dependency.json",
            {
                "name": "third_party",
                "type": "STATIC_LIBRARY",
                "paths": {"source": "dependency", "build": "dependency"},
            },
        )
        return build

    def test_reads_only_root_project_targets_and_artifacts_with_spaces(self) -> None:
        with temporary_directory(prefix="cmuck file api ") as directory:
            root = Path(directory)
            build = self.create_reply(root)
            targets = read_file_api(root, build)
            self.assertEqual([target.name for target in targets], ["Crisp App"])
            self.assertEqual(targets[0].category, "application")
            self.assertEqual(targets[0].executable, build / "Crisp" / "Crisp App.exe")

    def test_retries_transient_snapshot_failure(self) -> None:
        target = Target("app", "EXECUTABLE", Path("."), Path("."), (), "", None)
        with patch.object(
            cmucklib,
            "_read_file_api_once",
            side_effect=[TransientSnapshotError("changed"), ([target], ("index", 1, 1))],
        ) as reader:
            self.assertEqual(read_file_api(Path("."), Path(".")), [target])
            self.assertEqual(reader.call_count, 2)

    def test_reports_targeted_recovery_after_repeated_failure(self) -> None:
        with patch.object(
            cmucklib,
            "_read_file_api_once",
            side_effect=TransientSnapshotError("missing reply"),
        ):
            with self.assertRaisesRegex(CmuckError, "Stop concurrent configure/build operations"):
                read_file_api(Path("."), Path("."), retries=2)


class CTestTest(unittest.TestCase):
    def test_parses_command_working_directory_and_labels(self) -> None:
        cases = parse_ctest_document(
            {
                "tests": [
                    {
                        "name": "Suite.Case",
                        "command": ["C:/build path/Test.exe", "--gtest_filter=Suite.Case"],
                        "properties": [
                            {"name": "WORKING_DIRECTORY", "value": "C:/working path"},
                            {"name": "LABELS", "value": ["unit", "math"]},
                        ],
                    }
                ]
            }
        )
        self.assertEqual(cases[0].command[0], "C:/build path/Test.exe")
        self.assertEqual(cases[0].working_directory, Path("C:/working path"))
        self.assertEqual(cases[0].labels, ("unit", "math"))

    def test_maps_case_to_owning_artifact(self) -> None:
        artifact = (Path.cwd() / "build path" / "Test.exe").resolve()
        target = Target("Test", "EXECUTABLE", Path.cwd(), Path.cwd(), (artifact,), "Crisp/Tests", "Test.exe")
        case = CTestCase("Suite.Case", (str(artifact), "--flag"), None, ())
        self.assertEqual(target_for_case(case, [target]), target)

    def test_exact_regex_is_anchored_and_escaped(self) -> None:
        self.assertEqual(exact_ctest_regex(["Target.Suite.Case[0]"]), r"^Target\.Suite\.Case\[0\]$")


class DiagnosticsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.targets = [
            Target("CrispNoise", "STATIC_LIBRARY", Path("."), Path("."), (), "Crisp/Libraries", None),
            Target("CrispNoiseTest", "EXECUTABLE", Path("."), Path("."), (), "Crisp/Tests", None),
        ]

    def test_unique_partial_target(self) -> None:
        self.assertEqual(resolve_target("NoiseTest", self.targets).name, "CrispNoiseTest")

    def test_ambiguous_target_lists_matches(self) -> None:
        with self.assertRaisesRegex(CmuckError, "ambiguous target.*CrispNoise"):
            resolve_target("Noise", self.targets)


class ClickInterfaceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.runner = CliRunner()
        self.temporary = temporary_directory(prefix="cmuck click ")
        self.root = Path(self.temporary.name)
        write_json(self.root / "CMakePresets.json", preset_document())
        self.previous_directory = Path.cwd()
        os.chdir(self.root)

    def tearDown(self) -> None:
        os.chdir(self.previous_directory)
        self.temporary.cleanup()
        logger = logging.getLogger("cmuck")
        logger.handlers.clear()
        logger.setLevel(logging.WARNING)

    def selection(self) -> PresetSelection:
        return PresetCatalog.load(self.root).select("x64-debug")

    def test_mode_aliases_map_through_vendor_configuration(self) -> None:
        for alias, preset in (("dev", "x64-debug"), ("opt", "x64-release")):
            with (
                self.subTest(alias=alias),
                patch.object(
                    cmuck,
                    "_metadata",
                    return_value=(self.root, self.selection(), []),
                ) as metadata,
            ):
                result = self.runner.invoke(
                    cmuck.cli,
                    ["targets", f"@mode/{alias}", "--type", "application"],
                )
                self.assertEqual(result.exit_code, 0, result.output)
                metadata.assert_called_once_with(preset)

    def test_unknown_mode_is_a_click_usage_error(self) -> None:
        result = self.runner.invoke(cmuck.cli, ["targets", "@mode/fast"])
        self.assertEqual(result.exit_code, 2)
        self.assertIn("available: @mode/dev, @mode/opt", result.output)

    def test_passthrough_preserves_tokens_after_separator(self) -> None:
        captured: list[tuple[str, ...]] = []

        @cmuck.click.command(cls=cmuck.PassthroughCommand)
        @cmuck.click.argument("target")
        @cmuck.click.pass_context
        def sample(ctx: cmuck.click.Context, target: str) -> None:
            del target
            captured.append(cmuck.passthrough_args(ctx))

        result = self.runner.invoke(sample, ["app", "--", "--flag=value", "argument with spaces", "@mode/opt"])
        self.assertEqual(result.exit_code, 0, result.output)
        self.assertEqual(captured, [("--flag=value", "argument with spaces", "@mode/opt")])

    def test_build_preserves_child_exit_code(self) -> None:
        target = Target("App", "EXECUTABLE", self.root, self.root, (), "Crisp/Applications", None)
        with (
            patch.object(cmuck, "_metadata", return_value=(self.root, self.selection(), [target])),
            patch.object(cmuck, "build_targets", return_value=7),
        ):
            result = self.runner.invoke(cmuck.cli, ["build", "App"])
        self.assertEqual(result.exit_code, 7)

    def test_metadata_command_does_not_request_build_environment(self) -> None:
        with (
            patch.object(cmuck, "_metadata", return_value=(self.root, self.selection(), [])),
            patch.object(cmuck, "get_build_environment") as environment,
        ):
            result = self.runner.invoke(cmuck.cli, ["targets"])
        self.assertEqual(result.exit_code, 0, result.output)
        environment.assert_not_called()

    def test_default_logging_uses_stderr_and_keeps_stdout_clean(self) -> None:
        completed = subprocess.CompletedProcess(["cmake"], 0, stdout="", stderr="")
        with (
            patch.object(cmuck, "get_build_environment", return_value={}),
            patch.object(cmuck.subprocess, "run", return_value=completed),
        ):
            result = self.runner.invoke(cmuck.cli, ["configure"])
        self.assertEqual(result.exit_code, 0, result.output)
        self.assertEqual(result.stdout, "")
        self.assertIn("INFO", result.stderr)
        self.assertIn("Configuring x64-debug", result.stderr)
        self.assertNotIn("Command:", result.stderr)

    def test_verbose_logging_shows_commands(self) -> None:
        completed = subprocess.CompletedProcess(["cmake"], 0, stdout="", stderr="")
        with (
            patch.object(cmuck, "get_build_environment", return_value={}),
            patch.object(cmuck.subprocess, "run", return_value=completed),
        ):
            result = self.runner.invoke(cmuck.cli, ["--verbose", "configure"])
        self.assertEqual(result.exit_code, 0, result.output)
        self.assertIn("DEBUG", result.stderr)
        self.assertIn("Command: cmake --preset x64-debug", result.stderr)

    def test_quiet_logging_suppresses_status(self) -> None:
        completed = subprocess.CompletedProcess(["cmake"], 0, stdout="", stderr="")
        with (
            patch.object(cmuck, "get_build_environment", return_value={}),
            patch.object(cmuck.subprocess, "run", return_value=completed),
        ):
            result = self.runner.invoke(cmuck.cli, ["--quiet", "configure"])
        self.assertEqual(result.exit_code, 0, result.output)
        self.assertEqual(result.stdout, "")
        self.assertEqual(result.stderr, "")

    def test_verbose_and_quiet_are_mutually_exclusive(self) -> None:
        result = self.runner.invoke(cmuck.cli, ["--verbose", "--quiet", "targets"])
        self.assertEqual(result.exit_code, 2)
        self.assertIn("--verbose and --quiet cannot be used together", result.stderr)


if __name__ == "__main__":
    unittest.main()
