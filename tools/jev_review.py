#!/usr/bin/env python3
"""JEV-assisted semantic review triage for NativeUI.

Usage:
  TYPESAFE_API_KEY=... python3 tools/jev_review.py 165 --ticket-only
  TYPESAFE_API_KEY=... python3 tools/jev_review.py 165
  TYPESAFE_API_KEY=... python3 tools/jev_review.py 165 --pr 433
  python3 tools/jev_review.py 165 --ticket-only --dry-run --request-out /tmp/jev-ticket-request.json

The tool is intentionally a triage layer. Jev probabilities are not review
evidence and never replace CODE_REVIEW.md, tests, sanitizers, platform checks,
or the independent exact-head final review.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

API_ROOT = "https://api.typesafe.ai"
DEFAULT_MODEL = "jev-latest"
POLICIES = ("AGENTS.md", "CODE_REVIEW.md", "AUTOMATION_CODE_REVIEW_GATE.md")
TICKET_CONTEXT = ("AGENTS.md", "CODE_REVIEW.md", "DESIGN.md", "CONTEXT.md", "ROADMAP.md")

TICKET_RISKS: dict[str, str] = {
    "ambiguous_requirement_risk":
        "Does the ticket contain a material requirement whose meaning, trigger, boundary, or expected behavior is ambiguous enough that an implementer would have to guess?",
    "unbounded_scope_risk":
        "Is the ticket scope materially unbounded, open-ended, or missing a clear stopping boundary?",
    "acceptance_not_testable_risk":
        "Does at least one material acceptance criterion fail to describe independently observable/testable behavior?",
    "acceptance_coverage_gap":
        "Is a material objective or scope requirement missing a corresponding acceptance criterion?",
    "required_test_oracle_gap":
        "Does at least one required test lack a defined behavioral oracle, or require behavior not actually specified by the ticket contract?",
    "failure_semantics_gap":
        "Are failure, rollback, retry, invalid-input, partial-publication, or recovery semantics materially underspecified where the feature can fail?",
    "ownership_lifetime_contract_gap":
        "Are ownership, lifetime, destruction, aliasing, reentrancy, or retained-reference semantics materially underspecified where they matter?",
    "threading_contract_gap":
        "Are thread confinement, cross-thread behavior, or real-time boundaries materially underspecified where they matter?",
    "exception_contract_gap":
        "Are exception propagation, noexcept, unwind, destructor, or foreign-ABI rules materially underspecified where they matter?",
    "performance_contract_gap":
        "Is a material hot-path, allocation, invalidation, caching, or complexity expectation required by NativeUI architecture but missing from the ticket?",
    "platform_contract_gap":
        "Are material cross-platform, backend, native lifecycle, context-loss, or integration semantics underspecified where they matter?",
    "dependency_gap":
        "Does the ticket appear to depend on unavailable, unresolved, contradictory, or insufficiently defined prerequisite work?",
    "architecture_conflict_risk":
        "Does the ticket materially conflict with the supplied NativeUI architecture, invariants, review policy, or roadmap boundaries?",
    "scope_exclusion_gap":
        "Does the ticket leave adjacent behavior insufficiently excluded, making scope creep or responsibility overlap likely?",
    "hidden_design_decision_risk":
        "Would a competent implementer still need to invent a material product/API/architecture decision that should instead be decided in the ticket?",
    "internal_contradiction_risk":
        "Does the ticket contain materially contradictory requirements, acceptance criteria, tests, dependencies, or completion rules?",
    "overconstrained_implementation_risk":
        "Does the ticket unnecessarily prescribe implementation details that are not required for observable correctness and conflict with maintaining implementation freedom?",
}

TICKET_READINESS = {
    "ready": "The ticket is sufficiently bounded, coherent, testable, architecturally compatible, and decision-complete to begin implementation.",
    "needs_clarification": "The ticket needs bounded requirement/test clarification before implementation, but no larger architecture decision is required.",
    "needs_architecture_work": "The ticket still requires a material API/product/architecture decision before implementation.",
    "blocked_by_dependency": "The ticket is sufficiently specified but cannot responsibly start because a prerequisite or dependency is unresolved.",
    "insufficient_context": "The supplied ticket/repository context is insufficient to judge implementation readiness.",
}

RISKS: dict[str, str] = {
    "instance_isolation_risk":
        "Does the change plausibly introduce mutable state, ownership, caching, or behavior that can leak across independent NativeUI/UI/plugin instances?",
    "mutable_global_state_risk":
        "Does the change plausibly introduce mutable process-global, singleton, function-static, static-data-member, registry, or thread-local state forbidden by NativeUI policy?",
    "lifetime_reentrancy_risk":
        "Does the change plausibly contain a lifetime or reentrancy defect, including callback-sensitive raw pointers/references surviving user code or owner destruction?",
    "transaction_recovery_gap":
        "Does the change plausibly have an incomplete prepare/commit/recovery contract that can publish partial state, lose accepted work, or leave stale pending/guard state?",
    "exception_unwind_risk":
        "Does the change plausibly mishandle exceptions or unwind, including stale guards/counters, throwing teardown, partial mutation, or foreign-ABI exception escape?",
    "partial_construction_cleanup_risk":
        "Does the change plausibly acquire native, registered, or backend resources without proving cleanup after every meaningful partial-construction failure point?",
    "threading_realtime_risk":
        "Does the change plausibly violate UI-thread confinement or a real-time/audio boundary?",
    "performance_allocation_risk":
        "Does the change plausibly add avoidable allocation, full-tree work, repeated compilation/materialization, excess invalidation, or another hot-path regression?",
    "platform_integration_risk":
        "Does the change plausibly depend on incorrect or insufficiently validated macOS, Windows, Linux, Pugl, Skia, native-handle, embedding, or context-loss behavior?",
    "objc_runtime_risk":
        "If Apple Objective-C or Objective-C++ code is touched, does the change plausibly violate NativeUI runtime collision, naming, lifecycle, threading, category, or ARC/bridging rules?",
    "acceptance_coverage_gap":
        "Is at least one material acceptance criterion from the issue not convincingly covered by the implementation/diff and supplied evidence?",
    "required_test_gap":
        "Is at least one material required test, fault seam, recovery case, isolation case, or platform/sanitizer validation missing or inadequately represented?",
    "scope_creep_risk":
        "Does the change plausibly implement behavior outside the issue scope or cross an explicit exclusion/dependency boundary without a correctness reason?",
    "exact_head_evidence_risk":
        "Is the supplied evidence plausibly stale, incomplete, truncated, or not tied to the exact review target head/state?",
    "privacy_secret_risk":
        "Does the review state plausibly contain newly introduced personal information, credentials, secrets, tokens, private keys, or other data that should not be committed?",
}

DEPTH = {
    "routine": "Normal NativeUI review is sufficient; no strong extra semantic-review focus is apparent.",
    "focused": "One or a few concrete areas deserve focused reasoning-model or human inspection before final review.",
    "deep": "Several interacting correctness, lifetime, transaction, or platform risks deserve a deep architecture-level review.",
    "insufficient_evidence": "The supplied state is too incomplete, stale, truncated, or mismatched to triage safely.",
}

SECRET_PATTERNS = (
    (re.compile(r"(?i)\b(authorization\s*:\s*bearer\s+)[^\s\"']+"), r"\1<REDACTED>"),
    (re.compile(r"(?i)\b((?:api[_-]?key|token|secret|password)\s*[:=]\s*)[^\s,\"']+"), r"\1<REDACTED>"),
    (re.compile(r"-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----.*?-----END [A-Z0-9 ]*PRIVATE KEY-----", re.S), "<REDACTED_PRIVATE_KEY>"),
)


class ReviewError(RuntimeError):
    pass


def git(*args: str, cwd: Path | None = None) -> str:
    p = subprocess.run(
        ["git", *args],
        cwd=str(cwd) if cwd else None,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if p.returncode:
        raise ReviewError(f"git {' '.join(args)} failed: {p.stderr.strip()}")
    return p.stdout


def root_dir() -> Path:
    return Path(git("rev-parse", "--show-toplevel").strip())


def repo_from_remote(remote: str) -> str:
    remote = remote.strip()
    for pattern in (
        r"^git@github\.com:([^/]+/[^/]+?)(?:\.git)?$",
        r"^ssh://git@github\.com/([^/]+/[^/]+?)(?:\.git)?$",
        r"^https?://github\.com/([^/]+/[^/]+?)(?:\.git)?/?$",
    ):
        m = re.match(pattern, remote)
        if m:
            return m.group(1)
    raise ReviewError(f"cannot infer owner/repo from origin {remote!r}; use --repo")


def redact(text: str) -> str:
    for pattern, replacement in SECRET_PATTERNS:
        text = pattern.sub(replacement, text)
    return text


def redact_value(value: Any) -> Any:
    """Redact string leaves without ever rewriting serialized JSON syntax."""
    if isinstance(value, str):
        return redact(value)
    if isinstance(value, list):
        return [redact_value(item) for item in value]
    if isinstance(value, dict):
        return {key: redact_value(item) for key, item in value.items()}
    return value


def trim(text: str, limit: int, name: str) -> str:
    if len(text) <= limit:
        return text
    marker = f"\n\n--- {name} truncated: {len(text) - limit} chars omitted ---\n"
    return text[: max(0, limit - len(marker))] + marker


def request(
    url: str,
    *,
    method: str = "GET",
    headers: dict[str, str] | None = None,
    body: Any = None,
    timeout: float = 15.0,
    raw: bool = False,
) -> Any:
    h = {"User-Agent": "nativeui-jev-review/0.1"}
    if headers:
        h.update(headers)
    data = None
    if body is not None:
        data = json.dumps(body, separators=(",", ":")).encode()
        h["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            payload = r.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")[:1200]
        raise ReviewError(f"HTTP {exc.code} from {url}: {detail}") from exc
    except urllib.error.URLError as exc:
        raise ReviewError(f"request failed for {url}: {exc.reason}") from exc
    if raw:
        return payload.decode(errors="replace")
    try:
        return json.loads(payload)
    except json.JSONDecodeError as exc:
        raise ReviewError(f"invalid JSON from {url}") from exc


def call_typesafe(payload: dict[str, Any], timeout: float) -> tuple[dict[str, Any], str]:
    """Use the official Python SDK when available, otherwise fall back to stdlib HTTP."""
    try:
        from typesafe_sdk import TypeSafeClient  # type: ignore[import-not-found]
    except ImportError:
        key = (os.getenv("TYPESAFE_API_KEY") or "").strip()
        if not key:
            raise ReviewError(
                "TYPESAFE_API_KEY is required (or install typesafe-sdk and configure it as in your working scripts)"
            )
        api = os.getenv("TYPESAFE_BASE_URL", API_ROOT).strip().rstrip("/") + "/v1/systemone"
        response = request(
            api,
            method="POST",
            headers={"Authorization": f"Bearer {key}", "Accept": "application/json"},
            body=payload,
            timeout=timeout,
        )
        return response, "stdlib-http"

    try:
        with TypeSafeClient() as client:
            response = client.system_one(
                state=payload["state"],
                questions=payload["questions"],
                model=payload["model"],
                timeout=timeout,
            )
    except Exception as exc:
        raise ReviewError(f"TypeSafe SDK request failed: {exc}") from exc

    if hasattr(response, "model_dump"):
        return response.model_dump(mode="json"), "typesafe-sdk"
    raise ReviewError("TypeSafe SDK returned an unsupported response object")


def gh_headers(accept: str = "application/vnd.github+json") -> dict[str, str]:
    h = {"Accept": accept, "X-GitHub-Api-Version": "2022-11-28"}
    token = os.getenv("GITHUB_TOKEN") or os.getenv("GH_TOKEN")
    if token:
        h["Authorization"] = f"Bearer {token}"
    return h


def gh(repo: str, path: str, *, accept: str = "application/vnd.github+json", raw: bool = False, timeout: float = 15.0) -> Any:
    owner, name = repo.split("/", 1)
    url = f"https://api.github.com/repos/{urllib.parse.quote(owner)}/{urllib.parse.quote(name)}/{path.lstrip('/')}"
    return request(url, headers=gh_headers(accept), timeout=timeout, raw=raw)


def issue_snapshot(issue: dict[str, Any]) -> dict[str, Any]:
    return {
        "number": issue.get("number"),
        "title": issue.get("title"),
        "state": issue.get("state"),
        "body": issue.get("body"),
        "labels": [x.get("name") for x in issue.get("labels", []) if isinstance(x, dict)],
        "milestone": (issue.get("milestone") or {}).get("title"),
    }


def dependency_references(body: str | None) -> list[int]:
    """Extract GitHub #NNN references from the ticket's Dependencies section only."""
    if not body:
        return []
    match = re.search(
        r"(?ims)^##\s+Dependencies\s*$\n(.*?)(?=^##\s+|\Z)",
        body,
    )
    if not match:
        return []
    return sorted({int(value) for value in re.findall(r"(?<!\w)#(\d+)\b", match.group(1))})


