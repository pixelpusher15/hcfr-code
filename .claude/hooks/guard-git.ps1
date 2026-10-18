# Repo guard: blocks git/gh operations that violate HCFR rules.
# Wired as a PreToolUse(Bash) hook. Exit 2 = block (stderr shown to Claude).
# Rules: never commit/push to main; never touch upstream arthur-liberman;
#        gh pr create must target the pixelpusher15/hcfr-code fork.
$ErrorActionPreference = 'Stop'

try {
    $raw  = [Console]::In.ReadToEnd()
    $data = $raw | ConvertFrom-Json
    $cmd  = [string]$data.tool_input.command
} catch { exit 0 }   # unparseable input -> don't block

if ([string]::IsNullOrWhiteSpace($cmd)) { exit 0 }

function Block([string]$why) {
    [Console]::Error.WriteLine(
        "BLOCKED by repo guard: $why`n" +
        "All work goes on a feature branch -> PR to pixelpusher15/hcfr-code. " +
        "Never commit/push to main, never target upstream arthur-liberman.")
    exit 2
}

$lc = $cmd.ToLower()

# (2) never touch upstream
if ($lc -match 'arthur-liberman') { Block "command references upstream arthur-liberman." }

# (3) gh pr create must target the fork
if ($lc -match 'gh\s+pr\s+create' -and $lc -notmatch '--repo\s+pixelpusher15/hcfr-code') {
    Block "gh pr create without --repo pixelpusher15/hcfr-code (would mis-target)."
}

# (1) no commit/push to main
$branch = ''
try { $branch = (& git rev-parse --abbrev-ref HEAD 2>$null | Out-String).Trim() } catch { $branch = '' }
$onMain = ($branch -eq 'main' -or $branch -eq 'master')

if ($lc -match 'git\s+commit' -and $onMain) { Block "git commit while on '$branch'." }
if ($lc -match 'git\s+push') {
    if ($onMain) { Block "git push while on '$branch'." }
    if ($lc -match 'push\s+\S+\s+(main|master)\b' -or $lc -match ':(refs/heads/)?(main|master)\b') {
        Block "git push targeting main/master."
    }
}

exit 0
