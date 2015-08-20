# messida
IDA debugger plugin based on MESS emulator

## Compilation (for Windows)
1. Download last **MESS** source using **Build Tools**;
2. Generate solution for **Visual Studio 2013** (*make vs2013 PYTHON_EXECUTABLE=C:\buildtools\vendor\python\python SUBTARGET=megadrive DRIVERS=src/mess/drivers/megadriv.c*);
3. Open **messida_emu.props**, **messida.vcxproj** in any text editor and edit paths in **IDA_SDK** accordingly to your real paths;
4. Open **messida.vcxproj** in any text editor and edit path in **IDA_DIR** accordingly to your real path;
5. Open generated solution and compile it;
6. Add **messida.vcxproj** into solution;
7. Open **View->Other Windows->Property Manager**;
8. Select both **emu**, **osd_windows**, then right click, select **Add Existing Property Sheet...** and open **messida_emu.props** file;
9. Now, press **Build** or **Rebuild** to generate **messida.plw** to your **IDA\plugins** dir.