def ticket_dependencies(repo: str, body: str | None, timeout: float) -> list[dict[str, Any]]:
    dependencies: list[dict[str, Any]] = []
    for number in dependency_references(body)[:12]:
        try:
            item = gh(repo, f"issues/{number}", timeout=timeout)
            dependencies.append({
                "number": number,
                "title": item.get("title"),
                "state": item.get("state"),
                "labels": [x.get("name") for x in item.get("labels", []) if isinstance(x, dict)],
                "is_pull_request": "pull_request" in item,
            })
        except ReviewError as exc:
            dependencies.append({"number": number, "lookup_error": str(exc)})
    return dependencies


def read_context(root: Path, names: tuple[str, ...], char_limit: int) -> dict[str, str]:
    context: dict[str, str] = {}
    for name in names:
        path = root / name
        context[name] = (
            trim(path.read_text(encoding="utf-8", errors="replace"), char_limit, name)
            if path.exists()
            else "<missing>"
        )
    return context


def local_change(root: Path, base: str, max_diff: int) -> dict[str, Any]:
    head = git("rev-parse", "HEAD", cwd=root).strip()
    merge_base = git("merge-base", head, base, cwd=root).strip()
    diff = git("diff", "--no-ext-diff", "--find-renames", merge_base, "--", cwd=root)
    return {
        "source": "local_worktree",
        "head_sha": head,
        "base_ref": base,
        "merge_base_sha": merge_base,
        "status": git("status", "--short", cwd=root),
        "changed_files": git("diff", "--name-only", merge_base, cwd=root).splitlines(),
        "diff": trim(diff, max_diff, "local diff"),
    }


