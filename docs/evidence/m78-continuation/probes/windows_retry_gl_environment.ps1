$env:GALLIUM_DRIVER='llvmpipe';$env:JUDAS_ENGINE_ROOT='C:\JudasValidation\Source'
& 'C:\JudasValidation\M78ContinuationSource\Scripts\v4\windows_direct_run.ps1' -ManifestName 'm78-continuation-direct-combined-v2.json' -CaseId 'balance_lab' -RunId 'balance_lab-step-trace-llvmpipe' -Arguments @('C:\JudasValidation\M78ContinuationResults\combined-v2\balance-step-llvmpipe','step-trace')
