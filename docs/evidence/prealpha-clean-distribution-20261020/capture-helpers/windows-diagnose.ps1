$ErrorActionPreference='Continue';$r='P:\JudasValidation\CleanDistribution'
Get-CimInstance Win32_Process|Where-Object {$_.Name -in @('cmake.exe','MSBuild.exe','cl.exe','link.exe','cmd.exe','Tracker.exe')}|Select ProcessId,ParentProcessId,Name,CommandLine|ConvertTo-Json -Depth 4
if(Test-Path "$r\Build\CMakeFiles\CMakeConfigureLog.yaml"){Get-Content "$r\Build\CMakeFiles\CMakeConfigureLog.yaml" -Encoding UTF8 -Tail 25}
Get-ChildItem "$r\Build" -Filter '*.lib' -Recurse -ErrorAction SilentlyContinue|Select -First 4 FullName,LastWriteTimeUtc|ConvertTo-Json
