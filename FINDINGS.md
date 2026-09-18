# Everything results in Windows 11 search — reconnaissance

Measured on Windows 11 25H2 (build 26200) on 2026-09-18/19. Everything is
what was measured; guesses are marked as such.

## The goal

Windows search keeps its own results (apps, settings, web). We add file and
folder results from [Everything](https://www.voidtools.com/), which indexes the
MFT and answers instantly. The mod never ships Everything — the user installs
it, and we query it.

## Established by measurement

| finding | evidence |
|---|---|
| Windhawk can inject into `SearchHost.exe` | recon mod ran; `InjectWindhawkTAP -> 00000000` |
| Its XAML tree is walkable, with named anchors | `Cortana.UI.Views.TaskbarSearchPage`, `Grid#RootGrid`, `CortanaRichSearchBox#SearchTextBox` |
| `SearchHost` is a Low-IL AppContainer | integrity `S-1-16-4096`, `IsAppContainer: 1` |
| ...and its file writes are redirected | our `%TEMP%` log landed in `…\Packages\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\AC\Temp\` |
| `StartMenuExperienceHost` is **not** sandboxed | Medium IL, `AppContainer: 0` |
| `ShellHost` (Control Center) is not sandboxed | Medium IL — this is why the brightness mod works |
| `ShellExperienceHost` **is** sandboxed | Low IL, AppContainer |
| The query is readable from outside the sandbox | UIA `ValuePattern` returned the live text, `ControlType.Edit`, class `RichEditBox` |
| Everything answers over IPC and HTTP | IPC window `EVERYTHING_TASKBAR_NOTIFICATION` found; 2,434 results for `readme` |
| A no-activate panel does not dismiss Start | 21 clicks received, **zero** `WM_ACTIVATE` |
| There is room beside the flyout | flyout at `531,135 858x890`; 531px free to its right on a 1920 screen |
| Start and Search are separate processes/windows | different PIDs, different integrity levels, separate `CoreWindow`s |
| The flyout raises show/hide events | on `Windows.UI.Core.CoreComponentInputSource`, **not** `CoreWindow` |
| `IsWindowVisible` does not track the flyout | it is never visible by that test even while on screen — likely DWM cloaking |
| The shell ignores synthetic `Win` keypresses | `keybd_event` produced no Start menu; real hardware input works |
| The Start button exposes no `InvokePattern` | UIA invoke throws |

## The wall, and the way round it

`SearchHost` is a Low-integrity AppContainer. Code injected there **runs fine**
— injection is not the problem — but it cannot reach out:

- no `WM_COPYDATA` to Everything (UIPI blocks Low -> Medium)
- no loopback HTTP (AppContainer blocks it by default)
- no general filesystem access

The restriction is directional. **Medium -> Low is permitted**, so data can be
pushed *down* into the sandbox even though nothing can be pulled *out*.

## Architectures considered

**A. External companion panel.** Our own `WS_EX_NOACTIVATE` window beside the
flyout; query via UIA; Everything queried directly. *Proven end to end.*
Lowest fragility (depends on UIA properties, not internal names). Cannot share
the flyout's animation — separate composition tree. Dismiss-sync is awkward.

**B. XAML injected into `SearchHost`, data pushed down.** Panel lives in the
flyout's own tree, so it shares the animation for free and is only present on
the search page by construction. A Medium-IL broker reads the query via UIA,
queries Everything and sends results down via `WM_COPYDATA`. Nothing is
weakened — this is the permitted direction.
**Verified** -- `WM_COPYDATA` reaches an AppContainer-owned window. This is
the chosen architecture.

**C. Inject into `StartMenuExperienceHost`.** Medium IL, no sandbox at all, so
no broker and no bridge. **Ruled out** -- see below: the window survives but is
cloaked while search is showing.

**D. Suppress the search page and replace it wholesale.** Possible —
suppression needs no outbound access — but then the mod owns apps, settings,
documents and web search. Rejected on scope.

**E. Replace `SearchHost.exe`.** TrustedInstaller-owned, inside a
system-signed package (`SignatureKind: System`), activated by manifest entry
`CortanaUI`, serviced on its own `2607.x` cadence. Rejected.

**F. Query redirect.** Hook a stable public API and launch Everything's own
window with the query, as the Bing redirector hooks
`combase!WindowsCreateString`. Nearly update-proof, but a different product —
no inline results.

## Fragility

`SearchHost` and the `SearchUx.*` DLLs are version `2607.28006.200.0` — a
date-coded app train inside the CBS package, serviced **out of band from
Windows Update** and more often than the OS. `ShellHost`, where the brightness
mod lives, is a plain System32 binary versioned with the OS. Anything that
depends on `Cortana.UI.*` names is therefore on shakier ground than the
brightness mod, and should fail safe: if the anchor is missing, inject nothing
and leave search working normally.

## Resolved since (2026-09-19)

**Neither flyout window is ever destroyed.** Both `StartMenuExperienceHost`
and `SearchHost` own a permanent `Windows.UI.Core.CoreWindow`, always
`IsWindowVisible == TRUE`, shown and hidden purely by **DWM cloaking**. Start's
is `1920x1025 @ 0,0`; search's is `858x890 @ 531,135`. Measured from inside
Start via a recon mod, three identical cycles:

```
search cloaked=0   start cloaked=0    <- Win pressed, both uncloak
search cloaked=0   start cloaked=2    <- typed, Start cloaks
search cloaked=2   start cloaked=2    <- Esc, both cloak
```

This kills **option C**: Start's window survives, but it is cloaked exactly
while the search page is showing, so anything drawn there would be invisible
at the only moment it matters.

It also gives a precise, durable detector for "the search page is up":

    search window uncloaked AND start window cloaked

via `DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED)`. The handles never go stale
because the windows are never destroyed. This depends on no XAML names and so
is immune to the `2607.x` app train. **`IsWindowVisible` is useless here** --
it is permanently TRUE for both.

**`WM_COPYDATA` crosses into the AppContainer.** A normal-integrity process
found the sandboxed window with `FindWindow` and sent it a payload, which
arrived intact:

```
SendMessage(WM_COPYDATA) returned 1, GetLastError=0
RECEIVED WM_COPYDATA  76 bytes  text='hello from a normal-integrity process'
```

So **option B is fully viable**, with nothing left assumed. Note the receiving
window must be a real top-level window, not `HWND_MESSAGE` -- message-only
windows are not findable from another process.

## Plan B proven end to end (2026-09-19)

A paint-test mod constructed a `Border`, a `SolidColorBrush` and a `TextBlock`
**inside the Low-IL AppContainer** and appended them to Microsoft's own
`Grid#RootGrid` under `Cortana.UI.Views.TaskbarSearchPage`:

