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
**Unverified:** whether `WM_COPYDATA` reaches an AppContainer-owned window.

**C. Inject into `StartMenuExperienceHost`.** Medium IL, no sandbox at all, so
no broker and no bridge.
**Unverified:** whether Start's window survives the handoff to `SearchHost`.
If it is destroyed there is nothing to render into. Four attempts to measure
this lapsed for want of someone at the keyboard; the reliable way is a recon
mod that logs its own window lifecycle from inside, so no timing is involved.

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

## Open questions

1. Does `StartMenuExperienceHost`'s window survive the handoff? (decides C)
2. Does `WM_COPYDATA` from Medium IL reach an AppContainer window? (decides B)
3. Do the search results render in XAML or in the `HostedWebView2Control`
   seen in the idle tree? Only the idle page was captured; the tree with
   results showing was never dumped.
4. Is there an existing right-hand pane in the results layout that could be
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
