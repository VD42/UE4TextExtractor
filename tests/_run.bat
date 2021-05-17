..\x64\Release\UE4TextExtractor.exe . _texts.txt
if errorlevel 1 (
    echo. >> _texts.txt
    echo. >> _texts.txt
    echo !!!!!!!!!!!!!!!!!!!!!!!!CRASH!!!!!!!!!!!!!!!!!!!!!!!! >> _texts.txt
)