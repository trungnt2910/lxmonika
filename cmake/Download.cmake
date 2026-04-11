function(monika_download_zip url dest)
    if("${ARGV2}" STREQUAL "")
        set(STRIP_COMPONENTS 0)
    else()
        set(STRIP_COMPONENTS ${ARGV2})
    endif()

    message(STATUS "Downloading: ${url}")

    file(MAKE_DIRECTORY "${dest}")

    execute_process(
        COMMAND powershell -NoProfile -Command "
            Add-Type -AssemblyName System.Net.Http;
            Add-Type -AssemblyName System.IO.Compression;
            Add-Type -AssemblyName System.IO.Compression.FileSystem;

            $url = '${url}';
            $dest = '${dest}';
            $strip = [int]${STRIP_COMPONENTS};

            $client = New-Object System.Net.Http.HttpClient;
            try {
                $response = $client.GetAsync(
                    $url, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead
                ).Result;
                if (-not $response.IsSuccessStatusCode) {
                    throw \"Download Failed: HTTP Error $($response.StatusCode)\"
                }

                $stream = $response.Content.ReadAsStreamAsync().Result;
                $zip = New-Object System.IO.Compression.ZipArchive($stream);

                $totalEntries = $zip.Entries.Count;
                $currentEntry = 0;
                $lastReported = -1;

                foreach ($entry in $zip.Entries) {
                    $currentEntry++;
                    $progress = [Math]::Floor(($currentEntry / $totalEntries) * 100);

                    # Only print at 10% increments and only once per increment
                    if ($progress % 10 -eq 0 -and $progress -ne $lastReported) {
                        Write-Host \"Downloading: $progress%\";
                        $lastReported = $progress;
                    }

                    $parts = $entry.FullName.Split(
                        @('/', '\\'), [System.StringSplitOptions]::RemoveEmptyEntries
                    );
                    if ($parts.Count -le $strip) { continue; }

                    $newPath = $parts[$strip..($parts.Count-1)] `
                        -join [System.IO.Path]::DirectorySeparatorChar;
                    $targetFile = [System.IO.Path]::Combine($dest, $newPath);

                    $parentDir = [System.IO.Path]::GetDirectoryName($targetFile);
                    if (-not (Test-Path $parentDir)) {
                        [System.IO.Directory]::CreateDirectory($parentDir) | Out-Null
                    }

                    if (-not [string]::IsNullOrEmpty($entry.Name)) {
                        [System.IO.Compression.ZipFileExtensions]::ExtractToFile(
                            $entry, $targetFile, $true
                        );
                    }
                }
            } finally {
                if ($zip) { $zip.Dispose() }
                if ($stream) { $stream.Dispose() }
                $client.Dispose();
            }
        "
        RESULT_VARIABLE STATUS
    )

    if(NOT STATUS EQUAL 0)
        message(FATAL_ERROR "Failed to stream and extract zip from ${url}")
    endif()
endfunction()
