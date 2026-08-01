#!/usr/bin/env python3
"""A thin Click interface over CMake presets, the File API, and CTest."""

from __future__ import annotations

import logging
import subprocess
import sys
from functools import wraps
from pathlib import Path
from time import perf_counter
from typing import Any, Callable, Sequence, TypeVar

import click

from cmucklog import configure_logging, format_command
from cmucklib import (
    CTestCase,
    CmuckError,
    PresetCatalog,
    PresetSelection,
    Target,
    build_targets,
    exact_ctest_regex,
    find_project_root,
    get_build_environment,
    read_ctest,
    read_file_api,
    require_configured,
    resolve_target,
    run_program,
    target_for_case,
    write_file_api_query,
)


logger = logging.getLogger("cmuck")


class PassthroughCommand(click.Command):
    """Capture every token after ``--`` before Click performs parsing."""

    def parse_args(self, ctx: click.Context, args: list[str]) -> list[str]:
        if "--" in args:
            separator = args.index("--")
            ctx.meta["passthrough"] = tuple(args[separator + 1 :])
            args = args[:separator]
        else:
            ctx.meta["passthrough"] = ()
        return super().parse_args(ctx, args)


class ModeAliasGroup(click.Group):
    """Translate legacy @mode aliases using mappings owned by CMakePresets.json."""

    def parse_args(self, ctx: click.Context, args: list[str]) -> list[str]:
        separator = args.index("--") if "--" in args else len(args)
        command_args = args[:separator]
        passthrough = args[separator:]
        aliases = [argument for argument in command_args if argument.startswith("@mode/")]
        if not aliases:
            return super().parse_args(ctx, args)
        if len(aliases) > 1:
            raise click.UsageError("specify at most one @mode alias", ctx)
        if any(argument == "--preset" or argument.startswith("--preset=") for argument in command_args):
            raise click.UsageError("do not combine @mode/... with --preset", ctx)
        try:
            catalog = PresetCatalog.load(find_project_root())
        except CmuckError as error:
            raise click.UsageError(str(error), ctx) from error
        token = aliases[0]
        mode = token.removeprefix("@mode/")
        preset = catalog.mode_aliases.get(mode)
        if preset is None:
            available = ", ".join(f"@mode/{name}" for name in sorted(catalog.mode_aliases))
            raise click.UsageError(f"unknown mode alias {token!r}; available: {available}", ctx)
        command_args.remove(token)
        command_index = next(
            (index for index, argument in enumerate(command_args) if argument in self.commands),
            None,
        )
        if command_index is None:
            raise click.UsageError("@mode aliases must accompany a cmuck command", ctx)
        command_args[command_index + 1 : command_index + 1] = ["--preset", preset]
        return super().parse_args(ctx, [*command_args, *passthrough])


def passthrough_args(ctx: click.Context) -> tuple[str, ...]:
    return tuple(ctx.meta.get("passthrough", ()))


F = TypeVar("F", bound=Callable[..., Any])


def user_errors(function: F) -> F:
    """Render core errors using Click's normal concise diagnostic style."""

    @wraps(function)
    def wrapped(*args: Any, **kwargs: Any) -> Any:
        try:
            return function(*args, **kwargs)
        except CmuckError as error:
            raise click.ClickException(str(error)) from error

    return wrapped  # type: ignore[return-value]


def _context(preset: str | None) -> tuple[Path, PresetSelection]:
    root = find_project_root()
    selection = PresetCatalog.load(root).select(preset)
    return root, selection


def _metadata(preset: str | None) -> tuple[Path, PresetSelection, list[Target]]:
    root, selection = _context(preset)
    require_configured(selection)
    return root, selection, read_file_api(root, selection.build_dir)


def _exit_for(ctx: click.Context, returncode: int) -> None:
    if returncode:
        ctx.exit(returncode)


def _unique_targets(targets: Sequence[Target]) -> list[Target]:
    result: list[Target] = []
    seen: set[str] = set()
    for target in targets:
        if target.name not in seen:
            result.append(target)
            seen.add(target.name)
    return result


def _owner_cases(cases: Sequence[CTestCase], targets: Sequence[Target], owner: Target) -> list[CTestCase]:
    return [case for case in cases if target_for_case(case, targets) == owner]


def _select_case(selector: str, cases: Sequence[CTestCase], targets: Sequence[Target]) -> list[CTestCase]:
    exact = [case for case in cases if case.name == selector]
    if exact:
        return exact
    test_targets = [target for target in targets if target.category == "test"]
    matching_targets = [target for target in test_targets if target.name.casefold() == selector.casefold()]
    if matching_targets:
        return _owner_cases(cases, targets, matching_targets[0])
    partial = [case for case in cases if selector.casefold() in case.name.casefold()]
    if len(partial) == 1:
        return partial
    if len(partial) > 1:
        names = ", ".join(case.name for case in partial[:12])
        raise CmuckError(f"ambiguous test {selector!r}; matches: {names}")
    # Reuse target diagnostics when the selector resembles a target.
    resolve_target(selector, test_targets, "test or test target")
    raise CmuckError(f"no CTest cases belong to test target {selector!r}")


