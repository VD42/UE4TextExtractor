# UE4TextExtractor

Extract localizable text from .uasset, .uexp and .umap files and convert locres to txt and backward.
  
#### Usage  

Extract localizable texts to locres or txt file:  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old] [-raw-text-signatures=<signature1>,<signature2>,...] [-all-uexps]`  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file> [-raw-text-signatures=<signature1>,<signature2>,...] [-all-uexps] [-src]`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\unpacked" "C:\MyGame\Content\Paks\texts.locres"`  
  
Use `-raw-text-signatures=<signature1>,<signature2>,...` (or `-raw-text-signatures=all` if you don't want to go into detail, but it's not recommended) modifier for parsing localizable text by custom signatures. See also: [here](https://github.com/VD42/UE4TextExtractor/blob/master/RAW_TEXT_SIGNATURES.md).  
Use `-all-uexps` modifier for additionaly parsing uexp files without matching uasset or umap files.  
Use `-src` modifier to add string source information (filenames) to the txt file.  
  
Convert locres to txt or backward:  
`UE4TextExtractor.exe <path to texts.txt file> <path to texts.locres file> [-old]`  
`UE4TextExtractor.exe <path to texts.locres file> <path to texts.txt file>`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\texts.txt" "C:\MyGame\Content\Paks\texts.locres" -old`  
  
Use `-old` modifier for old-version locres file generation.
