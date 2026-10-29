# Everything & Power Tools in the Start Menu

A native replacement for Windows 11 search, powered by voidtools Everything. Press Win+S, click the taskbar search icon, or start typing in Start to search files, apps, and settings in a separate window styled after your Start menu. Windows Search stays out of the way: its window is never shown, and it cannot start the Edge WebView2 process behind its Bing-backed search panel.

Version **2.1.1** separates installed-app management from ordinary search and improves opening-key handling for keyboard remappers. Version 2.1 introduced independent Apps-panel updates, Quick Tools cards and services management.

![Everything & Power Tools in the Start Menu](https://raw.githubusercontent.com/bardelyne/start-everything/main/img.png)

*Version 2.0: the separate search window with a folder preview.*

## Version 2.1.1 Changes

- Keeps ordinary search focused on launchable apps and Windows Settings, avoiding duplicate installed-only entries.
- Adds `/all` for managing desktop registrations and all current-user packages, including hidden components. Uninstall uses the exact product registration or package version, asks for confirmation, and respects protected components.
- Loads the first 20 inventory entries and appends 20 more near the bottom, preserving existing cards and your scroll position. Filters and the command name are customizable.
- Lets keyboard remappers process opening keys before search captures them, preserving their replacement keys and Unicode text. Automated remapper tests pass; testing with EPKL itself remains pending.

## Version 2.1 Changes

- Updates Apps independently of file searches, reuses result cards, and keeps existing files visible until replacements arrive.
- Warms app, Settings and installed-program catalogs in the background and refreshes them after registry, package or shortcut changes.
- Includes installed-only programs with friendly names, icons, launcher detection, installation-folder actions and confirmed uninstall actions.
- Adds configurable Quick Tools names, weather/forecast cards, currency and crypto conversions, a bounded connection test, live hardware graphs and service management.
- Keeps online cards off on a fresh install. After enabling them, weather, currency and crypto load automatically after a short typing pause, with cached results shown immediately. Speed tests require Run or Enter and have finite payload caps, reduced on metered connections.

## Version 2.0 Changes

- Adds 50 left-to-right interface languages, automatic Windows display-language detection, and a manual language override. Calculator and conversion input support language-independent symbols.
- Discovers searchable Settings names and keywords from Windows' local database and navigation metadata, cached in memory. Keeps additional Windows tools and falls back to the built-in list if native data is unavailable.
- Search runs in its own Windows 11 window, opened directly with Win+S, the taskbar search icon, an optional custom shortcut, or by typing in Start. Start itself still opens normally.
- Copies Start's measured size, alignment, background, rounded corners, and supported Start Menu Styler appearance. Root context menus and submenus receive their copied style before appearing.
- Adds app and custom-command suggestions for an empty query, shared favorite learning, configurable menu width and height, and settings organized into sections.
- Adds named custom commands with CMD, Windows PowerShell, PowerShell 7, or a custom shell; optional hidden terminals; and emoji, Windows glyph, image, icon, or executable icons.
- Adds automatic text and monochrome-icon contrast across the search window, with a fixed-color override. Background sampling pauses during motion and scrolling; unchanged samples reuse the cached color map.
- Fetches previews on a dedicated background thread, with built-in Windows image decoding, PDF rendering, and SVG rasterization. With a bottom taskbar, preview cards grow down while they fit search, then align at its bottom and grow upward when taller. Top and side taskbars keep previews top-aligned and growing downward. Crossing that height and changing selections mid-animation remain smooth.
- Keeps search input and selection when a context menu closes. Typing or Backspace closes the menu and continues the query; Escape closes the menu before clearing search.
- Defers search UI creation until first use, buffers opening text, waits for the editable IME context before replaying opening keys through Windows native input, and restores native and text-box focus after input handoffs.
- Supports top and side taskbars, with animations confined to the selected monitor. Right-side taskbars place the preview on the left.

## Key Features

- **Instant voidtools Everything IPC**: Queries Everything directly through its Win32 IPC interface for fast results across millions of files. The mod keeps no index of its own.
- **Smart Apps and Windows Settings Search**: Instant fuzzy matching across Desktop applications, Microsoft Store / UWP packages, Windows Settings, Control Panel applets, and Windows tools. Settings names and keywords come from Windows' own local search database and navigation metadata, cached in memory and refreshed with the app index when the Windows catalog changes. Additional Windows tools stay available, and an audited built-in list is used if native data is missing or incompatible. No Windows Search indexing service or extra database software is required. Shell icons are rendered at your display's exact pixel size.
- **Installed App Management**: Type `/all` for registered desktop programs and every current-user package, including frameworks, resources and hidden Windows components. This list comes directly from uninstall registrations and the package catalog, without AppsFolder launch entries. It never launches a program: click or press Enter to open its management menu. Right-click offers **Uninstall** when Windows permits it, with confirmation, plus **Open file location**, **Copy path** and **Copy name** where available. Protected components are read-only. Search within the list with `/all office`; browse all matches by scrolling; the list starts with 20 entries and appends 20 more near the bottom. Ordinary search excludes installed-only records, and registered metadata continues to provide aliases for real launchers. Missing launchers can still be recovered through a unique, verified GUI executable in the product folder.
- **Reusable Apps Cards**: App and Settings results reuse attached card slots as you type. Text, icons and menu targets update in place; unused slots collapse. Ordinary search stays bounded by **Max App Results**. The installed inventory uses the same reusable cards and grows only for its loaded batches. Rich Quick Tools cards keep their own layouts.
- **Silent App-list Updates**: Windows registry, Store/MSIX package and Start-menu shortcut notifications refresh the catalog after changes settle. Install/update/remove events bypass the installed-metadata cache. Reopening search keeps a safety refresh with a five-second minimum; otherwise unchanged installation metadata is reused for up to 30 seconds. A native background worker builds a replacement catalog and loads icons while app matching continues against the last complete snapshot. No network requests or periodic polling are needed for these listeners.
- **Background Catalog Warmup**: After the taskbar is available, app, Settings and installed-program metadata load on a background worker. First search reuses that catalog. The search window and its XAML UI remain deferred until needed.
- **Independent App Results**: App results update without waiting for Everything's file search. Cached icons appear immediately; missing icons fill in afterward. File results arrive separately and preserve the app selection.
- **Windows Services**: Use `/services` to find services by name or description and manage them from the right-click menu.
- **Online Utility Cards**: Use `/weather` for your approximate local weather, `/money 100 USD > EUR` for currency conversion, `/crypto BTC` for a crypto spot price, or `/speedtest` for a connection test with a circular speed meter. Data loads separately from local search. See [Online Cards](#online-cards).
- **Learns Your Favorites**: Apps and custom commands you open from here more often move up among results that match equally well. A clearly better match always stays on top. Can be turned off in the settings, which also forgets what was learned.
- **Separate Animated Search Window**: Search follows Start's size, taskbar alignment, colors, and rounded corners, with opening and closing animations that follow the taskbar edge (bottom, top, left, or right) and respect Windows' animation setting. It stays out of the taskbar and Alt+Tab. Typing in Start switches immediately without an opening animation and carries the opening text into search. The search bar uses Windows' own search icon.
- **Windows Search Out of the Way**: `SearchHost.exe` keeps running for the shell, but its window is never shown and it cannot launch Edge WebView2, the web view behind its Bing-backed search panel.
- **Search Entry Points**: Open search directly with Win+S, the taskbar search icon, Start’s search bar, or an optional custom shortcut. Pressing Win or clicking Start opens Start normally; begin typing to switch into search.
- **Suggested Apps**: An empty search shows apps and custom commands together, ordered by learned usage when enabled and alphabetically when learning is off. Escape clears the query to suggestions; press it again to close search.
- **Custom Commands**: Add named commands to Apps, choose CMD, Windows PowerShell, PowerShell 7, or a custom shell, and optionally hide the terminal. Icons accept emoji, pasted Windows icon glyphs, Unicode codes, and local image, icon, or app paths. Commands also appear in empty-search suggestions.
- **Optional Files Panel**: Hide the Files panel and pause Everything file queries. With automatic sizing, search becomes half as wide. Apps, settings, custom commands, and utilities remain available.
- **Custom Menu Size**: Set your own menu width and height, or turn the toggle off to follow Start's size. Dimensions follow Windows display scaling and stay within the screen's available area.
- **Inline Calculator**: Type a sum (e.g. `100 * 5`, `sqrt(144)`, `15% * 200`, `2^10`) and the result shows above the apps; large and small results show in scientific notation too (`= 99000`, `= 9.9e4`). Press Enter to copy it.
- **Unit Converter**: Type `100 km` to see it in the usual units of its kind, all on one line first, or `100 km → mi` (also `in`, `->`, `=`) for just one; `255 hex` or `0xFF` for other bases. It knows length, mass, temperature, speed, area, volume, data, data rate, time, pressure, energy, power, angle and frequency, with `.` or `,` as the decimal point, and each kind has its color. A number alone, or one in a name (`2024 report`, `3d`), is left to the search. Add units it does not know in the settings, and have them convert back too (Reciprocal).
- **Network Interface Inspector**: Type `/ip` to display all active Wi-Fi, Ethernet, and VPN network interfaces with their IP addresses, subnet masks, gateways, and hardware descriptions. Press Enter to copy the IP.
- **Live Hardware Cards**: Type `/usage` for CPU, memory, each GPU, disk activity and network traffic, with colored graphs covering the last 60 seconds. Readings update once a second while the command is visible and stay entirely on your PC.
- **Full Right-Click Context Menu**: Right-click any file, folder, or application to Open, Run as Administrator, Open in terminal, Properties, Create desktop shortcut, Cut/Copy (files), Copy name (files/folders), Copy path, Open file location, or Uninstall (apps). Escape closes the menu; typing closes it and continues the query.
- **Native Properties Dialogs**: Properties opens through `explorer.exe`, the same dialog as in File Explorer.
- **File Preview**: Images, muted media, PDF pages, SVGs, text/document excerpts, and file properties beside search. The card adapts its alignment and resize direction to the search window’s height. See [File Preview](#file-preview) for formats, copying, and folder sizes.
- **Drag and Drop**: Drag a file or folder into File Explorer, the desktop, or another app. It is always a copy, never a move. Previews stay hidden during the drag. A successful drop closes search; Escape cancels.
- **Uninstall**: Right-click an app and choose Uninstall. After a confirmation that names what goes, a Store app is removed as Start removes it, and a program runs its own uninstaller, the one Installed apps in Settings runs. Protected Windows components offer no uninstall action. A Store app is removed in the background, with no progress bar: it is gone from the list after a few seconds.
- **Opens in Front**: Programs are started by Explorer, the way the stock Start menu starts them, so they come to the front even when they take a while to start or you move the mouse meanwhile.
- **Explicit Web Search**: Trigger web searches on demand using the `?` prefix (e.g. `?query`). Includes customizable keyword shortcuts such as `?yt` (YouTube), `?gh` (GitHub), `?w` (Wikipedia), and `?r` (Reddit).
- **Start Menu Styler Compatibility**: Reads Start's rendered `AcrylicBorder`, search box, headers, footer, and opened context menus from its live visual tree using public XAML properties. Supported custom glass brushes also copy their actual composition effect definitions, including tint alpha, blur, saturation, and luminosity, without theme presets. Unchanged materials reuse bounded caches of effect definitions without keeping Start's composition objects alive. The last captured effect definitions are saved in this mod's own Windhawk storage on unload, separated by Windows session and restricted to the current Start process and matching live brush identities. This preserves already captured glass across updates and off/on cycles without retaining GPU resources or reconnecting another mod's brushes. New live captures replace saved definitions. Automatic text/icon contrast or a fixed text color keeps labels readable; see [Automatic Text and Icon Contrast](#automatic-text-and-icon-contrast).
- **Keyboard Input**: Opening text and context-menu typing carry into search. Chinese, Japanese, and Korean opening keys use Windows' native input path once the search box is ready for IME composition. Keyboard remappers such as EPKL/AutoHotkey can process opening keys first; keys they consume are not translated a second time. Focus uses standard foreground APIs without attaching thread input.
- **Accidental Open Prevention**: Enter only triggers actions when an item is selected in the active panel, preventing unintended opening of files.

## Settings Sections

The mod's Settings page groups related options together:

| Section | Options |
| :--- | :--- |
| **Appearance** | Display language, text size and color, keyboard shortcut hints, and custom menu dimensions |
| **Apps & Search** | App result limit, favorite learning, search delay, and an optional keyboard shortcut |
| **Files & Preview** | Files panel, file result limit, previews, animation, and excluded paths |
| **Quick Tools** | Localized command names, past/upcoming weather days, extra locations, and saved currency/crypto cards |
| **Online Cards** | Offline mode, automatic or manual weather location, temperature units, and default quote currency |
| **Web Search** | Default search engine and keyword shortcuts |
| **Custom Commands** | Named commands, icons, shells, and terminal visibility |
| **Custom Unit Conversions** | Units, formulas, categories, and reciprocal conversion |

## Interface Language

Open **Settings → Appearance → Display Language** and choose **Auto** or one of the 50 included left-to-right language options. Auto follows the Windows display language; unsupported languages use English. The search bar, headings, hints, context menus, preview labels and utility messages are translated. The layout remains left-to-right.

Languages: Afrikaans, Albanian, Armenian, Bengali, Bosnian, Bulgarian, Catalan, Chinese (Simplified and Traditional), Croatian, Czech, Danish, Dutch, English, Estonian, Filipino, Finnish, French, Georgian, German, Greek, Hindi, Hungarian, Icelandic, Indonesian, Italian, Japanese, Korean, Latvian, Lithuanian, Macedonian, Malay, Norwegian Bokmål, Polish, Portuguese (Brazil and Portugal), Romanian, Russian, Serbian (Latin), Slovak, Slovenian, Spanish, Swahili, Swedish, Tamil, Telugu, Thai, Turkish, Ukrainian and Vietnamese.

Translations are bundled and work offline. Changing this option does not change the keyboard layout or Windows regional formats. Windows-provided application and Settings names retain their native Windows language. Windhawk settings descriptions and the documentation remain in English.

Use symbols for calculations and conversions:

| Input | Result |
| :--- | :--- |
| `10% * 7` or `10% × 7` | `0.7` |
| `7 * 10%` | `0.7` |
| `200 * (1 + 10%)` | `220` |
| `8 ÷ 2`, `5 − 2`, `√(144)` | `4`, `3`, `12` |
| `2 * π` | About `6.283185` |
| `1,5 * 2` | `3` |
| `100 km → mi` or `100 km = mi` | About `62.137119 mi` |

A postfix `%` divides by 100. Binary remainder remains available as `10 % 3` (result `1`); use parentheses for a signed divisor, such as `10 % (-3)`. Existing English expressions such as `15% of 200`, function names such as `sqrt`, and `to` conversions still work. Both `.` and `,` mean a decimal point; thousands separators are not supported. Unit symbols and mathematical notation do not change with the interface language.

## Custom Commands

In Windhawk, open the mod's **Settings → Custom Commands** and add an entry:

- **Name**: Display name and search term.
- **Icon**: Paste an emoji or a Windows icon glyph, enter a Unicode code such as `E713` (Settings) or `E7E8` (Power), or provide a full path to an image, `.ico`, or executable. `U+E713`, `0xE713`, `\uE713`, and `&#xE713;` also work. Environment variables in paths are expanded. Leave blank for the terminal icon.
- **Shell**: Choose **Command Prompt (CMD)**, **Windows PowerShell**, **PowerShell 7**, or **Custom shell**. Existing commands default to CMD. PowerShell 7 must be installed separately; it is found in its standard installation folder or through PATH.
- **Shell Executable**: For Custom shell, enter its full executable path or a name available in PATH, such as `nu.exe` or `wsl.exe`. Environment variables such as `%ProgramFiles%` are supported.
- **Shell Arguments**: For Custom shell, put `{command}` once as a separate argument where the shell expects the command. The mod quotes it as one Windows argument, preserving quotes, spaces, Unicode and operators inside your code. Do not add quotes around `{command}`. Environment variables in this arguments template are expanded before inserting the command.
- **Hide Terminal**: Run without showing the shell console (default off). Apps opened by the command can still show their own windows, and Run as administrator can still display Windows' permission prompt.
- **Command**: Enter the command for the selected shell. The mod does not guess the shell from the command text. Blank names or commands are ignored. If the selected shell is missing or the Custom shell arguments are invalid, search stays open and shows the reason.

Commands appear alongside installed apps in search results and empty-search suggestions, within **Max App Results**. They share the favorite system: frequently launched entries move up, and equal-use suggestions sort alphabetically. Turning off **Learn Favorite Apps** stops learning and clears history for both apps and commands. Right-click for **Run**, **Run as administrator**, or **Copy command**. Settings changes apply without reopening search. Up to 100 entries are supported.

A name may start with `/`, such as `/backup`. Single-word custom `/commands` share names with Quick Tools and their active compatibility aliases. If the same name belongs to two entries, typing it shows a red conflict card instead of running either one. Names without `/` remain ordinary search entries. A slash name does not add argument forwarding: the saved script runs exactly as configured.

For example, name a CMD command `Downloads`, use `📂` as its icon, and set its command to `explorer.exe "%USERPROFILE%\Downloads"`. Browse Microsoft's [Segoe Fluent Icons list](https://learn.microsoft.com/en-us/windows/apps/design/iconography/segoe-fluent-icons-font) for Windows glyphs and their codes.

Custom shell examples (the shell must already be installed):

| Shell | Shell Executable | Shell Arguments |
| :--- | :--- | :--- |
| Git Bash | `%ProgramFiles%\Git\bin\bash.exe` | `-c {command}` |
| Nushell | `nu.exe` | `-c {command}` |
| Bash in WSL | `wsl.exe` | `--exec bash -lc {command}` |

Use a full executable path if it is installed somewhere else. WSL requires an installed Linux distribution. PowerShell 7 installation options are described in [Microsoft’s documentation](https://learn.microsoft.com/en-us/powershell/scripting/install/install-powershell-on-windows).

## Quick Tools

Open **Settings → Quick Tools** to rename the weather, currency, crypto, IP, hardware-usage, services and installed-inventory commands. Enter the word after `/`, in your own language. A leading slash is optional in the setting. For example, setting Weather Command Name to `météo` makes `/météo` and `/météo London` work. The layout remains left-to-right, but command names can contain non-Latin letters.

Names are case-insensitive, cannot contain spaces, and must be distinct. Blank or invalid names use their defaults. If two commands share a name, typing that name shows a red warning card; give them separate names in settings to continue. English compatibility aliases remain only while their primary command name is unchanged. The examples below use the default names.

- **Weather:** choose 0–7 past days and 0–7 upcoming days. The default is two past days, Today, and five upcoming days, arranged chronologically inside one card. Today uses Windows' accent color and follows the city's local date; past days are subdued. Add up to eight **Extra Weather Locations** for separate cards when you enter the command alone. An explicit city argument shows just that city. The primary location and temperature units remain under **Online Cards**.
- **Currency:** **Saved Currency Cards** can contain a code such as `EUR`, or a conversion such as `100 USD > EUR`. A code alone means one unit converted into the default quote currency. Entering the currency command alone displays the saved list; adding arguments performs that one conversion instead.
- **Crypto:** **Saved Crypto Cards** accepts symbols such as `BTC`, or conversions such as `0.5 ETH > EUR`. Entering the crypto command alone displays the list. Currency and crypto lists support up to twelve entries each; blank lists restore their default examples. Repeated equivalent entries are combined.
- **IP:** the renamed IP command still displays local network interfaces and addresses. It works offline and does not contact the approximate-location service.
- **Hardware usage:** `/usage` displays local resource cards. The command name can be changed here too. See [Live Hardware Usage](#live-hardware-usage).
- **Speed test:** opens a ready card; press **Run** or Enter to start. The command name defaults to `speedtest`, with `/speed` as an alias while that name is unchanged.

Saved cards fill in individually as their requests complete. Conversions sharing a pair reuse one downloaded rate, and a failed entry does not prevent the other cards from appearing. The selected card stays selected as other cards finish loading.

## Online Cards

Weather, exchange rates, crypto prices and internet speed tests appear as cards in the Apps panel. **Online Cards → Offline Mode is on by default on a fresh install.** Turn it off to opt in. Weather, currency and crypto load automatically after a 450 ms typing pause; cached results appear immediately. No Enter press is needed. A speed test requires Run or Enter. Ordinary app/file search, calculations, unit conversions, IP, usage and services stay local. Existing saved settings are preserved.

| Type in search | Result |
| :--- | :--- |
| `/weather` | Weather for the primary location plus saved extra locations, with history and forecasts inside each card. |
| `/weather London` | Weather for a city you choose. A country qualifier such as `/weather Paris, France` can help distinguish places. |
| `/money 100 USD > EUR` | Convert an amount using the latest available currency reference rate. |
| `/money` | Display the saved currency cards. |
| `/money USD IQD` | Show the rate for one US dollar in Iraqi dinars. |
| `/crypto BTC` | Show one bitcoin in the default quote currency, initially USD. |
| `/crypto` | Display the saved crypto cards. |
| `/crypto 0.5 ETH > EUR` | Convert a crypto amount using an online spot price. |
| `/speedtest` or `/speed` | Open the connection-test card, then press Run or Enter. |

With the default names, `/currency` and `/fx` are aliases for `/money`; `/price` is an alias for `/crypto`. The amount defaults to 1. Target currencies can use `>`, `→`, `/`, `to`, or `in`, or simply a space between codes. Currency commands use three-letter codes such as USD, EUR, GBP, JPY and IQD. Crypto commands accept supported ticker symbols, including crypto quote currencies; Bitcoin, Ethereum and Dogecoin also work as names. Invalid arguments show examples without making a request, using your configured command name.

**Weather:** [Open-Meteo](https://open-meteo.com/) supplies the temperature, daily low/high, humidity and wind speed. City searches use its [GeoNames-based geocoding service](https://open-meteo.com/en/docs/geocoding-api). Automatic location uses [ipwho.is](https://ipwhois.io/documentation) and is marked `≈` because it is approximate and can follow a VPN's exit location. Set **Online Cards → Weather Location** to a city, or include a city in the command, to avoid the IP lookup. Temperature can follow Windows' regional measurement settings or use Celsius/Fahrenheit; wind speed is in km/h. Weather timestamps use the city's local time.

**Currency:** [Frankfurter](https://frankfurter.dev/) supplies published reference rates. The date on the card is the provider's rate date. These are reference rates, rather than a bank's retail buy/sell quote, and may not change on weekends or holidays.

**Crypto:** [Coinbase's public price API](https://docs.cdp.coinbase.com/coinbase-app/track-apis/prices) supplies spot prices for supported pairs. Cards show when the price was fetched, in UTC. Supported assets and quote currencies depend on the provider. Prices are snapshots, not a streaming market feed.

**Internet speed:** the 270-degree speedometer has a cyan-to-mint download arc, a purple upload arc, a moving needle, labeled ticks and a large Mbps reading with a second reading in kB/s, MB/s or GB/s, selected automatically. Both final results also show their byte-per-second equivalents (8 bits = 1 byte). The arc, needle and central reading animate together between samples and respect Windows animation settings. The scale starts at 0–1000 Mbps, expands for faster connections, and is shared by download and upload. Latency and both results stay visible above the dial. Stop and Run again are available; changing the query or closing search cancels the active test. An incomplete test never replaces the last completed result.

The native client selects a responsive node from a small set of [public LibreSpeed servers](https://librespeed.org/backend-servers/servers.php) and reuses that selection for an hour. On unrestricted connections it uses five download streams and three upload streams with [LibreSpeed-style adaptive timing](https://github.com/librespeed/speedtest/blob/master/speedtest_worker.js), warmup/grace periods and the default 6% transport-overhead estimate. Faster tests shorten automatically instead of always waiting 15 seconds per direction. Upload bodies grow with the connection and shrink if a server rejects them. Rejected bodies still count against the run's allowance. Request starts are paced, server Retry-After instructions are honored, and Run during cooldown sends no traffic. Downloads stop at the measurement cutoff; already-started uploads get a bounded acknowledgment drain, and only acknowledged bytes within the measured interval contribute to the result.

**Each run has a test-payload cap of 256 MiB download plus 128 MiB upload.** Metered or unknown-cost connections use smaller caps of 64 MiB down and 32 MiB up, with four streams and no grace-period traffic. Headers, small control responses and network-protocol overhead are additional. Capped runs show **Test data limit reached** and can be less precise, especially on fast connections. No extra app, account or API key is needed. No personal files, IP/ISP lookup requests, telemetry or measurement reports are sent. Server capacity, routing, other traffic and VPNs affect results. The last completed result stays in memory until the mod unloads; reopening the command does not start another transfer.

Press Enter or click a completed card to copy the converted numeric value; a weather card copies its displayed weather details, and a speed-test card copies its measurements. Weather and money cards also support right-click copying. Help, loading and error cards do not launch anything.

Results are cached in memory: weather for 10 minutes, currency pairs for an hour, and crypto pairs for 30 seconds. Location lookups are reused for six hours. Changing only the amount reuses a cached pair and calculates locally. Old data is marked **Cached** while a refresh is needed or has failed. Failed requests back off for a minute. Providers are contacted after a 450 ms typing pause, on a separate worker, so they do not delay app results or file results. Further typing replaces pending lookups and prevents old responses from replacing the current results. There is no periodic background polling.

**Privacy and connectivity:** providers see your public IP as part of the connection. For weather, the location service returns an approximate city and Open-Meteo receives the requested city or coordinates. A city name paused on while typing can be sent as a location lookup. Currency and crypto providers receive only the requested pair, without the amount. No normal search text, account credentials, cookies, or telemetry are sent. Automatic location and downloaded results are not saved to disk. **Online Cards → Offline Mode** is enabled on a fresh install; turning it off opts in to these features. Turning it on disables provider requests. Public services can be unavailable or rate limited, and the free Open-Meteo service is intended for noncommercial use. Weather data is credited to Open-Meteo and location names to GeoNames.

## Installed programs and packages

Use `/all` to manage all named desktop uninstall registrations and the packages registered for the current Windows user. This includes frameworks, resource packages, bundles and hidden system components. It does not enumerate other users or add launch shortcuts from `shell:AppsFolder`.

Add a name, publisher or package identity to filter, for example `/all office` or `/all Microsoft.VCLibs`. Results are alphabetical when unfiltered. The list starts with 20 products and appends up to 20 more as you scroll near the bottom. Existing cards and the scroll position stay in place. A tall viewport can request another batch to fill its available space. There are no navigation pages, and this list is independent of **Max App Results**. Changing the filter returns to the first 20 entries and scrolls to the top. **Quick Tools → Installed Inventory Command Name** renames `/all`; the renamed command takes the same optional search terms.

Package rows identify main, framework, resource, bundle and optional packages, with resource identifiers such as `split.scale-150` or `split.language-de` when Windows provides them. Publisher, version and architecture distinguish installations further. These labels do not change which components Windows allows you to uninstall. Technical package identities are searchable only in this management list. If `/all` is already used by a custom command or another Quick Tool, rename either command in settings; the conflict warning prevents either action from running accidentally.

Clicking a product or pressing Enter opens its management menu. There is no app **Open**, **Run** or administrator-launch action in this list. **Uninstall** asks for confirmation and uses the exact registry registration or listed package version. Protected components have no uninstall action. Available installation folders can be opened or copied, and product names can be copied. The list uses the existing background catalog and change listeners; searching it performs no file search, network request or fresh inventory scan.

## Windows Services

Type `/services` to list local Windows services, or search with `/services audio`, `/services spooler` or words from a service description. `/services running` and `/services stopped` filter by state; the translated state labels work too. The command name is customizable under **Quick Tools → Services Command Name**. It participates in duplicate-name detection and works in Offline Mode.

Each result shows the display name, internal name, current state, process ID when running, and description when available. Results update once per second while visible. Search uses cached entries immediately; service enumeration runs separately so it cannot delay ordinary app or file searches. Changing the query or closing search stops status polling.

Right-click a result for **Start**, **Stop**, **Restart**, **Copy name**, **Copy details**, or **Open Services** for the full Windows management console. Unavailable operations are disabled based on the latest state and the controls the service accepts. Only one operation runs at a time, and Windows errors are shown on the affected card. Clicking a service result or pressing Enter copies its details.

Operations use Windows’ service controller through a short-lived hidden Windows PowerShell helper. Administrator approval is requested only if Windows denies the selected operation. Service names are encoded as data rather than inserted into executable instructions. Restart waits for Stop to finish before starting again; dependent services are never forcibly stopped. Disabled services are not enabled automatically, and startup types are not changed. An operation already requested can finish after search closes. No service information leaves the PC.

## Live Hardware Usage

Enter `/usage` to show a separate card for **CPU**, **RAM**, **each GPU**, **disk activity** and **network traffic**. Each card has a colored graph with up to 60 seconds of history. Values update every second without rebuilding the controls or moving selection. Clicking a measured card copies its current reading.

- CPU shows average processor activity; RAM shows used physical memory and its total.
- GPU cards use Windows performance counters. Activity is combined across processes on each engine, then the busiest engine represents the adapter, following Task Manager's approach. Each detected GPU has its own graph.
- Disk activity shows the aggregate physical-disk active percentage, plus read/write throughput.
- Network traffic sums active physical Ethernet and Wi-Fi adapters, with download/upload rates. Virtual and tunnel interfaces are excluded to avoid counting VPN traffic twice. This observes existing traffic and does not generate a network test.

Collection runs on a separate worker, only while this command is open. Closing search or changing the query stops it and releases performance-counter resources. Reopening starts fresh history. Unavailable counters are shown as unavailable, and missing readings leave gaps rather than false zeroes. Nothing is uploaded or saved to disk, and this command works in Offline Mode. Cards use monochrome Windows icons. GPU cards also show **VRAM used / total** for dedicated GPU memory, updated every second; unavailable counters show a dash. GPU support depends on the installed Windows display driver and counters.

## File Preview

Enable **Settings → Files & Preview → File Preview** to show a card beside the selected file or folder. The card includes its name, type, size, dates, and relevant media or document properties.

With a **bottom taskbar**, the preview uses the available screen space independently of the search window's height:

- **Preview fits inside search's height:** align at the top and resize downward.
- **Preview is taller than search:** align its bottom with search and resize upward, including above the search window.
- **Preview crosses search's height:** switch alignment continuously during the same resize animation, without growing down and jumping up afterward. Rapid selection changes continue from the current size.

The card stays within the monitor's usable area where its height permits. If upward growth reaches the screen's top edge, it uses the available space below.

With a **top, left, or right taskbar**, the preview always aligns with the top of search and resizes downward. A **right taskbar** places the preview on the **left** of search.

| Content | Preview behavior |
| :--- | :--- |
| **PNG, JPEG, BMP, GIF, TIFF** | Shell thumbnails, with Windows' built-in image decoder as a fallback. IrfanView or another image viewer is not required. |
| **SVG** | Rendered using Windows' SVG support. |
| **Video, animated GIF and WebP** | Muted looping playback when **Play Videos and Animations** is enabled. Video format support depends on the codecs available in Windows. |
| **PDF** | Rendered with Windows' PDF engine, without a separate PDF app. With animation enabled, cycle through up to the first ten pages, one per second. |
| **Text and code** | The beginning of the file, with selectable and copyable text. |
| **Word, Excel, PowerPoint, RTF and OpenDocument** | A text excerpt when a supported Windows or Office document filter is available. Text is extracted without document page layout. |
| **Folders** | A folder icon, item counts, and the indexed folder size when enabled in Everything. |
| **PSD and other specialist formats** | A shell thumbnail when a suitable thumbnail provider is installed; otherwise a file icon and available details. Installing an app does not necessarily install a thumbnail provider. |

Click the name, an individual detail value, or the full path to copy it. Copy targets follow the text area, and copying keeps the displayed path unchanged. Select and copy text directly from text/document excerpts. Previews stay hidden throughout drag and drop.

Preview extraction runs on a dedicated background thread. Videos use Windows’ native media presenter, muted and looping. No separate helper process or preview application is required. Animated-image and PDF-frame caches are bounded to protect memory use; hiding the preview stops playback and cancels pending page rendering.

Turn off **Play Videos and Animations** to keep previews still, or turn off **File Preview** to hide the card. Hiding the Files panel also hides its previews and stops the mod's Everything file queries; the Everything application itself keeps running.

## Automatic Text and Icon Contrast

Leave **Settings → Appearance → Text Color** set to **auto** to adapt text and monochrome interface icons to their backgrounds. App and file icons keep their original colors. Enter a color such as **#202020** or **#FFFFFF** to use one fixed text color instead. Windows high contrast takes precedence.

One shared background map covers the search bar, both result panels, headings, and footer. Labels use strong local light or dark tones, and subtitles and hints stay opaque for readability. The map stays aligned while either panel scrolls and is reused as results change. Separate preview and context-menu popups adapt to their own backgrounds.

Background samples refresh up to four times per second while stationary and pause during opening, closing, scrolling, and drag and drop. Unchanged samples reuse the existing map. The menu continues to follow supported Start Menu Styler backgrounds; arbitrary custom effects may not be reproduced exactly.

## Folder Sizes in Preview

To show folder sizes, open **Everything → Tools → Options → Indexes**, enable **Index folder size**, and click **OK**. Let Everything finish updating its index, then select a folder with File Preview enabled. Folder sizes are read from Everything's index; the mod does not calculate them by scanning folders. Excluded files are not included. Without this option, previews can show item counts but no indexed folder size. See the [Everything documentation](https://www.voidtools.com/en-us/support/everything/indexes/).

## Requirements

1. **Windows 11 (x86-64)**: Tested on **25H2 and 26H2**. Other Windows versions and builds, including Insider builds, have not been verified.
2. **voidtools Everything**: **Everything 1.4** or **Everything 1.5**, including alpha/beta and named instances. Keep the application running in the tray; the service alone is not enough. Download from [voidtools](https://www.voidtools.com/).
3. **Windhawk**: Install [Windhawk](https://windhawk.net/) and load the mod.

## Taskbar Search

**Win+S**, the **taskbar search icon**, and **Start’s search bar** open search directly. Pressing **Win** or clicking the **Start button** opens Start normally; begin typing there to switch into this search. The full taskbar search box is not supported, so set Search to **Search icon only** or **Hide**:

1. Right-click an empty area on the Taskbar and select **Taskbar settings** (or navigate to **Settings** > **Personalization** > **Taskbar**).
2. Under **Taskbar items**, set **Search** to **Search icon only** or **Hide**.

## Keyboard Shortcuts and Navigation

To add another way to open search, go to **Settings → Apps & Search**, turn on **Enable Alternative Shortcut**, then choose **Shortcut Modifiers** and **Shortcut Key**. The default is **Alt+Space**, with the feature off until enabled. You can combine Ctrl, Alt, Shift, and Windows, or choose None for function, media, browser, or special keys (unmodified typing keys are refused so normal typing cannot be blocked). The key list includes letters, digits, function, navigation, numpad, punctuation/OEM, browser, and media keys. F12 is reserved by Windows, and Fn/hardware-only keys cannot be registered. Punctuation labels use a US keyboard layout. If the combination belongs to Windows or another app, choose another; registration failures are recorded in the mod log. Changes apply immediately, and Win+S remains available.

| Shortcut / Input | Action |
| :--- | :--- |
| **Win+S / Taskbar search icon** | Open the search window directly |
| **Alternative shortcut (optional)** | Open and focus search using your chosen modifier and key combination |
| **Type in Start** | Switch immediately and carry the opening text into search |
| **Click Start's search bar** | Open and focus the search window |
| **Up / Down Arrow Keys** | Navigate selection through applications, utility cards, and files |
| **Tab / Shift + Tab** | Move to the next / previous result, through the apps and on into the files |
| **Left / Right Arrow Keys** | Switch between the Apps and Files columns (Right only with the cursor at the end of the query, so the arrows still move the cursor while you edit) |
| **Enter** | Launch selected application, copy calculation/conversion/IP result, or open item. Pressed before the results for what you typed are in, it opens the first one as soon as they arrive |
| **Ctrl + Enter** | Run selected application or file as Administrator (triggers UAC) |
| **Shift + Enter** | Open the selected result's context menu (same as right-click); navigate it with the arrow keys and Enter |
| **Escape** | Close an open context menu first; otherwise clear the query to suggestions, or close search when already empty |
| **Type while a context menu is open** | Close the menu and continue editing the query |
| **Right-Click** | Open the result's context menu |

Note on Pinning: Windows 11 blocks programmatic pinning to the Taskbar or Start Menu. Use Create desktop shortcut first, then right-click the shortcut on your desktop and select Pin to Taskbar or Pin to Start.

## Quick Tools and Utilities

| Command | Description | Example | Enter Action |
| :--- | :--- | :--- | :--- |
| `<sum>` | Calculation | `100 * 5`<br>`√(144)`<br>`15% × 200` | Copies the result |
| `<number> hex`, `0xFF`, `0b1010` | The number in other bases (digits grouped: `0b111 1111 1111`) | `2047 hex`<br>`0x7FF` | Copies the hex value |
| `<number> <unit>` | The number in the usual units of its kind, all on one line first | `100 km`<br>`32 f`<br>`1,5 kg` | Copies the selected one |
| `<number> <unit> → <unit>` | Just that conversion (also `to`, `in`, `->`, `=`) | `100 km → mi`<br>`100 mbps → MB/s` | Copies the result |
| `/ip` | Lists all active Wi-Fi, Ethernet, and VPN adapters with IP, subnet, gateway, and adapter name | `/ip` | Copies selected IP address to clipboard |
| `/speedtest` | Measures HTTPS latency and approximate download/upload speed | `/speedtest` or `/speed` | Starts a ready test, or copies a completed result |
| `?<query>` | Searches the web using the default search engine URL | `?weather today` | Opens search in default web browser |
| `?<shortcut> <query>` | Searches specific service using configured shortcut keyword | `?yt lofi hip hop`<br>`?gh windhawk` | Opens query on target service |

## Troubleshooting and Compatibility

- **Everything is reported as not running:** start the Everything application and leave it running in the tray. Its service alone does not provide the search IPC window. Everything 1.4 and 1.5 alpha/beta, including named instances, are supported.
- **A preview shows only an icon:** enable File Preview. Common image formats have a built-in decoding fallback; specialist formats may require a thumbnail provider, and videos may require a compatible Windows codec. Unavailable or unreadable content falls back to its icon and available properties.
- **Folder sizes are missing:** enable Everything's **Index folder size** option; see [Folder Sizes in Preview](#folder-sizes-in-preview). The mod does not recursively scan folders to calculate sizes.
- **Start Menu Styler appearance is not copied yet:** open Start so its layout and live visual tree can be measured (and open a context menu in Start once so its live menu presenter can be captured). Supported static glass effects are captured when Start creates them. If a custom style predates the very first capture, reapply the current Start Menu Styler theme once or restart Start, then open Start. Once captured, matching materials are restored automatically after updates or off/on cycles in that Start process. A Start restart discards old recovery data and captures the new brushes normally. Changes made in place to an opaque custom brush while this mod is disabled cannot always be observed; reapply that theme if its appearance differs. Capture is passive: the mod does not reconnect or rebuild brushes owned by Start or another mod. Live image/visual sources, animated custom effects, and unsupported effects use an approximation; their appearance may differ.
- **A custom command does not start:** choose the shell that understands the command. PowerShell 7 and custom shells must already be installed. Hide Terminal hides the shell console; it does not shorten the command's lifetime or hide windows opened by the command.

Tested on **Windows 11 25H2 and 26H2, x86-64**. Other Windows versions and builds, including Insider builds, have not been verified. For a reproducible problem, include the full Windows build, mod version, Everything version and instance name, other Start/search mods, and steps to reproduce it.

## Upgrading from 1.x

Version 2.0 organizes settings into collapsible sections. Windhawk does not transfer preferences from the old flat setting names to these sections automatically. Before upgrading from 1.x, save your settings or note your choices, then reapply them in the new sections after updating. This includes web search shortcuts and excluded path patterns. Custom commands and custom unit conversions keep their existing setting names.
