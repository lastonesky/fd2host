#!/usr/bin/env bash
# run_rounds.sh - two-agent autonomous FD2 translation loop.
#
#   planner  : read-only recon, writes build/agents/next_translation.md
#   executor : implements exactly one full verified round per AGENTS.md, commits
#
# Usage:  bash tools/agents/run_rounds.sh [rounds]
# Env:    PI_BIN, MODEL_PLANNER, MODEL_EXECUTOR, PI_EXTRA
#
# The loop refuses to start (or continue) while the worktree is dirty, so it can
# never interleave with a human/agent edit. It stops on the first failure.
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

PI_BIN="${PI_BIN:-/c/Users/Ted/AppData/Local/hermes/node/pi}"
MODEL_PLANNER="${MODEL_PLANNER:-deepseek/deepseek-flash:low}"
MODEL_EXECUTOR="${MODEL_EXECUTOR:-deepseek/deepseek-flash:high}"
PI_EXTRA="${PI_EXTRA:-}"
ROUNDS="${1:-8}"
LOGDIR="$ROOT/build/agents"
NEXT="$LOGDIR/next_translation.md"
mkdir -p "$LOGDIR"

log() { echo "[$(date '+%F %T')] $*" | tee -a "$LOGDIR/loop.log"; }

log "agent loop start: rounds=$ROUNDS planner=$MODEL_PLANNER executor=$MODEL_EXECUTOR"

for ((r = 1; r <= ROUNDS; r++)); do
    if [ -n "$(git status --porcelain)" ]; then
        log "round $r: worktree dirty at loop top; refusing to continue"
        git status --short | tee -a "$LOGDIR/loop.log"
        break
    fi

    HEAD_BEFORE="$(git rev-parse HEAD)"
    log "==== round $r planner ($HEAD_BEFORE) ===="
    timeout 2400 "$PI_BIN" --print --no-session \
        --model "$MODEL_PLANNER" \
        --exclude-tools 'subagent,subagents_enable' $PI_EXTRA \
        --append-system-prompt "只读规划 agent：只允许写 build/agents/next_translation.md；不得修改 src/ 或 docs/，不得运行构建/回归。严格按 tools/agents/planner.md 执行。" \
        "$(cat tools/agents/planner.md)" \
        >"$LOGDIR/round-${r}-planner.log" 2>&1
    prc=$?
    log "planner exit=$prc"
    if [ $prc -ne 0 ] || [ ! -s "$NEXT" ]; then
        log "planner produced no assignment (exit=$prc); stopping at round $r"
        break
    fi
    cp "$NEXT" "$LOGDIR/round-${r}-assignment.md"

    log "==== round $r executor ===="
    timeout 14400 "$PI_BIN" --print --no-session \
        --model "$MODEL_EXECUTOR" \
        --exclude-tools 'subagent,subagents_enable' $PI_EXTRA \
        --append-system-prompt "执行 agent：按 AGENTS.md + docs/TRANSLATION.md 完成恰好一个转译轮次，最后 git commit。任何一步失败就回退到本轮开始前的提交并把阻塞写入 build/agents/next_translation.md。严格按 tools/agents/executor.md 执行。" \
        "$(cat tools/agents/executor.md)" \
        >"$LOGDIR/round-${r}-executor.log" 2>&1
    erc=$?
    HEAD_AFTER="$(git rev-parse HEAD)"
    log "executor exit=$erc head $HEAD_BEFORE -> $HEAD_AFTER"

    if [ $erc -ne 0 ] || [ "$HEAD_BEFORE" = "$HEAD_AFTER" ]; then
        log "executor did not land a commit (exit=$erc); stopping at round $r"
        break
    fi
done

log "agent loop end"
