del /Q _texts.txt
del /Q _out.txt
..\x64\Release\UE4TextExtractor.exe . _texts.txt > _out.txt
if errorlevel 1 (
    echo. >> _out.txt
    echo. >> _out.txt
    echo !!!!!!!!!!!!!!!!!!!!!!!!CRASH!!!!!!!!!!!!!!!!!!!!!!!! >> _out.txt
)