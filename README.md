# UE4TextExtractor

[![CircleCI](https://circleci.com/gh/VD42/UE4TextExtractor/tree/master.svg?style=svg)](https://circleci.com/gh/VD42/UE4TextExtractor/tree/master)

Extract localizable text from .uasset, .uexp and .umap files and convert locres to txt and backward.
  
#### Usage  

Extract localizable texts to locres or txt file:  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old]`  
`UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file>`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\unpacked" "C:\MyGame\Content\Paks\texts.locres"`  
  
Convert locres to txt or backward:  
`UE4TextExtractor.exe <path to texts.txt file> <path to texts.locres file> [-old]`  
`UE4TextExtractor.exe <path to texts.locres file> <path to texts.txt file>`  
Example: `UE4TextExtractor.exe "C:\MyGame\Content\Paks\texts.txt" "C:\MyGame\Content\Paks\texts.locres" -old`  
  
Use `-old` modifier for old-version locres file generation.