```
InjectWindhawkTAP (attempt 1) -> 00000000
TaskbarSearchPage added - attempting to inject a visible element
found Grid#RootGrid
SUCCESS: element appended to RootGrid
```

and the element **rendered on screen**. So XAML construction and rendering
work in the sandbox; only reaching *out* is blocked, which is what the
`WM_COPYDATA` push solves. Nothing in plan B is assumed any more.

Note the TAP must be deferred until a XAML window exists -- calling it earlier
returns `ERROR_NOT_FOUND` (`0x80070490`). This is the same trap as in the
per-monitor-brightness mod, and it bites whenever the host has just restarted,
which installing a mod causes.

**Plan C is closed, measured.** A hook on `DwmSetWindowAttribute` inside
`StartMenuExperienceHost` logged **no** `DWMWA_CLOAK` call while Start opened
and handed off to search. The cloak is applied from outside the process, so a
mod injected there cannot intercept it.

## Open questions

1. Do the search results render in XAML or in the `HostedWebView2Control`
   seen in the idle tree? Only the idle page was captured; the tree with
   results showing was never dumped. This decides whether the apps-|-files
   split is cheap or expensive -- not whether it is possible.
2. Is there an existing right-hand pane in the results layout that could be
   filled, instead of splitting the content area ourselves?

## Design notes

- Two-panel split (apps | files) inside the search page is the best-looking
  option and the most fragile: it couples us to how Microsoft lays out that
  region. Filling an existing pane, if one exists, would be far cheaper.
- "Empty query shows Start, typing shows search" needs no implementation —
  it is the process handoff, and it is why page detection is free.
- Animation cannot be shared by an external window. A deliberate ~100ms
  stagger reads as intentional; a near-miss sync reads as broken.
- Dismissal matters more than entrance. A panel that lingers after the flyout
  is gone looks broken in a way a late entrance never does.
