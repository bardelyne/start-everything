# Development checks

Run on Windows x64 with Windhawk's compiler installed. CXX can select another compatible compiler. The mod remains a standalone source file.

- node scripts/embed-locales.mjs --check: validates all language keys, placeholders and the generated catalog.
- node scripts/test-localization.mjs: compiles the production calculator, conversions and localization tables and runs their regression cases.
- node scripts/test-material-recovery.mjs: compiles production material capture/reconstruction code and tests native effects, live and hidden reload recovery, bounded caches, storage validation, untouched host metadata, and cross-thread cleanup.

The material suite runs in a separate hidden process with a mock Windhawk storage backend. Its test hooks apply only to that process. It does not inject into Explorer or change Windhawk settings. The test runner embeds a Windows compatibility manifest into its own temporary executable so desktop XAML is available.

Both test runners accept an output directory as their first argument. Results are saved there in results.log.
