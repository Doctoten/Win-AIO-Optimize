#requires -Version 5.1
<# Check the actual PE resources, without executing the app. Refresh only this
   file's Explorer entry after packaging; never reset the global icon cache. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Binary,
    [switch]$RefreshExplorer
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$binaryPath = (Resolve-Path -LiteralPath $Binary).Path
$iconPath = Join-Path $PSScriptRoot '..\src\icon.ico'
if (-not ('WaioPackaging.IconCheck' -as [type])) {
    Add-Type @'
using System;
using System.IO;
using System.Runtime.InteropServices;
namespace WaioPackaging {
    public static class IconCheck {
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
        [DllImport("kernel32.dll", EntryPoint="FindResourceW", SetLastError=true)]
        static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);
        [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
        [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
        [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
        static extern uint ExtractIconEx(string file, int index, IntPtr[] large, IntPtr[] small, uint count);
        [DllImport("user32.dll")] static extern bool DestroyIcon(IntPtr icon);
        [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
        static extern void SHChangeNotify(uint change, uint flags, string path, IntPtr unused);

        static byte[] Resource(IntPtr module, int id, int type) {
            IntPtr entry = FindResource(module, (IntPtr)id, (IntPtr)type);
            if (entry == IntPtr.Zero) throw new InvalidDataException("Missing icon resource.");
            int length = checked((int)SizeofResource(module, entry));
            IntPtr data = LockResource(LoadResource(module, entry));
            if (data == IntPtr.Zero || length == 0) throw new InvalidDataException("Empty icon resource.");
            byte[] bytes = new byte[length];
            Marshal.Copy(data, bytes, 0, length);
            return bytes;
        }
        public static void Verify(string binary, string icon) {
            byte[] source = File.ReadAllBytes(icon);
            if (source.Length < 6 || BitConverter.ToUInt16(source, 0) != 0 ||
                BitConverter.ToUInt16(source, 2) != 1 || BitConverter.ToUInt16(source, 4) != 7)
                throw new InvalidDataException("Expected seven application icon sizes.");
            // Data-file loading never runs the executable or requests elevation.
            IntPtr module = LoadLibraryEx(binary, IntPtr.Zero, 0x22);
            if (module == IntPtr.Zero) throw new InvalidDataException("Cannot read executable resources.");
            try {
                byte[] group = Resource(module, 1, 14);
                if (group.Length != 6 + 14 * 7 || BitConverter.ToUInt16(group, 2) != 1 ||
                    BitConverter.ToUInt16(group, 4) != 7)
                    throw new InvalidDataException("Executable icon group is incomplete.");
                int[] sizes = {16, 24, 32, 48, 64, 128, 256};
                for (int i = 0; i < sizes.Length; i++) {
                    int s = 6 + 16 * i, g = 6 + 14 * i;
                    if (source.Length < s + 16 || source[s] != (byte)(sizes[i] % 256) || source[s + 1] != source[s])
                        throw new InvalidDataException("Unexpected application icon size.");
                    for (int j = 0; j < 12; j++)
                        if (source[s + j] != group[g + j]) throw new InvalidDataException("Stale executable icon entry.");
                    int length = checked((int)BitConverter.ToUInt32(source, s + 8));
                    int offset = checked((int)BitConverter.ToUInt32(source, s + 12));
                    byte[] embedded = Resource(module, BitConverter.ToUInt16(group, g + 12), 3);
                    if (length != embedded.Length || offset < 0 || offset > source.Length - length)
                        throw new InvalidDataException("Invalid embedded icon frame.");
                    for (int j = 0; j < length; j++)
                        if (source[offset + j] != embedded[j]) throw new InvalidDataException("Executable contains an outdated logo.");
                }
            } finally { FreeLibrary(module); }
            IntPtr[] large = new IntPtr[1], small = new IntPtr[1];
            try {
                ExtractIconEx(binary, 0, large, small, 1);
                if (large[0] == IntPtr.Zero || small[0] == IntPtr.Zero)
                    throw new InvalidDataException("Windows Shell cannot extract the application icon.");
            } finally {
                if (large[0] != IntPtr.Zero) DestroyIcon(large[0]);
                if (small[0] != IntPtr.Zero) DestroyIcon(small[0]);
            }
        }
        public static void Refresh(string binary) {
            // SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT.
            SHChangeNotify(0x2000, 0x3005, binary, IntPtr.Zero);
        }
    }
}
'@
}
[WaioPackaging.IconCheck]::Verify($binaryPath, [IO.Path]::GetFullPath($iconPath))
Write-Host 'Embedded app logo: seven sizes match source; Windows Shell extraction: PASS'
if ($RefreshExplorer) {
    [WaioPackaging.IconCheck]::Refresh($binaryPath)
    Write-Host 'Explorer refresh requested for the packaged executable.'
}