@click.group(
    cls=ModeAliasGroup,
    context_settings={"help_option_names": ["-h", "--help"]},
    epilog="Legacy aliases: @mode/dev selects x64-debug; @mode/opt selects x64-release.",
)
@click.option("-v", "--verbose", is_flag=True, help="Show resolved metadata and subprocess commands.")
@click.option("-q", "--quiet", is_flag=True, help="Show only warnings and errors.")
def cli(verbose: bool, quiet: bool) -> None:
    """Build, discover, run, and test Crisp targets through CMake metadata."""

    if verbose and quiet:
        raise click.UsageError("--verbose and --quiet cannot be used together")
    configure_logging(verbose=verbose, quiet=quiet)


@cli.command(cls=PassthroughCommand)
@click.option("--preset", metavar="NAME", help="Configure preset (default: x64-debug when available).")
@click.pass_context
@user_errors
def configure(ctx: click.Context, preset: str | None) -> None:
    """Configure a preset and request a cmuck File API reply.

    Arguments after ``--`` are forwarded to CMake unchanged.
    """

    root, selection = _context(preset)
    write_file_api_query(selection.build_dir)
    command = ["cmake", "--preset", selection.configure_name, *passthrough_args(ctx)]
    logger.info("Configuring %s", selection.configure_name)
    logger.debug("Command: %s", format_command(command))
    returncode = subprocess.run(command, cwd=root, env=get_build_environment(), check=False).returncode
    _exit_for(ctx, returncode)


@cli.command(cls=PassthroughCommand)
@click.argument("targets", nargs=-1, required=True)
@click.option("--preset", metavar="NAME", help="Configure preset to build.")
@click.pass_context
@user_errors
def build(ctx: click.Context, targets: tuple[str, ...], preset: str | None) -> None:
    """Build one or more explicit project targets.

    Arguments after ``--`` are forwarded to ``cmake --build`` unchanged.
    """

    root, selection, available = _metadata(preset)
    selected = _unique_targets([resolve_target(name, available) for name in targets])
    _exit_for(ctx, build_targets(root, selection, [target.name for target in selected], passthrough_args(ctx)))


@cli.command(cls=PassthroughCommand)
@click.argument("target")
@click.option("--preset", metavar="NAME", help="Configure preset to build and run.")
@click.pass_context
@user_errors
def run(ctx: click.Context, target: str, preset: str | None) -> None:
    """Build and run one application target.

    Arguments after ``--`` are passed to the executable unchanged.
    """

    root, selection, available = _metadata(preset)
    applications = [item for item in available if item.category == "application"]
    selected = resolve_target(target, applications, "application target")
    _exit_for(ctx, build_targets(root, selection, [selected.name]))
    if selected.executable is None:
        raise CmuckError(f"target {selected.name!r} has no executable artifact")
    _exit_for(ctx, run_program(selected.executable, passthrough_args(ctx), root))


@cli.command(cls=PassthroughCommand)
@click.argument("target")
@click.option("--preset", metavar="NAME", help="Configure preset to build and run.")
@click.pass_context
@user_errors
def bench(ctx: click.Context, target: str, preset: str | None) -> None:
    """Build and run one benchmark target.

    Arguments after ``--`` are passed to the benchmark unchanged.
    """

    root, selection, available = _metadata(preset)
    benchmarks = [item for item in available if item.category == "benchmark"]
    selected = resolve_target(target, benchmarks, "benchmark target")
    _exit_for(ctx, build_targets(root, selection, [selected.name]))
    if selected.executable is None:
        raise CmuckError(f"target {selected.name!r} has no executable artifact")
    _exit_for(ctx, run_program(selected.executable, passthrough_args(ctx), root))


@cli.command("targets")
@click.option("--preset", metavar="NAME", help="Configured preset whose metadata should be read.")
@click.option(
    "target_type",
    "--type",
    type=click.Choice(["all", "executable", "application", "library", "test", "benchmark"]),
    default="all",
    show_default=True,
)
@user_errors
def list_targets(preset: str | None, target_type: str) -> None:
    """List Crisp-owned targets without configuring or building."""

    root, _, available = _metadata(preset)

    def included(target: Target) -> bool:
        if target_type == "all":
            return target.category != "other"
        if target_type == "executable":
            return target.type == "EXECUTABLE"
        return target.category == target_type

    selected = sorted((target for target in available if included(target)), key=lambda target: target.name.casefold())
    for target in selected:
        try:
            source = target.source_dir.relative_to(root)
        except ValueError:
            source = target.source_dir
        click.echo(f"{target.name}\t{target.category}\t{source}")
    click.echo(f"\n{len(selected)} targets")


