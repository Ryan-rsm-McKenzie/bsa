$ErrorActionPreference = 'Stop';
$toolsDir = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$url = 'https://github.com/OpenCppCoverage/OpenCppCoverage/releases/download/release-0.9.9.0/OpenCppCoverageSetup-x86-0.9.9.0.exe'
$url64 = 'https://github.com/OpenCppCoverage/OpenCppCoverage/releases/download/release-0.9.9.0/OpenCppCoverageSetup-x64-0.9.9.0.exe'

$packageArgs = @{
	packageName    = $env:ChocolateyPackageName
	unzipLocation  = $toolsDir
	fileType       = 'exe'
	url            = $url
	url64bit       = $url64

	softwareName   = 'opencppcoverage*'

	checksum       = 'D6CE9F61457AC084DEFC7A37B383E81CEEB01EF30933E104017DB9F33BC3AB05'
	checksumType   = 'sha256'
	checksum64     = '2295A733DA39412C61E4F478677519DD0BB1893D88313CE56B468C9E50517888'
	checksumType64 = 'sha256'

	silentArgs     = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-'
	validExitCodes = @(0)
}

Install-ChocolateyPackage @packageArgs
Install-ChocolateyPath "$($env:SystemDrive)\Program Files\OpenCppCoverage" 'Machine'
