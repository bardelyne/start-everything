# Interface translations

The JSON files contain 50 complete left-to-right language packs. `en.json` defines the keys and message placeholders. They cover the search window, context menus, preview labels, and calculator, conversion, web, network and command messages.

Edit a language file, then run:

```text
node scripts/embed-locales.mjs
node scripts/embed-locales.mjs --check
node scripts/test-localization.mjs
```

The generator validates keys, placeholders and writing direction, then embeds the tables and language dropdown into `start-everything.wh.cpp`. The catalog source stays standalone; users do not need these JSON files or Node.js. The tests compile extracted production localization and calculator code on Windows with Windhawk's compiler; `CXX` can select another compatible compiler.

Use `{0}`, `{1}`, etc. to place values in a complete message. Keep every placeholder exactly once unless the English source specifies otherwise. `{command}` is a literal command-template marker and must remain unchanged. Do not translate product names, file paths, commands, mathematical function names or unit symbols.

Auto mode reads the Windows UI language. Regional variants use their supported language or script; unsupported languages fall back to English. Changing the interface language does not change Windows' display language, keyboard layout or regional number/date formats. Windows-provided app names, Settings titles, property values and error text remain in their original language. Windhawk's settings descriptions and this documentation are in English.

Translations are included with AI assistance and can be improved by native speakers. When reporting a wording issue, include the language and string key.
