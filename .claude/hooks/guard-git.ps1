# Repo guard: blocks git/gh operations that violate HCFR rules.
# Wired as a PreToolUse(Bash|PowerShell) hook. Exit 2 = block (stderr shown to Claude).
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

# (2) never touch upstream
if ($cmd -match 'arthur-liberman') { Block "command references upstream arthur-liberman." }

# (3) gh pr create must target the fork (--repo X, --repo=X, -R X, -R=X)
if ($cmd -match '\bgh\s+pr\s+create\b' -and
    $cmd -cnotmatch '(--repo|-R)(\s+|=)["'']?pixelpusher15/hcfr-code(?=$|\s|["''])') {
    Block "gh pr create without --repo pixelpusher15/hcfr-code (would mis-target)."
}

# (1) no commit/push to main.
# A git invocation may carry global options before the subcommand
# (git -C <dir>, git -c k=v, git --no-pager, git --git-dir=<dir>).
# The <pre> capture holds only those global options, so a subcommand's own
# flags (e.g. `git commit -C <commit>`) are never mistaken for `git -C <dir>`.
$gitPre = '\bgit(?<pre>(\s+(-[cC]\s+("[^"]*"|''[^'']*''|\S+)|--?[a-z][\w-]*(=\S+)?))*)\s+'

# Branch of the repo a git invocation acts on: its global -C target if given,
# else the cwd. An unresolvable -C target falls back to the cwd's branch.
function BranchFor([string]$pre) {
    $b = ''
    if ($pre -cmatch '-C\s+(?:"(?<d>[^"]*)"|''(?<d>[^'']*)''|(?<d>\S+))') {
        try { $b = (& git -C $Matches['d'] rev-parse --abbrev-ref HEAD 2>$null | Out-String).Trim() } catch { $b = '' }
    }
    if (-not $b) {
        try { $b = (& git rev-parse --abbrev-ref HEAD 2>$null | Out-String).Trim() } catch { $b = '' }
    }
    return $b
}
function IsMain([string]$b) { return ($b -eq 'main' -or $b -eq 'master') }

if ($cmd -match ($gitPre + 'commit\b')) {
    $branch = BranchFor $Matches['pre']
    if (IsMain $branch) { Block "git commit while on '$branch'." }
}
if ($cmd -match ($gitPre + 'push\b(?<args>[^;&|\r\n]*)')) {
    $pushArgs = $Matches['args']
    $branch   = BranchFor $Matches['pre']
    if (IsMain $branch) { Block "git push while on '$branch'." }
    # any argument naming main/master as a ref (optionally quoted or forced),
    # or a refspec whose destination is main/master
    if ($pushArgs -match '(^|\s)["'']?\+?(refs/heads/)?(main|master)["'']?(\s|$)' -or
        $pushArgs -match ':(refs/heads/)?(main|master)\b') {
        Block "git push targeting main/master."
    }
}

exit 0
