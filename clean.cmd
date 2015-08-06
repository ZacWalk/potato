for /F %%f in ('dir /s /b debug release bin obj') do del /q /s %%f
del *.sdf
