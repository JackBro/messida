# messida
IDA debugger plugin based on MESS emulator

## Compilation (for Windows)
1. Download latest **MESS** source using **Build Tools**;
2. Apply **patch.diff** to **src\mame** dir.
3. Generate solution for **Visual Studio 2013** (*make vs2013 PYTHON_EXECUTABLE=C:\buildtools\vendor\python\python SUBTARGET=megadrive DRIVERS=src/mess/drivers/megadriv.c*);
4. Put all files from this repo to **src\mame\3rdparty\messida**;
5. Open **messida_emu.props**, **messida.vcxproj** in any text editor and edit paths in **IDA_SDK** accordingly to your real paths;
6. Open **messida.vcxproj** in any text editor and edit path in **IDA_DIR** accordingly to your real path;
7. Open generated solution and compile it;
8. Add **messida.vcxproj** to solution;
9. Open **View->Other Windows->Property Manager**;
10. Select both **emu**, **osd_windows**, then right click, select **Add Existing Property Sheet...** and open **messida_emu.props** file;
11. Now, press **Build** or **Rebuild** to generate **messida.plw** to your **IDA\plugins** dir.
