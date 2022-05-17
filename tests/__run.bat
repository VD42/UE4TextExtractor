del /Q _texts.txt
del /Q _texts.locres
del /Q _texts.old.locres
del /Q _texts.locres.txt
del /Q _texts.old.locres.txt
del /Q _out.txt

echo EMPTY_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe >> _out.txt

echo. >> _out.txt
echo EXTRACT_TO_LOCRES_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe . _texts.locres >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)

echo. >> _out.txt
echo EXTRACT_TO_OLD_LOCRES_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe . _texts.old.locres -old >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)

echo. >> _out.txt
echo EXTRACT_TO_TXT_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe . _texts.txt >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)

echo. >> _out.txt
echo CONVERT_LOCRES_TO_TXT_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe _texts.locres _texts.locres.txt >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)
comp _texts.txt _texts.locres.txt /M || echo DIFFERENCES! >> _out.txt

echo. >> _out.txt
echo CONVERT_OLD_LOCRES_TO_TXT_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe _texts.old.locres _texts.old.locres.txt >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)
comp _texts.txt _texts.old.locres.txt /M || echo DIFFERENCES! >> _out.txt

echo. >> _out.txt
echo CONVERT_TXT_TO_LOCRES_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe _texts.txt _texts.convert.locres >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)
comp _texts.locres _texts.convert.locres /M || echo DIFFERENCES! >> _out.txt

echo. >> _out.txt
echo CONVERT_TXT_TO_OLD_LOCRES_TEST >> _out.txt
..\x64\Release\UE4TextExtractor.exe _texts.txt _texts.convert.old.locres -old >> _out.txt
if errorlevel 1 (
    echo ERROR! >> _out.txt
)
comp _texts.old.locres _texts.convert.old.locres /M || echo DIFFERENCES! >> _out.txt