def pr_change(repo: str, number: int, max_diff: int, timeout: float) -> dict[str, Any]:
    pr = gh(repo, f"pulls/{number}", timeout=timeout)
    diff = gh(repo, f"pulls/{number}", accept="application/vnd.github.v3.diff", raw=True, timeout=timeout)
    files = gh(repo, f"pulls/{number}/files?per_page=100", timeout=timeout)
    return {
        "source": "github_pr",
        "number": number,
        "title": pr.get("title"),
        "body": pr.get("body"),
        "draft": pr.get("draft"),
        "base_ref": (pr.get("base") or {}).get("ref"),
        "base_sha": (pr.get("base") or {}).get("sha"),
        "head_ref": (pr.get("head") or {}).get("ref"),
        "head_sha": (pr.get("head") or {}).get("sha"),
        "changed_files": [f.get("filename") for f in files if isinstance(f, dict)],
        "diff": trim(diff, max_diff, "PR diff"),
    }


def ticket_questions() -> dict[str, Any]:
    out: dict[str, Any] = {}
    suffix = (
        " Judge the ticket itself before implementation. Use only the supplied ticket, dependencies, "
        "and repository architecture/policy context. Do not assume a particular implementation."
    )
    for key, instruction in TICKET_RISKS.items():
        out[key] = {
            "type": "noul",
            "instructions": instruction + suffix,
            "criteria": {
                "true": "The described ticket-specification risk or gap is materially present.",
                "false": "The ticket and supplied repository context reasonably resolve this concern, or it is not applicable.",
            },
        }
    out["ticket_readiness"] = {
        "type": "choice",
        "instructions": (
            "Classify whether this NativeUI ticket is ready to enter implementation. "
            "Treat unresolved product/API/architecture decisions differently from ordinary clarification and dependency blocking."
        ),
        "criteria": TICKET_READINESS,
    }
    return out


