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

## The results are web content, not XAML (2026-09-19)

763 elements were logged as the search page built itself while typing. There
is **no `ListView`, no `ItemsRepeater`, no `ListViewItem`** anywhere. The only
content-bearing element is:

```
Cortana.UI.Views.HostedWebView2Control  #QueryFormulationHostedWebView2  832x660
Windows.UI.Xaml.Controls.Grid           #WebViewGrid                     832x660
```

The flyout is 858x890; the search box takes the top strip and the web view
fills everything below. So "Best match", the result rows, and the details pane
with Open / Open file location / Share / Copy path are all **web content**.

Consequences:

- There is no XAML results list to add rows to, and no item template to reuse
  for free styling. Our panel must be styled by hand.
- `#WebViewGrid` is an ordinary XAML Grid we *can* move and resize, so a
  side-by-side layout is still reachable -- but as a hard boundary between
  their surface and ours, not an interleaving.

**Shrinking the web view does not work.** Pushing its right edge in by 330px
made the web content hit a responsive breakpoint: it dropped its results list
entirely and showed only the details pane. Losing Microsoft's results defeats
the point of adding to search rather than replacing it.

**Widening the flyout window is permitted** -- `SetWindowPos` on the
`CoreWindow` succeeded and the new width stuck. Whether widening *plus* an
equal margin nudge preserves the results at their original width is still
**untested**: three attempts were invalidated by my own bugs, not by the
approach.

## Taking the results surface over works (2026-09-19)

The winning move came from inspecting the tree in UWPSpy:

```
TaskbarSearchPage > Grid#RootGrid > Grid#QueryFormulationRoot
  > QueryFormulationControl#QueryFormulation > Grid
    > HostedWebView2Control#QueryFormulationHostedWebView2
      > Grid#WebViewGrid > WebView2Standalone.Controls.WebView2
```

Setting `Visibility = Collapsed` on `#QueryFormulationHostedWebView2` and
appending our own `Grid` to its parent `Panel` gives us the whole results area,
cleanly. A two-column layout rendered full width with no artefacts.

This succeeds precisely because it does **not** ask Microsoft's layout to fit
into less space -- the web view leaves the layout entirely, so there is no
breakpoint to trip. Shrinking failed for the opposite reason.

Reversible: flip `Visibility` back and remove our grid.

**The consequence is a product decision, not a technical one.** Owning that
surface means losing Microsoft's results wholesale -- apps, settings,
documents and web. A two-column "apps | files" layout therefore has to supply
the apps column itself; app entries are enumerable from the `AppsFolder` shell
namespace, with icons and launch verbs. Settings and web results would simply
be gone unless rebuilt.

So there are two honest products here:
- **Add to search**: a panel beside the flyout (proven, option A), Microsoft's
  results untouched.
- **Replace search**: take the surface over (proven, this section) and supply
  apps and files ourselves -- faster and local, but you own the whole
  experience.

## The apps column: shell:AppsFolder (2026-09-19)

`shell:AppsFolder` is the virtual folder that unions Win32 shortcuts and Store
apps. Measured on this machine:

- **187 apps** enumerated -- 26 Store/UWP, 161 classic
- Each carries a display name and a launchable identifier
- Substring matching works well: `note` finds Notepad, `term` finds Terminal,
  `ste` finds Steam and SteelSeries

Identifiers come in several shapes, and this matters:

```
Brave                                          plain name
C:\...\ollama app.exe                          full path
Microsoft.WindowsNotepad_8wekyb3d8bbwe!App     UWP AUMID
{1AC14E77-...}\msconfig.exe                    known-folder GUID + relative
http://support.steampowered.com/               URL
```

