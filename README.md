# Everything & Power Tools in the Start Menu

A native replacement for Windows 11 search, powered by voidtools Everything. Press Win+S, click the taskbar search icon, or start typing in Start to search files, apps, and settings in a separate window styled after your Start menu. Windows Search stays out of the way: its window is never shown, and it cannot start the Edge WebView2 process behind its Bing-backed search panel.

Version **2.0** introduces a separate Windows 11 search window, with Start appearance copying, app suggestions, and custom commands.

![Everything & Power Tools in the Start Menu](https://raw.githubusercontent.com/bardelyne/start-everything/main/img.png)

*Version 2.0: the separate search window with a folder preview.*

## Version 2.0 Changes

- Adds 50 left-to-right interface languages, automatic Windows display-language detection, and a manual language override. Calculator and conversion input support language-independent symbols.
- Restores compatible captured Start backgrounds after updates and off/on cycles using this mod's Windhawk cache, including while Start is hidden.
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
- **Full Right-Click Context Menu**: Right-click any file, folder, or application to Open, Run as Administrator, Open in terminal, Properties, Create desktop shortcut, Cut/Copy (files), Copy name (files/folders), Copy path, Open file location, or Uninstall (apps). Escape closes the menu; typing closes it and continues the query.
- **Native Properties Dialogs**: Properties opens through `explorer.exe`, the same dialog as in File Explorer.
- **File Preview**: Images, muted media, PDF pages, SVGs, text/document excerpts, and file properties beside search. The card adapts its alignment and resize direction to the search window’s height. See [File Preview](#file-preview) for formats, copying, and folder sizes.
- **Drag and Drop**: Drag a file or folder into File Explorer, the desktop, or another app. It is always a copy, never a move. Previews stay hidden during the drag. A successful drop closes search; Escape cancels.
- **Uninstall**: Right-click an app and choose Uninstall. After a confirmation that names what goes, a Store app is removed as Start removes it, and a program runs its own uninstaller, the one Installed apps in Settings runs. Protected Windows components offer no uninstall action. A Store app is removed in the background, with no progress bar: it is gone from the list after a few seconds.
- **Opens in Front**: Programs are started by Explorer, the way the stock Start menu starts them, so they come to the front even when they take a while to start or you move the mouse meanwhile.
- **Explicit Web Search**: Trigger web searches on demand using the `?` prefix (e.g. `?query`). Includes customizable keyword shortcuts such as `?yt` (YouTube), `?gh` (GitHub), `?w` (Wikipedia), and `?r` (Reddit).
- **Start Menu Styler Compatibility**: Reads Start's rendered `AcrylicBorder`, search box, headers, footer, and opened context menus from its live visual tree using public XAML properties. Supported custom glass brushes also copy their actual composition effect definitions, including tint alpha, blur, saturation, and luminosity, without theme presets. Unchanged materials reuse bounded caches of effect definitions without keeping Start's composition objects alive. Last captured definitions are saved in this mod's Windhawk storage on unload and used as a compatible fallback after a reload, including while Start is hidden. Recovery storage is separated by Windows session and applies only to the current Start process; it is discarded when Start restarts. Disabling the mod releases its hooks and live caches; its recovery data stays in its own Windhawk storage. Automatic text/icon contrast or a fixed text color keeps labels readable; see [Automatic Text and Icon Contrast](#automatic-text-and-icon-contrast).
- **Keyboard Input**: Opening text and context-menu typing carry into search. Chinese, Japanese, and Korean opening keys use Windows' native input path once the search box is ready for IME composition. Focus uses standard foreground APIs without attaching thread input.
- **Accidental Open Prevention**: Enter only triggers actions when an item is selected in the active panel, preventing unintended opening of files.

## Settings Sections

The mod's Settings page groups related options together:

| Section | Options |
| :--- | :--- |
| **Appearance** | Display language, text size and color, keyboard shortcut hints, and custom menu dimensions |
| **Apps & Search** | App result limit, favorite learning, search delay, and an optional keyboard shortcut |
| **Files & Preview** | Files panel, file result limit, previews, animation, and excluded paths |
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

For example, name a CMD command `Downloads`, use `📂` as its icon, and set its command to `explorer.exe "%USERPROFILE%\Downloads"`. Browse Microsoft's [Segoe Fluent Icons list](https://learn.microsoft.com/en-us/windows/apps/design/iconography/segoe-fluent-icons-font) for Windows glyphs and their codes.

Custom shell examples (the shell must already be installed):

| Shell | Shell Executable | Shell Arguments |
| :--- | :--- | :--- |
| Git Bash | `%ProgramFiles%\Git\bin\bash.exe` | `-c {command}` |
| Nushell | `nu.exe` | `-c {command}` |
| Bash in WSL | `wsl.exe` | `--exec bash -lc {command}` |

Use a full executable path if it is installed somewhere else. WSL requires an installed Linux distribution. PowerShell 7 installation options are described in [Microsoft’s documentation](https://learn.microsoft.com/en-us/powershell/scripting/install/install-powershell-on-windows).

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

## Built-In Commands and Utilities

| Command | Description | Example | Enter Action |
| :--- | :--- | :--- | :--- |
| `<sum>` | Calculation | `100 * 5`<br>`√(144)`<br>`15% × 200` | Copies the result |
| `<number> hex`, `0xFF`, `0b1010` | The number in other bases (digits grouped: `0b111 1111 1111`) | `2047 hex`<br>`0x7FF` | Copies the hex value |
| `<number> <unit>` | The number in the usual units of its kind, all on one line first | `100 km`<br>`32 f`<br>`1,5 kg` | Copies the selected one |
| `<number> <unit> → <unit>` | Just that conversion (also `to`, `in`, `->`, `=`) | `100 km → mi`<br>`100 mbps → MB/s` | Copies the result |
| `/ip` | Lists all active Wi-Fi, Ethernet, and VPN adapters with IP, subnet, gateway, and adapter name | `/ip` | Copies selected IP address to clipboard |
| `?<query>` | Searches the web using the default search engine URL | `?weather today` | Opens search in default web browser |
| `?<shortcut> <query>` | Searches specific service using configured shortcut keyword | `?yt lofi hip hop`<br>`?gh windhawk` | Opens query on target service |

## Troubleshooting and Compatibility

- **Everything is reported as not running:** start the Everything application and leave it running in the tray. Its service alone does not provide the search IPC window. Everything 1.4 and 1.5 alpha/beta, including named instances, are supported.
- **A preview shows only an icon:** enable File Preview. Common image formats have a built-in decoding fallback; specialist formats may require a thumbnail provider, and videos may require a compatible Windows codec. Unavailable or unreadable content falls back to its icon and available properties.
- **Folder sizes are missing:** enable Everything's **Index folder size** option; see [Folder Sizes in Preview](#folder-sizes-in-preview). The mod does not recursively scan folders to calculate sizes.
- **Start Menu Styler appearance is not copied yet:** open Start so its layout and live visual tree can be measured (and open a context menu in Start once so its live menu presenter can be captured). Supported static glass effects are captured when Start creates them. If a custom style was already applied before the mod loaded, reapply the current Start Menu Styler theme once so its material can be captured. Compatible last-known definitions are recovered from this mod's Windhawk cache after updates and off/on cycles, until Start creates fresh brushes. Live image/visual sources, animated custom effects, and unsupported effects use an approximation; their appearance may differ.
- **A custom command does not start:** choose the shell that understands the command. PowerShell 7 and custom shells must already be installed. Hide Terminal hides the shell console; it does not shorten the command's lifetime or hide windows opened by the command.

Tested on **Windows 11 25H2 and 26H2, x86-64**. Other Windows versions and builds, including Insider builds, have not been verified. For a reproducible problem, include the full Windows build, mod version, Everything version and instance name, other Start/search mods, and steps to reproduce it.

## Upgrading from 1.x

Version 2.0 organizes settings into collapsible sections. Windhawk does not transfer preferences from the old flat setting names to these sections automatically. Before upgrading from 1.x, save your settings or note your choices, then reapply them in the new sections after updating. This includes web search shortcuts and excluded path patterns. Custom commands and custom unit conversions keep their existing setting names.
