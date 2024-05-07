# Custom raw text signatures

### What if I don't want to read or understand anything?

Use `-raw-text-signatures=all` modifier:  
  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old] -raw-text-signatures=all`  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file> -raw-text-signatures=all`  
  
But it's highly not recommended, and it's still better to read what's written below.

### Why would I want to do that?

If you have files that contain something very similar to localizable text:

![raw_text_screenshot](https://github.com/VD42/UE4TextExtractor/assets/1012077/692f7cb1-fb37-4a06-b88b-20c45607cbcc)

(that is, in the form of three fields, where the first is the namespace (usually an empty string, and only it is currently supported), the key (usually a 32-character hexadecimal sequence, and only it is currently supported), and the text),

but by default the program does not recognize these strings, you can use the option for custom raw text signatures.

### How to use it?

Use `-raw-text-signatures=<signature1>,<signature2>,...` modifier:  
  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old] -raw-text-signatures=<signature1>,<signature2>,...`  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file> -raw-text-signatures=<signature1>,<signature2>,...`  
  
For example:  
`UE4TextExtractor.exe "C:\MyGame\Content\Paks\unpacked" "C:\MyGame\Content\Paks\texts.txt" -raw-text-signatures=SnowfallScriptAsset`

### How does it work?

When a specified sequence is encountered in a uasset- or umap-file (`SnowfallScriptAsset` for example, you can also specify multiple signatures separated by commas), localizable string processing is enabled for that file (or for the corresponding uexp-file), which is described above. You need to choose some sequence that will be present only in the files you want to further process. If you choose too general sequence, the program operation time will significantly increase, and the result will be worse. If you choose too narrow sequence, only one file will be processed.

![raw_text_screenshot_uasset](https://github.com/VD42/UE4TextExtractor/assets/1012077/74cbcbc3-db36-491b-9bb4-d3506d55a961)

`SnowfallScriptAsset` is a good choice, as all files that contain assets of this type related to the Snowfall plugin will be processed. `Package` is a bad choice, as almost all game files will be processed, it will be long, and the result will probably be a lot of garbage. `PrivatePhotoshoot` is a bad choice, as only one file `PrivatePhotoshoot.uasset` will be processed.

### What do I get?

Even more lines to translate! Of course, if these strings are present and have the necessary format, and everything is configured correctly.
