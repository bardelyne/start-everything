# Development checks

Run on Windows x64 with Windhawk's compiler installed. CXX can select another compatible compiler. The mod remains a standalone source file.

- node scripts/embed-locales.mjs --check: validates all language keys, placeholders and the generated catalog.
- node scripts/test-localization.mjs: compiles the production calculator, conversions and localization tables and runs their regression cases.
- node scripts/test-material-capture.mjs: compiles production material capture/reconstruction code and tests native effects, non-owning caches, reused factory/brush identities (including failed and animated captures), current-load hidden brushes, warm enable/update cycles with existing opaque XAML wrappers, live changes while disabled, balanced one-time resource refresh, reentrant/failed/unsupported refreshes, cache reset/eviction, untouched host metadata, and cross-thread cleanup.

- node scripts/test-xaml-lifecycle.mjs: reproduces the lost XAML diagnostics subscription after a secondary island thread exits, then verifies the production shutdown drain across 12 reloads while another XAML thread stays alive. The negative control deliberately skips the drain and must reproduce a pending subscription.

The material suite runs in separate hidden processes without a Windhawk storage backend. Its test hooks apply only to those processes. A second process exits with populated material caches and live composition objects, deliberately skipping normal mod cleanup to exercise global destruction after the other threads terminate. The suite does not inject into Explorer or change Windhawk settings. The test runner embeds a Windows compatibility manifest into its own temporary executable so desktop XAML is available.

All test runners accept an output directory as their first argument. Results are saved there in results.log.

The lifecycle suite attaches its own diagnostic test DLL only to its own hidden test process. It never attaches to Explorer or changes installed mod settings. Each process has a timeout, and the surviving host remains alive during every secondary-thread reload.
