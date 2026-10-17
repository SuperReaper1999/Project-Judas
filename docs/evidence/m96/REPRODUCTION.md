# M96 reproduction

Use the M96 checkpoint on Experimental, repository root, native Linux desktop GL.
Existing dependencies suffice; no new runtime libraries were introduced.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas_ui_production_tests judas_ui_showcase_application_tests judas_runtime_ui_tests judas_runtime_ui_application_tests judas_text_localization_tests judas_input_tests judas_touch_input_tests judas_local_multiplayer_tests judas_local_multiplayer_script_tests judas_local_device_adapter_tests judas_animation_workflow_tests judas_save_application_tests judasjs_examples_tests judas_editor judas judas_export judas_script_check -j 3
python3 scripts/m96_validation.py --output .cache/m96/reproduction-validation
python3 scripts/m96_delivery.py --output .cache/m96/reproduction-delivery
```

Output directories must be fresh: failures are retained, not overwritten.
The checker uses the existing developer-only TypeScript installation under
`.cache/m90/types/node_modules/typescript`; substitute a standard local TS 5.9.3
installation when reproducing elsewhere. Node/TypeScript are not game dependencies.

The application check performs real native SDL text delivery, JS UI logic, normal
save-service publication into a fresh world, independent player documents and a
720×1280 window resize. The core check also exercises native clipboard and virtual
SDL device routing; physical gamepads/input methods are not inferred from this.

The editor driver copies the project before ordinary editor source-save/Play-Stop
checks. It exports through the existing exporter, byte-verifies a package copy in
/tmp and launches it from a different unrelated working directory. Saves and data
are isolated through the existing XDG application-data path.

## Android

With the installed SDK/NDK/JDK described in docs/ANDROID.md:

```sh
JAVA_HOME="$PWD/.cache/m88/jdk17" PATH="$PWD/.cache/m88/jdk17/bin:$PATH" python3 tools/build_android.py projects/ui_showcase/ui_showcase.judasproj --output .cache/m96/reproduction-arm64.apk --cache .cache/m88 --abis arm64-v8a --jobs 3 --id org.projectjudas.fieldnotes --name "Judas Field Notes"
```

This reuses the existing Android build/dependency cache and development signer.
Use your own SDK/JDK/cache/signing settings outside this machine. The build tool
verifies the APK signature/alignment. No device installation is performed by this
command. No Android device/emulator was attached during the final M96 check;
phone keyboard/IME/touch, app lifecycle and hardware performance remain outstanding.

`development/android-build.log` preserves the original mesh-codec link failure.
`development/android-link-baseline.txt` identifies the exact accepted CMake
placement that excluded the dependency before Android returned. Corrected build
and artifact hashes are separate; no older APK is substituted for the final one.

## Authoring

Open projects/ui_showcase/ui_showcase.judasproj in build/judas_editor. Select the
showcase or player UI asset, choose Edit UI document, select the input/list/scroll
node in its hierarchy and use the properties and canvas. Source preview emits no
game events. Save UI source, then restart Play to load the edited document.

Do not run scripts/m96_lab.py as a smoke test: it deliberately regenerates the
original source assets/project/scene, replacing local authored edits in that lab.