def questions() -> dict[str, Any]:
    out: dict[str, Any] = {}
    suffix = (
        " Judge only from the supplied issue, policies, diff, metadata, and evidence. "
        "This is review triage, not merge approval."
    )
    for key, instruction in RISKS.items():
        out[key] = {
            "type": "noul",
            "instructions": instruction + suffix,
            "criteria": {
                "true": "The described risk or evidence gap is plausibly present.",
                "false": "The supplied state reasonably indicates that this risk is absent or not applicable.",
            },
        }
    out["review_depth"] = {
        "type": "choice",
        "instructions": "What additional semantic review depth should be scheduled for this exact target before NativeUI's mandatory final review?",
        "criteria": DEPTH,
    }
    return out


def analyze(response: dict[str, Any], review_at: float, high_at: float) -> dict[str, Any]:
    answers = response.get("answers")
    if not isinstance(answers, dict):
        raise ReviewError("TypeSafe response has no answers object")

    judgments = []
    for key, q in RISKS.items():
        answer = answers.get(key)
        if not isinstance(answer, dict) or answer.get("type") != "noul":
            raise ReviewError(f"missing/invalid Noul answer for {key}")
        p = answer.get("noul")
        if not isinstance(p, (int, float)) or not 0 <= float(p) <= 1:
            raise ReviewError(f"invalid probability for {key}")
        probability = float(p)
        level = "high" if probability >= high_at else "review" if probability >= review_at else "low"
        judgments.append({"id": key, "probability": probability, "level": level, "question": q})

    depth = answers.get("review_depth")
    if not isinstance(depth, dict) or depth.get("choice") not in DEPTH:
        raise ReviewError("missing/invalid review_depth Choice answer")

    escalations = [j for j in judgments if j["level"] != "low"]
    return {
        "model": response.get("model"),
        "usage": response.get("usage"),
        "review_depth": {
            "choice": depth.get("choice"),
            "confidence": depth.get("confidence"),
            "probabilities": depth.get("probabilities"),
        },
        "judgments": judgments,
        "escalations": [j["id"] for j in escalations],
        "high_risk": [j["id"] for j in judgments if j["level"] == "high"],
    }


