# FLOWTONE 0.4.0 verification

Compiled source commit: `75e5b6a224779dd9261080db4609a68ae7610df1`. JUCE: `454fe891acc8204f99f4ef1e3a28c84b1a4da145`. Later documentation and installer repackaging commits do not change the compiled plug-in sources.

| Check | Russian | English |
|---|---|---|
| Full local processor/analysis regression | PASS | PASS |
| [Independent macOS CI](https://github.com/Abynarc/FLOWTONE/actions/runs/41633816387) | PASS | PASS |
| [Windows x64 CI](https://github.com/Abynarc/FLOWTONE/actions/runs/41633816475) | PASS | PASS |

UI checks exercise the actual dropdown in both directions, independent instances, no processing-parameter changes, translated controls/menus/tooltips, 1000/1280/1600-pixel layouts, editor recreation, state loading with an open editor, and old projects without a language property. Catalog coverage: 97 entries. Switching language retains completed file analysis, source selection and processing settings. RU/EN Windows screenshots were inspected.

The processor suite covers pitch/duration/stereo preservation, quality/formant modes, quality transitions, automation, latency-aligned host bypass, variable blocks, real-time allocation guards, A/B and state. Analysis tests cover live/file sources, file formats, cache/repeat/cancellation, retained streams, long intros/outros, ambiguous/variable tempo, 32 rhythmic stress cases and source changes. Worker creation/destruction stress passed 10,000 cycles in each CI language run.

macOS PKG: arm64 + x86_64, minimum macOS 12. Package expansion, signed-payload hashes, ad-hoc signature, install/upgrade scripts on an isolated volume, system and per-user backups, and corrupted-payload rejection passed. The signed AU was loaded directly, its state round-tripped, and 144,000 stereo frames rendered at 48 kHz; the +0.5-semitone pitch test was within 0.25 cents.

Windows: build, install, byte comparison, upgrade backup, pluginval strictness 5, uninstall and backup retention passed. [Final installer repackaging](https://github.com/Abynarc/FLOWTONE/actions/runs/41635813364) replaces the Russian instructions with the current version. It checks the source commit, original artifact hashes and unchanged VST3 binary, and repeats install/upgrade/backup/VST3/uninstall validation. Audio-processing tests refer to the source-identical full CI run above.

Limits: no manual listening or interactive Logic/FL Studio session was performed. The Intel slice was cross-compiled; native Intel Mac hardware was not tested. The macOS package is not Developer ID signed/notarized; Windows has no publisher certificate. Host automation parameter names retain their existing registration independently of the custom editor language.
