#requires -Version 5.1
<# Export the code-native vector mark in assets/logo.svg to Windows icon sizes.
   Development-only; the portable app has no drawing-tool/runtime dependencies. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
public static class WaioIconExport {
    public static byte[] Render(int size) {
        using (var canvas = new Bitmap(size * 4, size * 4, PixelFormat.Format32bppArgb))
        using (var g = Graphics.FromImage(canvas)) {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.ScaleTransform(size / 64f, size / 64f);
            using (var tile = new GraphicsPath()) {
                tile.AddArc(8,8,116,116,180,90);
                tile.AddArc(132,8,116,116,270,90);
                tile.AddArc(132,132,116,116,0,90);
                tile.AddArc(8,132,116,116,90,90);
                tile.CloseFigure();
                using (var blue = new LinearGradientBrush(new Point(8,8),new Point(248,248),Color.FromArgb(39,137,245),Color.FromArgb(37,65,183)))
                    g.FillPath(blue,tile);
            }
            using (var mark = new GraphicsPath(FillMode.Alternate)) {
                mark.AddLine(57,190,112,68);
                mark.AddBezier(112,68,123,50.667f,133,50.667f,144,68);
                mark.AddLines(new PointF[] {new PointF(144,68),new PointF(199,190),new PointF(164,190),new PointF(151,159),new PointF(105,159),new PointF(92,190),new PointF(57,190)});
                mark.CloseFigure();
                mark.AddPolygon(new PointF[] {new PointF(128,99),new PointF(112,137),new PointF(144,137)});
                g.FillPath(Brushes.White,mark);
            }
            using (var mint = new SolidBrush(Color.FromArgb(134,245,208))) g.FillEllipse(mint,178,52,26,26);
            using (var output = new Bitmap(size,size,PixelFormat.Format32bppArgb))
            using (var down = Graphics.FromImage(output))
            using (var bytes = new MemoryStream()) {
                down.CompositingMode = CompositingMode.SourceCopy;
                down.InterpolationMode = InterpolationMode.HighQualityBicubic;
                down.DrawImage(canvas,0,0,size,size);
                output.Save(bytes,ImageFormat.Png);
                return bytes.ToArray();
            }
        }
    }
}
'@
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sizes = @(16,24,32,48,64,128,256)
$frames = @($sizes | ForEach-Object { ,([WaioIconExport]::Render($_)) })
$stream = New-Object IO.MemoryStream
$writer = New-Object IO.BinaryWriter($stream)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i=0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
        $offset += $frames[$i].Length
    }
    foreach ($frame in $frames) { $writer.Write([byte[]]$frame) }
    $writer.Flush()
    [IO.File]::WriteAllBytes((Join-Path $repoRoot 'src/icon.ico'),$stream.ToArray())
} finally { $writer.Dispose(); $stream.Dispose() }
Write-Host 'Generated src/icon.ico: 16, 24, 32, 48, 64, 128, 256 px.'