def analyze_ticket(response: dict[str, Any], review_at: float, high_at: float) -> dict[str, Any]:
    answers = response.get("answers")
    if not isinstance(answers, dict):
        raise ReviewError("TypeSafe response has no answers object")

    judgments = []
    for key, q in TICKET_RISKS.items():
        answer = answers.get(key)
        if not isinstance(answer, dict) or answer.get("type") != "noul":
            raise ReviewError(f"missing/invalid Noul answer for {key}")
        p = answer.get("noul")
        if not isinstance(p, (int, float)) or not 0 <= float(p) <= 1:
            raise ReviewError(f"invalid probability for {key}")
        probability = float(p)
        level = "high" if probability >= high_at else "review" if probability >= review_at else "low"
        judgments.append({"id": key, "probability": probability, "level": level, "question": q})

    readiness = answers.get("ticket_readiness")
    if not isinstance(readiness, dict) or readiness.get("choice") not in TICKET_READINESS:
        raise ReviewError("missing/invalid ticket_readiness Choice answer")

    high = [j["id"] for j in judgments if j["level"] == "high"]
    escalations = [j["id"] for j in judgments if j["level"] != "low"]
    choice = readiness["choice"]

    if choice == "insufficient_context":
        status = "INSUFFICIENT_CONTEXT"
    elif choice == "blocked_by_dependency" or "dependency_gap" in high:
        status = "BLOCKED"
    elif choice == "needs_architecture_work" or any(
        key in high for key in ("architecture_conflict_risk", "hidden_design_decision_risk")
    ):
        status = "NEEDS_ARCHITECTURE_WORK"
    elif choice == "needs_clarification" or high:
        status = "NEEDS_WORK"
    else:
        status = "READY"

    return {
        "model": response.get("model"),
        "usage": response.get("usage"),
        "ticket_status": status,
        "ticket_readiness": {
            "choice": choice,
            "confidence": readiness.get("confidence"),
            "probabilities": readiness.get("probabilities"),
        },
        "judgments": judgments,
        "escalations": escalations,
        "high_risk": high,
    }


