# UE4TextExtractor

Extract localizable text from .uasset, .uexp and .umap files and convert locres to txt and backward.
  
#### Usage  

Extract localizable texts to locres or txt file:  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old] [-raw-text-signatures=<signature1>,<signature2>,...]`  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file> [-raw-text-signatures=<signature1>,<signature2>,...]`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\unpacked" "C:\MyGame\Content\Paks\texts.locres"`  
  
Use `-raw-text-signatures=<signature1>,<signature2>,...` modifier for parsing localizable text by custom signatures. See also: [here](https://github.com/VD42/UE4TextExtractor/blob/master/RAW_TEXT_SIGNATURES.md).  
Use `-all-uexps` modifier for additionaly parsing uexp files without matching uasset or umap files.  
  
Convert locres to txt or backward:  
`UE4TextExtractor.exe <path to texts.txt file> <path to texts.locres file> [-old]`  
`UE4TextExtractor.exe <path to texts.locres file> <path to texts.txt file>`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\texts.txt" "C:\MyGame\Content\Paks\texts.locres" -old`  
  
Use `-old` modifier for old-version locres file generation.