@cli.command("tests")
@click.argument("query", required=False)
@click.option("--label", multiple=True, help="Require a CTest label (repeatable).")
@click.option("--preset", metavar="NAME", help="Configured preset whose metadata should be read.")
@user_errors
def list_tests(query: str | None, label: tuple[str, ...], preset: str | None) -> None:
    """List CTest cases without configuring or building."""

    _, selection, available = _metadata(preset)
    cases = read_ctest(selection.build_dir)
    if query:
        cases = [case for case in cases if query.casefold() in case.name.casefold()]
    if label:
        cases = [case for case in cases if all(item in case.labels for item in label)]
    for case in cases:
        owner = target_for_case(case, available)
        owner_name = owner.name if owner else "?"
        labels = ",".join(case.labels)
        click.echo(f"{case.name}\t{owner_name}\t{labels}")
    click.echo(f"\n{len(cases)} tests")


@cli.command(cls=PassthroughCommand)
@click.argument("selectors", nargs=-1)
@click.option("--label", multiple=True, help="Require a CTest label (repeatable).")
@click.option("--preset", metavar="NAME", help="Configure preset to build and test.")
@click.pass_context
@user_errors
def test(ctx: click.Context, selectors: tuple[str, ...], label: tuple[str, ...], preset: str | None) -> None:
    """Build owning test targets and run exact CTest cases.

    A selector may be a complete CTest name or a test target. With no selector,
    every structurally identified Crisp test target is built. Arguments after
    ``--`` are forwarded to CTest unchanged.
    """

    root, selection, available = _metadata(preset)
    test_targets = [target for target in available if target.category == "test"]
    if not test_targets:
        raise CmuckError("the File API contains no Crisp test targets")

    # Build enough executables to refresh gtest_discover_tests metadata before
    # asking CTest for its case inventory. Labels require the complete inventory.
    initial: list[Target] = []
    if not selectors or label:
        initial = test_targets
    else:
        for selector in selectors:
            initial.extend(
                target
                for target in test_targets
                if selector.casefold() == target.name.casefold()
                or selector.casefold().startswith(target.name.casefold() + ".")
            )
    initial = _unique_targets(initial)
    if initial:
        _exit_for(ctx, build_targets(root, selection, [target.name for target in initial]))

    cases = read_ctest(selection.build_dir)
    selected_cases: list[CTestCase] = []
    if selectors:
        for selector in selectors:
            selected_cases.extend(_select_case(selector, cases, available))
    else:
        selected_cases = list(cases)
    if label:
        selected_cases = [case for case in selected_cases if all(item in case.labels for item in label)]
    selected_cases = list(dict.fromkeys(selected_cases))
    if not selected_cases:
        qualifier = f" with labels {', '.join(label)}" if label else ""
        raise CmuckError(f"no tests selected{qualifier}")

    owners = _unique_targets(
        [owner for case in selected_cases if (owner := target_for_case(case, available)) is not None]
    )
    missing_owners = [owner for owner in owners if owner not in initial]
    if missing_owners:
        _exit_for(ctx, build_targets(root, selection, [target.name for target in missing_owners]))

    if selection.test_name:
        command = ["ctest", "--preset", selection.test_name]
        cwd = root
    else:
        command = ["ctest", "--test-dir", str(selection.build_dir), "--output-on-failure"]
        cwd = root
    command.extend(["--tests-regex", exact_ctest_regex(case.name for case in selected_cases)])
    command.extend(passthrough_args(ctx))
    logger.info("Running %d CTest case(s)", len(selected_cases))
    logger.debug("Command: %s", format_command(command))
    _exit_for(ctx, subprocess.run(command, cwd=cwd, check=False).returncode)


def main() -> int:
    start_time = perf_counter()
    configure_logging()
    exit_code = 0
    try:
        cli(standalone_mode=False)
    except click.UsageError as error:
        error.show()
        exit_code = error.exit_code
    except click.ClickException as error:
        logger.error("%s", error.format_message())
        exit_code = error.exit_code
    except click.exceptions.Exit as error:
        exit_code = error.exit_code
    except KeyboardInterrupt:
        logger.warning("Interrupted")
        exit_code = 130
    except Exception as error:
        if logger.isEnabledFor(logging.DEBUG):
            logger.exception("Unexpected failure")
        else:
            logger.error("Unexpected failure: %s", error)
        exit_code = 1
    finally:
        logger.info("Total execution time: %.2f s", perf_counter() - start_time)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
