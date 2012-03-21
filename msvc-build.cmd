call "C:\Program Files/Microsoft Visual Studio 10.0/Common7/Tools/vsvars32.bat"
perl contrib/buildsystems/generate -g Vcproj
start git.sln