**Do not rebuild a path by prefixing `shell:AppsFolder\`** -- the GUID-relative
ones fail to resolve that way (`0x80070002`). Keep the `IShellItem` from
enumeration and work from it directly: that gave **187 of 187 icons, zero
failures**, across every shape.

### Timing dictates the design

| operation | cost |
|---|---|
| enumerate names and identifiers | ~200 ms |
| enumerate + fetch all 187 icons | ~1820 ms (~9.7 ms each) |

Icons are ~90% of the cost, so they cannot be fetched per keystroke. The
broker should:

1. enumerate names and identifiers once at startup and cache them -- filtering
   187 entries in memory is then effectively free per keystroke;
2. fetch icons lazily on a background thread, only for the rows actually
   shown, cached by identifier, with rows rendering immediately and icons
   arriving a frame later;
3. re-enumerate on app install/uninstall via `SHChangeNotify`, not on a timer.

Note the apps enumeration must live in the **broker**, not the panel:
`SearchHost` is a Low-IL AppContainer with no shell namespace access.

## Traps found the hard way

- **The page-add callback fires before layout.** At `TaskbarSearchPage` add
  time, `WebViewGrid` measures `0x0` and the window is still at some default
  size. Anything that measures or resizes must wait for layout --
  `SizeChanged`/`LayoutUpdated` on the page, not the tree-change callback.
- **Teardown must marshal to the XAML thread.** A mod whose uninit runs on
  another thread and therefore skips cleanup leaves its elements and its
  layout changes in the shell's tree. They survive the mod being unregistered,
  pollute the baseline for the next run, and stack up visibly.
- **Restart the host between experiments.** The tree is only truly clean after
  `SearchHost` restarts.

## Open questions

1. Does widening the flyout while holding the web view at its original width
   preserve the results list? (the three invalid runs above)
2. Can the widened flyout be positioned sensibly on narrower screens, where
   858 + panel would not fit to the right?

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

## Everything IPC (broker, files column)

Everything 1.4.1.1032, queried through its hidden `EVERYTHING_TASKBAR_NOTIFICATION`
window with `WM_COPYDATA`. No SDK DLL and no HTTP server: the IPC window is
present whenever Everything runs, the other two are not.

- **UIPI silently eats the reply when the broker is elevated.** The query is
  accepted (`SendMessage` returns 1), Everything runs the search, and then
  sends the results *up* to a higher-integrity window, which Windows drops
  without telling either side. It is indistinguishable from a timeout, and it
  cost most of an evening spent suspecting the struct layout instead.
  `ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr)` on
  the reply window fixes it outright: zero replies in 2 s before, ~30 ms
  after. Everything runs at medium, so this only bites an elevated broker —
  but it will bite anyone testing from an admin shell.
- **The struct layouts were never wrong.** Checked against the official
  `everything_ipc.h`: `QUERYW`, `LISTW` and `ITEMW` matched what had been
  written from memory, field for field. The failure was entirely the integrity
  level.
- **`QUERY2` (message 18) is worth using over `QUERYW` (message 2).** It adds a
  sort order and returns size, dates, attributes and run count. Everything
  answers `FALSE` if it does not support it, so the fallback is free to keep.
- **The documented `QUERY2` field order disagrees with the flag bit order** —
  the header lists `SIZE` before `EXTENSION` while `EXTENSION` is the lower
  bit. The subset actually used (name, path, size, date-modified, attributes,
  run count) is ordered identically either way; that was confirmed against a
  live reply by hex dump and by checking that item strides closed exactly
  (376 bytes = 60 name + 292 path + 24 fixed). Anything outside that subset
  needs the same check before it is read.

### Latency

Best of 3, milliseconds, by pool size:

| query | matches | 8 | 50 | 200 | 300 | 500 | 1000 |
|---|---|---|---|---|---|---|---|
| `c` | 589945 | 11.3 | 11.7 | 16.6 | 48.6 | 65.7 | 145.6 |
| `code` | 8365 | 30.1 | 28.4 | 32.8 | 39.3 | 44.3 | 51.4 |
| `readme` | 2434 | 29.6 | 28.0 | 32.8 | 37.6 | 39.2 | 50.3 |
| no matches | 0 | 22.6 | 23.0 | 23.5 | 27.1 | 24.6 | 25.2 |

- **The first version of this table was measuring my own code.** Waiting for
  the reply with a `Sleep(1)` poll loop put a floor under every query at the
  15.6 ms system timer tick, so the numbers came out quantised at 15 / 30 / 45
  and a query with *zero* matches cost 30 ms. `MsgWaitForMultipleObjectsEx`
  with `QS_ALLINPUT` removed it. Worth remembering before trusting any
  latency number measured through a polling wait.
- ~23 ms is a fixed cost inside Everything that no pool size avoids.
- A pool of 200 stays near that floor for every query including a single
  letter; 300 already costs 48 ms on `c`.

### Ranking

Everything only guarantees name-ascending is instant — every other sort
depends on a fast-sort the user may not have enabled. Name-ascending is not a
presentation order (`code` leads with `-abstract-code-quality-task`), so the
broker over-fetches and reorders.

- **A weighted sum was the wrong shape.** Adding recency and run-count bonuses
  to a match score left thousands of results tied, and the top eight for
  `code` were eight unrelated folders all named "Code". A lexicographic key —
  match class, then run count, then modified time — makes the tie-breaks total
  and explicit.
- **Path-based demotion matters more than any scoring tweak.** Sending
  `node_modules`, `.gradle`, `WinSxS`, `site-packages` and friends below every
  other match class is what turns `brightness` from three WinSxS manifests
  into `brightness_engine.h`.
- **Everything's run count is sparse** — it only counts opens made through
  Everything itself. Zero for essentially everything on this machine, so it is
  currently doing nothing. Kept because when it is set it is the best evidence
  available, but it should not be relied on.
- **Open limitation:** the pool is the alphabetically first N matches, so
  ranking only reorders what that window caught. A recently edited file whose
  name sorts late never enters it. The fix needs a server-side
  date-modified-descending sort, which is fast only with the matching
  fast-sort enabled. Undecided: keep as is, or probe the date sort's cost once
  at startup and use it when cheap.

### Apps column

- `shell:AppsFolder` enumerates 187 apps in ~230 ms, names and identities
  only. Icons cost ~40–220 ms each, so they stay lazy and per visible row.
- Keep the enumerated PIDL. Re-parsing the display name fails for the
  GUID-relative entries; going through the item identity produced icons for
  187 of 187.

## The panel mod

`everything-search.wh.cpp` renders the two columns inside `SearchHost`, and
`broker/panel_push.exe` feeds it real data from the command line. Both halves
verified end to end on a live search flyout.

### Take over late, not early

The mod finds the web view host and builds its panel when the page is first
laid out, but leaves the stock results **visible** until the first batch of
rows actually arrives. Without a broker running, search behaves exactly as
Windows shipped it.

This was worth doing for its own sake, and it also paid for itself during
development: every reinstall, restart and crash left the machine with working
search instead of an empty panel.

### The reply channel: a return value, truncated to 32 bits

The panel cannot send anything upward -- `WM_COPYDATA` from low to normal
integrity is dropped. It answers by returning a value from its window
procedure instead, which is the result of the broker's own call rather than a
message of its own, and does cross back.

**But the return value is truncated to 32 bits.** The panel returned
`0x4000000100010002`; the broker received `0x00010002`, exactly the low half.
The first encoding put the validity bit at 62 and the sequence number at
32..47, so every part of it that mattered was silently discarded and the
click looked like it had never happened. Repacked into 32 bits: validity at
bit 31, 15-bit sequence, 4-bit kind, 12-bit index.

It also comes back **sign-extended** -- the working value arrived as
`0xFFFFFFFF80011002` -- so the decode masks to 32 bits before testing the
validity bit.

Diagnosing this needed logging on both sides at once. The panel's log proved
it had produced the right value; only the broker's log showed what arrived.
Either alone would have pointed at the wrong half.

### Stale listener windows are dangerous, not merely untidy

Teardown has to run on the XAML thread, and `Wh_ModUninit` frequently does
not. The old behaviour -- log "wrong thread" and give up -- leaves the panel
in the tree *and* leaves a top-level window whose window procedure points
into an image Windhawk is about to unload.

The next broker to call `FindWindow` gets that corpse. A `SendMessage` to it
blocked for 3.9 s and then took `SearchHost` down.

Two fixes, both needed:

- The mod now marshals teardown by sending a private message to its own
  listener window, which belongs to the XAML thread. That is a synchronous
  hop onto the only thread allowed to touch the tree or destroy the window.
  `SendMessageTimeout` with `SMTO_ABORTIFHUNG`, because a hung XAML thread
  must not hang the unload -- leaving the panel behind beats deadlocking the
  shell. Verified: "handed the results surface back / removed the panel /
  teardown marshalled to the XAML thread: done".
- The broker never uses a bare `SendMessage`. Every call is
  `SendMessageTimeout`, so a window left behind by an earlier build fails the
  call instead of hanging the broker.

### Measured

| | |
|---|---|
| push, 12 file rows + icons, 51.7 KB | 1.8 ms |
| click to action collected | one poll interval |
| panel build (first layout) | web view host 832x805 |

Icons travel as raw BGRA in the same message -- the panel has no way to fetch
one. 32x32 is 4 KB per row, and per-extension caching means a page of results
usually needs two or three distinct icons, not twelve.

### Still open

- The apps column keeps its share of the width even when it has no rows; it
  should collapse and give the space to files.
- Keyboard navigation. Rows are buttons, so they tab, but there is no
  up/down-arrow behaviour and no default selection.
- The broker still takes its query from the command line. Reading it out of
  the search box is the one piece missing before this runs by itself.