def ticket_focus_prompt(result: dict[str, Any], issue_number: int) -> str:
    lines = [
        f"Review and improve NativeUI ticket #{issue_number}; do not implement it.",
        f"Current ticket validation status: {result['ticket_status']}.",
        "Jev probabilities are triage signals, not evidence. Verify each item against the ticket and repository policies.",
        "Preserve the ticket's product intent while resolving specification gaps; do not invent implementation work outside scope.",
    ]
    by_id = {j["id"]: j for j in result["judgments"]}
    for key in result["escalations"]:
        j = by_id[key]
        lines.append(f"- {key}: p={j['probability']:.3f} ({j['level']}) — {j['question']}")
    if not result["escalations"]:
        lines.append("- No specification risk crossed the review threshold; perform a final consistency pass only.")
    lines.append("Return a corrected ticket draft and explicitly list every material clarification you made.")
    return "\n".join(lines)


def focus_prompt(result: dict[str, Any], head: str | None) -> str:
    lines = [
        "Perform a focused NativeUI review against AGENTS.md and CODE_REVIEW.md.",
        f"Review target head/state: {head or '<working tree>'}.",
        "Jev probabilities are triage signals, not evidence or conclusions.",
    ]
    by_id = {j["id"]: j for j in result["judgments"]}
    for key in result["escalations"]:
        j = by_id[key]
        lines.append(f"- {key}: p={j['probability']:.3f} ({j['level']}) — {j['question']}")
    if not result["escalations"]:
        lines.append("- No risk crossed the escalation threshold; still perform mandatory normal review and deterministic validation.")
    return "\n".join(lines)


