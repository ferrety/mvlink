# Application icon

`mvlink-source.png` is the original transparent artwork created with the built-in
image generation tool. Prompt:

> Use case: logo-brand. Asset type: desktop application icon for mvlink, a utility that moves files and leaves symbolic or hard links. Create one crisp simple professional icon: a blue file folder with a bold interlocking chain-link emblem in white and teal across its front, communicating folders and linked files. Flat geometric shapes, subtle depth, strong silhouette, thick clean forms legible at 16 and 32 pixels. Centered square composition, generous safety margin, transparent background. No letters, no words, no watermark, no extra decorations, no tiny details. Produce a single finished icon, not a presentation or icon sheet.

PNG sizes are embedded through `icons.qrc`; `mvlink.png` is the 256px Linux
desktop icon. `mvlink.ico` contains 16, 24, 32, 48, 64, 128, and 256px sizes for
Windows Explorer and executable resources. Derived sizes and ICO packaging use
ImageMagick; no image tools are needed to build the application.

To regenerate the derived files:

```sh
magick mvlink-source.png -resize 256x256 mvlink.png
magick mvlink-source.png -define icon:auto-resize=256,128,64,48,32,24,16 mvlink.ico
for size in 16 24 32 48 64 128 512; do
    magick mvlink-source.png -resize "${size}x${size}" "mvlink-${size}.png"
done
```
