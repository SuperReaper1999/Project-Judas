$ErrorActionPreference='Stop';$root='P:\JudasValidation\CleanDistribution';New-Item -ItemType Directory -Force "$root\Source"|Out-Null
$hash=(Get-FileHash "$root\Judas-Experimental-source.tar.gz" -Algorithm SHA256).Hash.ToLowerInvariant()
if($hash -ne '84066d077f9d08cd67e796c2ce7147591c2fcf24370931129f4340346a31d265'){throw 'Transferred archive hash mismatch'}
$hash