def write_json(path: str, value: Any) -> None:
    data = json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    if path == "-":
        sys.stdout.write(data)
    else:
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(data, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Triage a NativeUI implementation/review with TypeSafe Jev.")
    p.add_argument("issue", type=int, help="GitHub issue/ticket number")
    p.add_argument("--pr", type=int, help="review this GitHub PR exact head instead of local worktree")
    p.add_argument(
        "--ticket-only",
        action="store_true",
        help="validate the ticket specification before implementation; no diff or implementation is inspected",
    )
    p.add_argument("--repo", help="owner/name; default inferred from origin")
    p.add_argument("--base", default="origin/main", help="local comparison base when --pr is omitted")
    p.add_argument("--model", default=os.getenv("TYPESAFE_DEFAULT_MODEL", DEFAULT_MODEL))
    p.add_argument("--output", default="-", help="result JSON file or - for stdout")
    p.add_argument("--request-out", help="optional redacted System One request JSON")
    p.add_argument("--dry-run", action="store_true", help="build request without calling TypeSafe")
    p.add_argument("--no-redact", action="store_true", help="disable heuristic secret redaction")
    p.add_argument("--max-diff-chars", type=int, default=180000)
    p.add_argument("--policy-chars", type=int, default=50000)
    p.add_argument("--review-at", type=float, default=0.50)
    p.add_argument("--high-at", type=float, default=0.75)
    p.add_argument("--timeout", type=float, default=15.0)
    a = p.parse_args()
    if not 0 <= a.review_at <= a.high_at <= 1:
        p.error("require 0 <= --review-at <= --high-at <= 1")
    if a.ticket_only and a.pr is not None:
        p.error("--ticket-only cannot be combined with --pr")
    return a


def main() -> int:
    a = parse_args()
    try:
        root = root_dir()
        repo = a.repo or repo_from_remote(git("remote", "get-url", "origin", cwd=root))
        issue = gh(repo, f"issues/{a.issue}", timeout=a.timeout)

        if a.ticket_only:
            change = None
            state = {
                "repository": repo,
                "validation_contract": (
                    "Validate the ticket specification before implementation. Determine whether a competent NativeUI "
                    "implementer can proceed without inventing material product/API/architecture decisions. "
                    "No implementation or diff is being reviewed."
                ),
                "issue": issue_snapshot(issue),
                "dependencies": ticket_dependencies(repo, issue.get("body"), a.timeout),
                "repository_context": read_context(root, TICKET_CONTEXT, a.policy_chars),
            }
            qs = ticket_questions()
        else:
            change = pr_change(repo, a.pr, a.max_diff_chars, a.timeout) if a.pr else local_change(root, a.base, a.max_diff_chars)
            state = {
                "repository": repo,
                "review_contract": "Jev provides semantic triage only. NativeUI exact-head review policy and deterministic validation remain authoritative.",
                "issue": issue_snapshot(issue),
                "change": change,
                "policies": read_context(root, POLICIES, a.policy_chars),
            }
            qs = questions()
        if not a.no_redact:
            state = redact_value(state)

        payload = {"model": a.model, "state": state, "questions": qs}
        fingerprint = hashlib.sha256(
            json.dumps(state, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
        ).hexdigest()

        if a.request_out:
            write_json(a.request_out, payload)

        if a.dry_run:
            write_json(a.output, {
                "schema_version": 1,
                "dry_run": True,
                "mode": "ticket" if a.ticket_only else "implementation",
                "repository": repo,
                "issue": a.issue,
                "pull_request": None if a.ticket_only else a.pr,
                "head_sha": None if a.ticket_only else change.get("head_sha"),
                "state_sha256": fingerprint,
                "request": payload,
            })
            return 0

        response, typesafe_backend = call_typesafe(payload, a.timeout)

        if a.ticket_only:
            result = analyze_ticket(response, a.review_at, a.high_at)
            out = {
                "schema_version": 1,
                "dry_run": False,
                "mode": "ticket",
                "repository": repo,
                "issue": a.issue,
                "typesafe_backend": typesafe_backend,
                "state_sha256": fingerprint,
                "thresholds": {"review_at": a.review_at, "high_at": a.high_at},
                **result,
            }
            out["codex_ticket_prompt"] = ticket_focus_prompt(out, a.issue)
            write_json(a.output, out)
            print(
                f"JEV ticket validation: status={out['ticket_status']} "
                f"readiness={out['ticket_readiness']['choice']} "
                f"escalations={len(out['escalations'])} high={len(out['high_risk'])} "
                f"state={fingerprint[:12]}",
                file=sys.stderr,
            )
        else:
            result = analyze(response, a.review_at, a.high_at)
            out = {
                "schema_version": 1,
                "dry_run": False,
                "mode": "implementation",
                "repository": repo,
                "issue": a.issue,
                "pull_request": a.pr,
                "review_target": change.get("source"),
                "typesafe_backend": typesafe_backend,
                "head_sha": change.get("head_sha"),
                "state_sha256": fingerprint,
                "thresholds": {"review_at": a.review_at, "high_at": a.high_at},
                **result,
            }
            out["codex_focus_prompt"] = focus_prompt(out, change.get("head_sha"))
            write_json(a.output, out)
            print(
                f"JEV triage: depth={out['review_depth']['choice']} "
                f"escalations={len(out['escalations'])} high={len(out['high_risk'])} "
                f"state={fingerprint[:12]}",
                file=sys.stderr,
            )
        return 0
    except ReviewError as exc:
        print(f"jev-review: error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
