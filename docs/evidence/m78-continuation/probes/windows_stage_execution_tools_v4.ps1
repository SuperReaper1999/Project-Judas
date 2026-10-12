$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$root='C:\JudasValidation';$base='http://192.168.122.1:8872';$out="$root\M78ContinuationResults"
function Upload($p,$name){Invoke-WebRequest -UseBasicParsing "$base/$name" -Method Post -InFile $p|Out-Null}
try{
 $name='m78-continuation-execution-tools-v4.json';$m=(Invoke-WebRequest -UseBasicParsing "$base/$name").Content|ConvertFrom-Json;$target="$root\M78ContinuationSource\Scripts\v4"
 if(Test-Path $target){throw 'Preserve prior staged execution tools'};New-Item -ItemType Directory $target -Force|Out-Null;$zip="$target\tools.zip"
 Invoke-WebRequest -UseBasicParsing "$base/$($m.archiveName)" -OutFile $zip;if((Get-FileHash $zip).Hash.ToLower() -ne $m.archiveSHA256){throw 'Tool archive SHA mismatch'};Expand-Archive -LiteralPath $zip -DestinationPath $target
 $rows=@();foreach($file in $m.files){$p=Join-Path $target $file.path;if((Get-FileHash $p).Hash.ToLower() -ne $file.sha256){throw 'Tool source SHA mismatch'};$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($p,[ref]$tokens,[ref]$errors);$rows+=@{path=$p;sha256=$file.sha256;parserErrors=@($errors|ForEach-Object {$_.Message})};if($errors.Count){throw 'Native PowerShell parser error'}}
 $receipt="$out\m78-continuation-execution-tools-v4-stage.json";@{scope=$m.scope;tools=$rows;freeBytes=(Get-PSDrive C).Free}|ConvertTo-Json -Depth 5|Set-Content $receipt -Encoding UTF8;Upload $receipt ([IO.Path]::GetFileName($receipt))
}catch{$p="$out\m78-continuation-execution-tools-v4-stage-error.txt";[string]$_.Exception.Message|Set-Content $p;Upload $p ([IO.Path]::GetFileName($p));exit 1}
