@echo off

rd /S /Q .\build
rd /S /Q .\obj
del /Q .\lib\*.*
del /S /Q .\Demo\obj\*.*
del /S /Q .\Demo\VASTsim_consoleDev\*.pos
del /S /Q .\Demo\VASTsim_consoleDev\*.log
del /S /Q .\bin\*.*
del /S  .\*.ncb
del /S  .\*.user
del /S  .\*.obj
del /S  .\*.o
del /S  .\*.plg

