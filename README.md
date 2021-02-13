# YsIFontConverter
 A simple tool that is used to convert a .ttf font file into the format used by Ys I Chronicles<br/>
# Introduction
This is the first source code I publish and since I lack experience, the code is probably messy.<br/>
I have added comments describing each function, but if it is not enough, I recommend you contact me at feitaishi45@gmail.com<br/>
In any case, I tried it for a project translating Ys I in french, and it has also been used for a spanish translation.<br/>
So it should at least fulfill its mission, which would be to allow teams translating the game to render special characters such as é à ã
which are not available in the default font file.
# Dependencies
This tool relies on the FreeType library to render the letters from the TTF file.<br/>
It also uses the UTF8CPP portable library https://github.com/nemtrif/utfcpp/.
# How to use
First you'll have to specify the path to the font you want to insert into the game using the config_font.ini file.<br/> 
This file should be placed in the same folder than your font converter executable. You can play with the other settings inside this file but it should be okay to leave the default values.<br/>
If you want to add custom letters, just write them down as in the example at the end of the config file.<br/>
Then all you have to do is drag and drop the original font file ("FONT_PSP.SKI") extracted from the game .na archive onto the font converter executable, and wait for it to finish rendering the font (it can take a good amount of time). When it is finished, you'll be able to retrieve the font file inside the outputs folder that was just created,
as well as a text.ini file which contains several pairs of integers.The first column of integers contains the UTF-32 encoding for each character that was changed,
the second column contains the code to put inside the game scenario file.
Again don't hesitate to reach out to me at my mail if you encounter any trouble using this tool.
