
#include <math.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <windows.h>
#include "utf8.h"
#include <filesystem>
#include "Character.h"
#include "ArrayOperations.h"
#include <sstream>
#include <algorithm>
#include <map>


int char_height;
int size;
int total_number_of_chars;
size_t space_width;
std::string font;
std::string quiet = "true";
std::vector<Character> chrs;

/*read_length reads an entire character inside the buffer in order to determine its length in bytes,
so that we know which part of the font file we need to replace by a new character*/
size_t calculateTotalSize(std::vector<unsigned char>& buffer, int start_address) {
	size_t total_size = 0;
	size_t header_size = 0x10;  // Assuming the header size is 0x10 (16 bytes)
	size_t offset = 0;

	// Initial parsing to find the size of total_info_section and total_row_section
	
	size_t total_row_section_size = 0;

	int line_number = readShort(buffer, start_address + 6);

	size_t total_info_section_size = line_number * 4;
	size_t current_idx = 0;
	int current_col = 0;
	size_t compteur_de_pixels = 0;
	for (size_t idx_row = 0; idx_row < line_number; idx_row++) {
		
		//first we get the count of empty bytes
		while (compteur_de_pixels < line_number) {
			size_t empty_bytes_count = buffer[start_address + header_size + total_info_section_size + current_idx];
			compteur_de_pixels += empty_bytes_count;
			current_idx++;
			if (compteur_de_pixels < line_number) {
				//il y a encore des pixels à lire, non noirs cette fois
				size_t non_empty_pixels_count = buffer[start_address + header_size + total_info_section_size + current_idx];
				size_t frac = non_empty_pixels_count / 2;
				size_t nb_bytes_to_skip = (non_empty_pixels_count % 2 == 0) ? frac : frac + 1;
				current_idx += nb_bytes_to_skip + 1;
				compteur_de_pixels += non_empty_pixels_count;
			}
			else {
				
				break;
				
			}
		}
		compteur_de_pixels = 0;
		
	}
	size_t row_section_size = current_idx;
	//row_section_size += 2;
	total_size = header_size + total_info_section_size + row_section_size;
	//size_t oui = readInt(buffer, start_address + total_size);
	
	//std::cout << std::hex << oui << " " << start_address + total_size << std::endl;

	return total_size;
}

/*get_addr_char finds the position in the font file where the specified character's drawing is located
code is the code used inside the font file, an internal encoding rule that follows the behaviour of "get_code2" function given below*/
int get_addr_char(int code, std::vector<unsigned char> buffer, bool &success){
	
	int char_number = total_number_of_chars;
	
	if ((((code) * 4+2)>0)&& (((code) * 4+2)<buffer.size())) success = true;
	else {
		success = false; 
		return -1;
	}
	
	int corresponding_offset = readInt(buffer, (code)*4+2); 
	
	if (corresponding_offset == 0) { 
		success = false;
		return -1;
	}
	int addr = (char_number)*4+corresponding_offset+2;	
	
	if (((addr)>0)&& (addr<buffer.size())) success = true;
	else {
		success = false; 
		return -1;
	}
	return addr;
}
/*update_offsets updates the offset part (first part) which contains the offsets of each character, 
depending on the number of changed bytes inside the font file after adding or replacing a character*/
void update_offsets(std::vector<unsigned char> &buffer, int pos, int bytes_added, int nb_letters){
		for (int idx_letter = 4; idx_letter < nb_letters; idx_letter+=4){
			int current_offset = readInt(buffer, idx_letter+2);
			if (current_offset > 0){
				if (current_offset+nb_letters*4+2> pos){
					current_offset = current_offset + bytes_added;
					std::vector<unsigned char> byte_Array = intToByteArray(current_offset);
					applyChange(idx_letter+2, byte_Array,  4, buffer);
				}
			}
		}
}
/*get_code2 finds the code used inside the font file of the character specified by SJIS encoding 
(or at least I believe it was intended to be SJIS)*/
int get_code2(int sjis){
	//F0A3
	int poids_fort = sjis >> 8 & 0xff;  //=> 0xF0
	int iVar4;
	int poids_faible = (sjis & 0xff); //=> 0xA3
	int iVar1;
	if (poids_fort < 0xc0) { // 0xF0 >= 0xC0
		iVar4 = -0x81;
	}
	else {
		iVar4 = -0xc1; //iVar4 = -0xC1
	}
	if (poids_faible < 0x80) { //0xA3>=0x80
		iVar1 = -0x40;
	}
	else {
		iVar1 = -0x41; //iVar1 = -0x41
		
	} 
	return (poids_fort + iVar4) * 0xbc + poids_faible + iVar1; //(poids_fort + iVar4) = F0 - C1 = 2F // poids_faible + iVar1 = 0xA3 - 0x41 = 0x62
	} //code for F0A3 = 22E6
/*get_sjis_from_number does the opposite of get_code2 (finds the sjis for the character specified by its code*/
int	get_sjis_from_number(int ID){ //22E6
	int sjis;
	int fortandIvar4 = ID/0xBC;  //2F
	int faibleandIvar1 = ID%0xBC;  //62
	
	int fort1 = fortandIvar4 + 0xC1; //F0
	int fort2 = fortandIvar4 + 0x81; //B0
	int fort; 
	if (fort1 < 0xC0) fort = fort2; // F0 < C0 ? => fort = B0
	else fort = fort1; // fort = F0
	int faible1 = faibleandIvar1 + 0x40; //62+40 = A2
	int faible2 = faibleandIvar1 + 0x41; // 62 + 41 = A3
	int faible; 
	if (faible1 >= 0x80) faible = faible2; // A2 >= 80 => faible = A3
	else faible = faible1; //A2 < 0x80 => faible = A2
	sjis = (fort << 8) + faible; //F0A3
	
	return sjis;
}
/*build_characters_list creates a list of characters to either replace or add inside the font
the replaced characters are hardcoded inside the function, and the added characters are
specified inside the config_font.ini file*/
void build_characters_list(std::vector<uint8_t> buffer_font_file){
	//here we're going to create a character list;
	//we'll get the utf32 from the symbol written in the file
	//A, B, C ... regular characters will be hardcoded here since they're hardcoded in the game executable
	bool success = true;
	chrs.push_back(Character(get_addr_char(get_code2(0x8140), buffer_font_file, success),0x0,0x8140));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x1,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x2,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x3,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x4,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x5,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x6,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x7,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xA,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xB,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xC,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xD,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x10,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x11,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x12,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x13,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x14,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x15,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x16,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x17,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x18,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x19,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x1A,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x1B,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A8), buffer_font_file, success),0x1C,0x81A8));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A9), buffer_font_file, success),0x1D,0x81A9));
	chrs.push_back(Character(get_addr_char(get_code2(0x81AA), buffer_font_file, success),0x1E,0x81AA));
	chrs.push_back(Character(get_addr_char(get_code2(0x81AB), buffer_font_file, success),0x1F,0x81AB));
	chrs.push_back(Character(get_addr_char(get_code2(0x8140), buffer_font_file, success),0x20,0x8140));
	chrs.push_back(Character(get_addr_char(get_code2(0x8149), buffer_font_file, success),0x21,0x8149));
	chrs.push_back(Character(get_addr_char(get_code2(0x8168), buffer_font_file, success),0x22,0x8168));
	chrs.push_back(Character(get_addr_char(get_code2(0x8194), buffer_font_file, success),0x23,0x8194));
	chrs.push_back(Character(get_addr_char(get_code2(0x8190), buffer_font_file, success),0x24,0x8190));
	chrs.push_back(Character(get_addr_char(get_code2(0x8193), buffer_font_file, success),0x25,0x8193));
	chrs.push_back(Character(get_addr_char(get_code2(0x8195), buffer_font_file, success),0x26,0x8195));
	chrs.push_back(Character(get_addr_char(get_code2(0x8166), buffer_font_file, success),0x27,0x8166));
	chrs.push_back(Character(get_addr_char(get_code2(0x8169), buffer_font_file, success),0x28,0x8169));
	chrs.push_back(Character(get_addr_char(get_code2(0x816A), buffer_font_file, success),0x29,0x816A));
	chrs.push_back(Character(get_addr_char(get_code2(0x8196), buffer_font_file, success),0x2A,0x8196));
	chrs.push_back(Character(get_addr_char(get_code2(0x817B), buffer_font_file, success),0x2B,0x817B));
	chrs.push_back(Character(get_addr_char(get_code2(0x8143), buffer_font_file, success),0x2C,0x8143));
	chrs.push_back(Character(get_addr_char(get_code2(0x817C), buffer_font_file, success),0x2D,0x817C));
	chrs.push_back(Character(get_addr_char(get_code2(0x8144), buffer_font_file, success),0x2E,0x8144));
	chrs.push_back(Character(get_addr_char(get_code2(0x815E), buffer_font_file, success),0x2F,0x815E));
	chrs.push_back(Character(get_addr_char(get_code2(0x824F), buffer_font_file, success),0x30,0x824F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8250), buffer_font_file, success),0x31,0x8250));
	chrs.push_back(Character(get_addr_char(get_code2(0x8251), buffer_font_file, success),0x32,0x8251));
	chrs.push_back(Character(get_addr_char(get_code2(0x8252), buffer_font_file, success),0x33,0x8252));
	chrs.push_back(Character(get_addr_char(get_code2(0x8253), buffer_font_file, success),0x34,0x8253));
	chrs.push_back(Character(get_addr_char(get_code2(0x8254), buffer_font_file, success),0x35,0x8254));
	chrs.push_back(Character(get_addr_char(get_code2(0x8255), buffer_font_file, success),0x36,0x8255));
	chrs.push_back(Character(get_addr_char(get_code2(0x8256), buffer_font_file, success),0x37,0x8256));
	chrs.push_back(Character(get_addr_char(get_code2(0x8257), buffer_font_file, success),0x38,0x8257));
	chrs.push_back(Character(get_addr_char(get_code2(0x8258), buffer_font_file, success),0x39,0x8258));
	chrs.push_back(Character(get_addr_char(get_code2(0x8146), buffer_font_file, success),0x3A,0x8146));
	chrs.push_back(Character(get_addr_char(get_code2(0x8147), buffer_font_file, success),0x3B,0x8147));
	chrs.push_back(Character(get_addr_char(get_code2(0x8183), buffer_font_file, success),0x3C,0x8183));
	chrs.push_back(Character(get_addr_char(get_code2(0x8181), buffer_font_file, success),0x3D,0x8181));
	chrs.push_back(Character(get_addr_char(get_code2(0x8184), buffer_font_file, success),0x3E,0x8184));
	chrs.push_back(Character(get_addr_char(get_code2(0x8148), buffer_font_file, success),0x3F,0x8148));
	chrs.push_back(Character(get_addr_char(get_code2(0x8197), buffer_font_file, success),0x40,0x8197));
	chrs.push_back(Character(get_addr_char(get_code2(0x8260), buffer_font_file, success),0x41,0x8260));
	chrs.push_back(Character(get_addr_char(get_code2(0x8261), buffer_font_file, success),0x42,0x8261));
	chrs.push_back(Character(get_addr_char(get_code2(0x8262), buffer_font_file, success),0x43,0x8262));
	chrs.push_back(Character(get_addr_char(get_code2(0x8263), buffer_font_file, success),0x44,0x8263));
	chrs.push_back(Character(get_addr_char(get_code2(0x8264), buffer_font_file, success),0x45,0x8264));
	chrs.push_back(Character(get_addr_char(get_code2(0x8265), buffer_font_file, success),0x46,0x8265));
	chrs.push_back(Character(get_addr_char(get_code2(0x8266), buffer_font_file, success),0x47,0x8266));
	chrs.push_back(Character(get_addr_char(get_code2(0x8267), buffer_font_file, success),0x48,0x8267));
	chrs.push_back(Character(get_addr_char(get_code2(0x8268), buffer_font_file, success),0x49,0x8268));
	chrs.push_back(Character(get_addr_char(get_code2(0x8269), buffer_font_file, success),0x4A,0x8269));
	chrs.push_back(Character(get_addr_char(get_code2(0x826A), buffer_font_file, success),0x4B,0x826A));
	chrs.push_back(Character(get_addr_char(get_code2(0x826B), buffer_font_file, success),0x4C,0x826B));
	chrs.push_back(Character(get_addr_char(get_code2(0x826C), buffer_font_file, success),0x4D,0x826C));
	chrs.push_back(Character(get_addr_char(get_code2(0x826D), buffer_font_file, success),0x4E,0x826D));
	chrs.push_back(Character(get_addr_char(get_code2(0x826E), buffer_font_file, success),0x4F,0x826E));
	chrs.push_back(Character(get_addr_char(get_code2(0x826F), buffer_font_file, success),0x50,0x826F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8270), buffer_font_file, success),0x51,0x8270));
	chrs.push_back(Character(get_addr_char(get_code2(0x8271), buffer_font_file, success),0x52,0x8271));
	chrs.push_back(Character(get_addr_char(get_code2(0x8272), buffer_font_file, success),0x53,0x8272));
	chrs.push_back(Character(get_addr_char(get_code2(0x8273), buffer_font_file, success),0x54,0x8273));
	chrs.push_back(Character(get_addr_char(get_code2(0x8274), buffer_font_file, success),0x55,0x8274));
	chrs.push_back(Character(get_addr_char(get_code2(0x8275), buffer_font_file, success),0x56,0x8275));
	chrs.push_back(Character(get_addr_char(get_code2(0x8276), buffer_font_file, success),0x57,0x8276));
	chrs.push_back(Character(get_addr_char(get_code2(0x8277), buffer_font_file, success),0x58,0x8277));
	chrs.push_back(Character(get_addr_char(get_code2(0x8278), buffer_font_file, success),0x59,0x8278));
	chrs.push_back(Character(get_addr_char(get_code2(0x8279), buffer_font_file, success),0x5A,0x8279));
	chrs.push_back(Character(get_addr_char(get_code2(0x816D), buffer_font_file, success),0x5B,0x816D));
	chrs.push_back(Character(get_addr_char(get_code2(0x818F), buffer_font_file, success),0x5C,0x818F));
	chrs.push_back(Character(get_addr_char(get_code2(0x816E), buffer_font_file, success),0x5D,0x816E));
	chrs.push_back(Character(get_addr_char(get_code2(0x814F), buffer_font_file, success),0x5E,0x814F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8151), buffer_font_file, success),0x5F,0x8151));
	chrs.push_back(Character(get_addr_char(get_code2(0x8140), buffer_font_file, success),0x60,0x8140));
	chrs.push_back(Character(get_addr_char(get_code2(0x8281), buffer_font_file, success),0x61,0x8281));
	chrs.push_back(Character(get_addr_char(get_code2(0x8282), buffer_font_file, success),0x62,0x8282));
	chrs.push_back(Character(get_addr_char(get_code2(0x8283), buffer_font_file, success),0x63,0x8283));
	chrs.push_back(Character(get_addr_char(get_code2(0x8284), buffer_font_file, success),0x64,0x8284));
	chrs.push_back(Character(get_addr_char(get_code2(0x8285), buffer_font_file, success),0x65,0x8285));
	chrs.push_back(Character(get_addr_char(get_code2(0x8286), buffer_font_file, success),0x66,0x8286));
	chrs.push_back(Character(get_addr_char(get_code2(0x8287), buffer_font_file, success),0x67,0x8287));
	chrs.push_back(Character(get_addr_char(get_code2(0x8288), buffer_font_file, success),0x68,0x8288));
	chrs.push_back(Character(get_addr_char(get_code2(0x8289), buffer_font_file, success),0x69,0x8289));
	chrs.push_back(Character(get_addr_char(get_code2(0x828A), buffer_font_file, success),0x6A,0x828A));
	chrs.push_back(Character(get_addr_char(get_code2(0x828B), buffer_font_file, success),0x6B,0x828B));
	chrs.push_back(Character(get_addr_char(get_code2(0x828C), buffer_font_file, success),0x6C,0x828C));
	chrs.push_back(Character(get_addr_char(get_code2(0x828D), buffer_font_file, success),0x6D,0x828D));
	chrs.push_back(Character(get_addr_char(get_code2(0x828E), buffer_font_file, success),0x6E,0x828E));
	chrs.push_back(Character(get_addr_char(get_code2(0x828F), buffer_font_file, success),0x6F,0x828F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8290), buffer_font_file, success),0x70,0x8290));
	chrs.push_back(Character(get_addr_char(get_code2(0x8291), buffer_font_file, success),0x71,0x8291));
	chrs.push_back(Character(get_addr_char(get_code2(0x8292), buffer_font_file, success),0x72,0x8292));
	chrs.push_back(Character(get_addr_char(get_code2(0x8293), buffer_font_file, success),0x73,0x8293));
	chrs.push_back(Character(get_addr_char(get_code2(0x8294), buffer_font_file, success),0x74,0x8294));
	chrs.push_back(Character(get_addr_char(get_code2(0x8295), buffer_font_file, success),0x75,0x8295));
	chrs.push_back(Character(get_addr_char(get_code2(0x8296), buffer_font_file, success),0x76,0x8296));
	chrs.push_back(Character(get_addr_char(get_code2(0x8297), buffer_font_file, success),0x77,0x8297));
	chrs.push_back(Character(get_addr_char(get_code2(0x8298), buffer_font_file, success),0x78,0x8298));
	chrs.push_back(Character(get_addr_char(get_code2(0x8299), buffer_font_file, success),0x79,0x8299));
	chrs.push_back(Character(get_addr_char(get_code2(0x829A), buffer_font_file, success),0x7A,0x829A));
	chrs.push_back(Character(get_addr_char(get_code2(0x816F), buffer_font_file, success),0x7B,0x816F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8162), buffer_font_file, success),0x7C,0x8162));
	chrs.push_back(Character(get_addr_char(get_code2(0x8170), buffer_font_file, success),0x7D,0x8170));
	chrs.push_back(Character(get_addr_char(get_code2(0x8160), buffer_font_file, success),0x7E,0x8160));
	chrs.push_back(Character(get_addr_char(get_code2(0x8140), buffer_font_file, success),0x7F,0x8140));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x80,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x81,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x82,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x83,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x84,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x85,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x86,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x87,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x88,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x89,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8A,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8B,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8C,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8D,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8E,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x8F,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x90,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x91,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x92,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x93,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x94,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x95,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x96,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x97,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x98,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x99,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9A,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9B,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9C,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9D,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9E,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0x9F,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x8140), buffer_font_file, success),0xA0,0x8140));
	chrs.push_back(Character(get_addr_char(get_code2(0x8142), buffer_font_file, success),0xA1,0x8142));
	chrs.push_back(Character(get_addr_char(get_code2(0x8175), buffer_font_file, success),0xA2,0x8175));
	chrs.push_back(Character(get_addr_char(get_code2(0x8176), buffer_font_file, success),0xA3,0x8176));
	chrs.push_back(Character(get_addr_char(get_code2(0x8141), buffer_font_file, success),0xA4,0x8141));
	chrs.push_back(Character(get_addr_char(get_code2(0x8145), buffer_font_file, success),0xA5,0x8145));
	chrs.push_back(Character(get_addr_char(get_code2(0x8392), buffer_font_file, success),0xA6,0x8392));
	chrs.push_back(Character(get_addr_char(get_code2(0x8340), buffer_font_file, success),0xA7,0x8340));
	chrs.push_back(Character(get_addr_char(get_code2(0x8342), buffer_font_file, success),0xA8,0x8342));
	chrs.push_back(Character(get_addr_char(get_code2(0x8344), buffer_font_file, success),0xA9,0x8344));
	chrs.push_back(Character(get_addr_char(get_code2(0x8346), buffer_font_file, success),0xAA,0x8346));
	chrs.push_back(Character(get_addr_char(get_code2(0x8348), buffer_font_file, success),0xAB,0x8348));
	chrs.push_back(Character(get_addr_char(get_code2(0x8383), buffer_font_file, success),0xAC,0x8383));
	chrs.push_back(Character(get_addr_char(get_code2(0x8385), buffer_font_file, success),0xAD,0x8385));
	chrs.push_back(Character(get_addr_char(get_code2(0x8387), buffer_font_file, success),0xAE,0x8387));
	chrs.push_back(Character(get_addr_char(get_code2(0x8362), buffer_font_file, success),0xAF,0x8362));
	chrs.push_back(Character(get_addr_char(get_code2(0x815B), buffer_font_file, success),0xB0,0x815B));
	chrs.push_back(Character(get_addr_char(get_code2(0x8341), buffer_font_file, success),0xB1,0x8341));
	chrs.push_back(Character(get_addr_char(get_code2(0x8343), buffer_font_file, success),0xB2,0x8343));
	chrs.push_back(Character(get_addr_char(get_code2(0x8345), buffer_font_file, success),0xB3,0x8345));
	chrs.push_back(Character(get_addr_char(get_code2(0x8347), buffer_font_file, success),0xB4,0x8347));
	chrs.push_back(Character(get_addr_char(get_code2(0x8349), buffer_font_file, success),0xB5,0x8349));
	chrs.push_back(Character(get_addr_char(get_code2(0x834A), buffer_font_file, success),0xB6,0x834A));
	chrs.push_back(Character(get_addr_char(get_code2(0x834C), buffer_font_file, success),0xB7,0x834C));
	chrs.push_back(Character(get_addr_char(get_code2(0x834E), buffer_font_file, success),0xB8,0x834E));
	chrs.push_back(Character(get_addr_char(get_code2(0x8350), buffer_font_file, success),0xB9,0x8350));
	chrs.push_back(Character(get_addr_char(get_code2(0x8352), buffer_font_file, success),0xBA,0x8352));
	chrs.push_back(Character(get_addr_char(get_code2(0x8354), buffer_font_file, success),0xBB,0x8354));
	chrs.push_back(Character(get_addr_char(get_code2(0x8356), buffer_font_file, success),0xBC,0x8356));
	chrs.push_back(Character(get_addr_char(get_code2(0x8358), buffer_font_file, success),0xBD,0x8358));
	chrs.push_back(Character(get_addr_char(get_code2(0x835A), buffer_font_file, success),0xBE,0x835A));
	chrs.push_back(Character(get_addr_char(get_code2(0x835C), buffer_font_file, success),0xBF,0x835C));
	chrs.push_back(Character(get_addr_char(get_code2(0x835E), buffer_font_file, success),0xC0,0x835E));
	chrs.push_back(Character(get_addr_char(get_code2(0x8360), buffer_font_file, success),0xC1,0x8360));
	chrs.push_back(Character(get_addr_char(get_code2(0x8363), buffer_font_file, success),0xC2,0x8363));
	chrs.push_back(Character(get_addr_char(get_code2(0x8365), buffer_font_file, success),0xC3,0x8365));
	chrs.push_back(Character(get_addr_char(get_code2(0x8367), buffer_font_file, success),0xC4,0x8367));
	chrs.push_back(Character(get_addr_char(get_code2(0x8369), buffer_font_file, success),0xC5,0x8369));
	chrs.push_back(Character(get_addr_char(get_code2(0x836A), buffer_font_file, success),0xC6,0x836A));
	chrs.push_back(Character(get_addr_char(get_code2(0x836B), buffer_font_file, success),0xC7,0x836B));
	chrs.push_back(Character(get_addr_char(get_code2(0x836C), buffer_font_file, success),0xC8,0x836C));
	chrs.push_back(Character(get_addr_char(get_code2(0x836D), buffer_font_file, success),0xC9,0x836D));
	chrs.push_back(Character(get_addr_char(get_code2(0x836E), buffer_font_file, success),0xCA,0x836E));
	chrs.push_back(Character(get_addr_char(get_code2(0x8371), buffer_font_file, success),0xCB,0x8371));
	chrs.push_back(Character(get_addr_char(get_code2(0x8374), buffer_font_file, success),0xCC,0x8374));
	chrs.push_back(Character(get_addr_char(get_code2(0x8377), buffer_font_file, success),0xCD,0x8377));
	chrs.push_back(Character(get_addr_char(get_code2(0x837A), buffer_font_file, success),0xCE,0x837A));
	chrs.push_back(Character(get_addr_char(get_code2(0x837D), buffer_font_file, success),0xCF,0x837D));
	chrs.push_back(Character(get_addr_char(get_code2(0x837E), buffer_font_file, success),0xD0,0x837E));
	chrs.push_back(Character(get_addr_char(get_code2(0x8380), buffer_font_file, success),0xD1,0x8380));
	chrs.push_back(Character(get_addr_char(get_code2(0x8381), buffer_font_file, success),0xD2,0x8381));
	chrs.push_back(Character(get_addr_char(get_code2(0x8382), buffer_font_file, success),0xD3,0x8382));
	chrs.push_back(Character(get_addr_char(get_code2(0x8384), buffer_font_file, success),0xD4,0x8384));
	chrs.push_back(Character(get_addr_char(get_code2(0x8386), buffer_font_file, success),0xD5,0x8386));
	chrs.push_back(Character(get_addr_char(get_code2(0x8388), buffer_font_file, success),0xD6,0x8388));
	chrs.push_back(Character(get_addr_char(get_code2(0x8389), buffer_font_file, success),0xD7,0x8389));
	chrs.push_back(Character(get_addr_char(get_code2(0x838A), buffer_font_file, success),0xD8,0x838A));
	chrs.push_back(Character(get_addr_char(get_code2(0x838B), buffer_font_file, success),0xD9,0x838B));
	chrs.push_back(Character(get_addr_char(get_code2(0x838C), buffer_font_file, success),0xDA,0x838C));
	chrs.push_back(Character(get_addr_char(get_code2(0x838D), buffer_font_file, success),0xDB,0x838D));
	chrs.push_back(Character(get_addr_char(get_code2(0x838F), buffer_font_file, success),0xDC,0x838F));
	chrs.push_back(Character(get_addr_char(get_code2(0x8393), buffer_font_file, success),0xDD,0x8393));
	chrs.push_back(Character(get_addr_char(get_code2(0x814A), buffer_font_file, success),0xDE,0x814A));
	chrs.push_back(Character(get_addr_char(get_code2(0x814B), buffer_font_file, success),0xDF,0x814B));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE0,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE1,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE2,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE3,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE4,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE5,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE6,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE7,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE8,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xE9,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xEA,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xEB,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xEC,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xED,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xEE,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xEF,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF0,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF1,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF2,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF3,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF4,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF5,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF6,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF7,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF8,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xF9,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFA,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFB,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFC,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFD,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFE,0x81A6));
	chrs.push_back(Character(get_addr_char(get_code2(0x81A6), buffer_font_file, success),0xFF,0x81A6));
	
	std::ifstream file("config_font.ini");
	std::string str;
	std::size_t idx = -1;
	while (std::getline(file, str)) {
        size_t idx;
        if ((idx = str.find("CharacterSize")) != std::string::npos) {
            std::string size_str = str.substr(str.find("=") + 1);
            size = std::stoi(size_str);
        } else if ((idx = str.find("MaxHeightFromOrigin")) != std::string::npos) {
            std::string height_str = str.substr(str.find("=") + 1);
            char_height = std::stoi(height_str);
        } else if ((idx = str.find("Font")) != std::string::npos) {
            font = str.substr(str.find("=") + 1);
        } 
		else if ((idx = str.find("SpaceWidth")) != std::string::npos) {
			std::string space_str = str.substr(str.find("=") + 1);
			space_width = std::stoi(space_str);
		}
		else if ((idx = str.find("Quiet")) != std::string::npos) {
            quiet = str.substr(str.find("=") + 1);
        } else {
            // We expect the .ini file to be encoded in UTF-8, but the FreeType Library needs UTF-32 codes
            // to find the drawing of the character, therefore we need the UTF-32 code.
            int code = 0;
            std::vector<unsigned int> utf32line;
            utf8::utf8to32(str.begin(), str.end(), std::back_inserter(utf32line));
            if (!utf32line.empty()) {
                code = utf32line[0];
                chrs.push_back(Character(-1, code, -1));
            }
        }
    }
	std::cout << "Character size: " << std::hex << size << std::endl;
	std::cout << "Max height from origin: " << std::hex << char_height << std::endl;
	std::cout << "Font chosen: " << std::hex << font << std::endl;
	std::cout << "List of characters built!" << std::endl;
	file.close();
	
}


/*create_bitmap creates a vector of bytes corresponding to the drawing of the chosen character,
written in the Ys I font format*/
std::vector<unsigned char> create_bitmap(FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y){
	int i,j,p,q;
	FT_Int  x_max = x + bitmap->width;
	FT_Int  y_max = y + bitmap->rows;
	int origin = y;
	int lowest_y =  bitmap->rows; 
	int highest_y = 0;
	uint8_t column_number = x_max - x; //1 here to put some space for the next char
	uint8_t line_number = y_max - y;
	
	uint8_t max_dim = column_number > line_number ? column_number : line_number;


	uint8_t base_line_y = char_height+1;
	uint8_t offset_y = base_line_y-y;
	uint8_t offset_before_next_chr = column_number+2;
	/*The following line is a policy to manage the spaces between characters, there is probably a smarter way to do it*/
	if (column_number<5) offset_before_next_chr = column_number+3;
	//we also prevent a negative y position
	if ((offset_y>0x7F)) offset_y = 0;
	std::vector<unsigned char> header = {};
	size_t col_number_in_bitmap = column_number;
	column_number = line_number;

	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	header.push_back(offset_before_next_chr);
	header.push_back(0);
	header.push_back(max_dim);
	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	header.push_back(offset_y);
	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	header.push_back(0);
	
	//first, we build the bitmap :
	std::vector<std::vector<unsigned char>> image(max_dim, std::vector<unsigned char>(max_dim, 0));
	//map is first initialized
	for ( j = 0; j< max_dim; j++){
	  for ( i = 0; i< max_dim; i++){
		  image[j][i] = 0;
		  
	  }
	}
	
	for (int j = origin < 0 ? 0 : origin; j < lowest_y; j++) {
		for (int i = 0; i < max_dim; i++) {

			if (!(i >= col_number_in_bitmap) )
			{
				if (j < max_dim) {
					image[j][i] = (((bitmap->buffer[j * col_number_in_bitmap + i]) & 0xF0) >> 4);
				}
			}
			
		}
	}

	// Iterate over rows from origin - 1 to highest_y (inclusive)
	for (int j = origin < 0 ? 0 : origin-1; j >= highest_y; j--) {
		for (int i = 0; i < max_dim; i++) {
			if (!(i >= col_number_in_bitmap))
			{
				if (j >= 0 && j < max_dim) {
					image[j][i] = (((bitmap->buffer[j * col_number_in_bitmap + i]) & 0xF0) >> 4);
				}
			}
		}
	}
	//display image :
	
	if (quiet == "false"){
		for ( j = 0; j< max_dim; j++){
			for ( i = 0; i< max_dim; i++){
				if (image[j][i]>0x0) printf("%x",image[j][i]);
				else printf(" ");
			  
		  }
		  printf("\n");
		}
	}
	//our bitmap is done, now we need to translate it to the font format.
	
	
	std::vector<unsigned char> total;
	std::vector<unsigned char> total_row_section;
	std::vector<unsigned char> total_info_section;
	int activate = 0;
	int offset = 0;
	int counter = 0;
	for (int idx_row = 0; idx_row < max_dim; idx_row++) {
		int idx_col = 0;
		int nb_subpack = 1;
		std::vector<unsigned char> total_row;

		while (idx_col < max_dim) {
			std::vector<unsigned char> subpack;

			// Skip empty pixels
			while (idx_col < max_dim && image[idx_row][idx_col] == 0) {
				idx_col++;
			}

			// Calculate number of empty pixels
			int nb_empty_pixels = idx_col - counter;
			subpack.push_back(static_cast<unsigned char>(nb_empty_pixels));

			if (idx_col == max_dim) {
				total_row.insert(total_row.end(), subpack.begin(), subpack.end());
				break;
			}
			else {
				nb_subpack += 2;
			}

			// Process non-empty pixels
			counter = idx_col;
			int activate = 0;
			unsigned char current_pixel_byte = 0;

			while (idx_col < max_dim && image[idx_row][idx_col] != 0) {
				if (activate == 1) {
					activate = 0;
					current_pixel_byte = current_pixel_byte + (image[idx_row][idx_col] << 4);
					subpack.push_back(static_cast<unsigned char>(current_pixel_byte));
				}
				else {
					activate = 1;
					current_pixel_byte = image[idx_row][idx_col];
				}
				idx_col++;
			}

			if (activate == 1) {
				subpack.push_back(static_cast<unsigned char>(current_pixel_byte));
			}

			// Insert number of pixels after empty pixels
			int nb_pixels = idx_col - counter;
			subpack.insert(subpack.begin() + 1, static_cast<unsigned char>(nb_pixels));

			counter = idx_col;
			total_row.insert(total_row.end(), subpack.begin(), subpack.end());
		}
		
		// Update total_info_section and total_row_section
		total_info_section.push_back(static_cast<unsigned char>(offset));
		total_info_section.push_back(0);
		total_info_section.push_back(static_cast<unsigned char>(nb_subpack));
		total_info_section.push_back(0);
		offset = offset + total_row.size();
		counter = 0;
		total_row_section.insert(total_row_section.end(), total_row.begin(), total_row.end());
	}
	total_row_section.push_back((unsigned char) 0x00);
	total_row_section.push_back((unsigned char) 0x00);
	total_row_section.push_back((unsigned char) 0x00);
	total_row_section.push_back((unsigned char) 0x00);
	
	
	total.insert(total.end(), header.begin(), header.end());
	
	total.insert(total.end(), total_info_section.begin(), total_info_section.end());
	total.insert(total.end(), total_row_section.begin(), total_row_section.end());
	
	return total;
}
/*draw_character calls create_bitmap for the character specified by UTF32 (character_code)*/
std::vector<unsigned char> draw_character(int character_code, int *length, int target_height, FT_Library library, FT_Face face, FT_GlyphSlot  slot){
	
    FT_Load_Glyph(face, character_code, FT_LOAD_RENDER);
	int distance_top_to_origin = slot->bitmap_top;
		

	std::vector<unsigned char> letter_drawing = create_bitmap(&slot->bitmap,
             slot->bitmap_left,
             slot->bitmap_top);

	return letter_drawing;
}

std::map<int, std::vector<unsigned char>> parseFontFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file) {
		throw std::runtime_error("Could not open file");
	}

	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	int num_chars = readInt(buffer, 0) & 0xFFFF;
	std::map<int, std::vector<unsigned char>> char_map;
	unsigned int start = 2 + (num_chars + 1) * 4;
	size_t tot = 0;
	for (int i = 0; i < num_chars; ++i) {
		int addr_offset = 2 + i * 4;
		int char_addr = readInt(buffer, addr_offset) - 4;

		size_t sz = calculateTotalSize(buffer, start + char_addr);// read_length(buffer, start + char_addr);
		tot = tot + sz;
		//std::cout << std::hex << tot << std::endl;

		std::vector<unsigned char> char_data(buffer.begin() + start + char_addr, buffer.begin() + start + char_addr + sz);
		char_data.push_back(0);
		char_data.push_back(0);
		char_data.push_back(0);
		char_data.push_back(0);
		int sjis_code = get_sjis_from_number(i);
		int index = get_code2(sjis_code);
		char_map[i] = char_data;
		
	}

	return char_map;
}

int main( int     argc,
      char**  argv )
{
	
	
	FT_Library    library;
	FT_Face       face;
	FT_GlyphSlot  slot;
	FT_Error      error;
	space_width = 10;

	std::string font_file(argv[1]);
	std::string base_filename = font_file.substr(font_file.find_last_of("/\\") + 1);
	
	std::string Output = std::filesystem::current_path().string()+"\\outputs\\";
	std::string full_output_path = Output+base_filename;
	std::string output_ini = Output+"\\text.ini";
	
	const char * Outputcstr = Output.c_str();
	const char * full_output_pathcstr = full_output_path.c_str();
	const char * output_inicstr = output_ini.c_str();
	
	std::ifstream original_font_file(argv[1], std::ios::in | std::ios::binary);
	std::vector<uint8_t> buffer_font_file((std::istreambuf_iterator<char>(original_font_file)), std::istreambuf_iterator<char>());
	original_font_file.close();
	build_characters_list(buffer_font_file);
	error = FT_Init_FreeType(&library);
	std::stringstream ss;
	std::map<int, std::vector<unsigned char>> char_map = parseFontFile(argv[1]);
	std::cout << "Successfully parsed " << char_map.size() << " characters." << std::endl;
	int successful_characters = 0, inserted_characters = 0;
	auto size_index_array_it = char_map.rbegin();

	// Get the highest key
	int size_index_array = char_map.size();
	int addr_end_index_array = 4 * size_index_array + 2;

	for (auto it = chrs.begin(); it != chrs.end(); it++) {
		
		bool success = true;

		error = FT_New_Face(library, font.c_str(), 0, &face);
		error = FT_Set_Char_Size(face, size * 64, 0, 44, 44);

		slot = face->glyph;
		int code = FT_Get_Char_Index(face, it->utf32);
		int index = get_code2(it->sjis);
		
		if (code != 0) {
			successful_characters++;
			if (char_map.count(index) == 0) { //The character is new, we need to add it to the font file
				
				inserted_characters++;
				int original_length = 0;
				int length;
				std::vector<unsigned char> letter = draw_character(code, &length, char_height, library, face, slot);
				
				addr_end_index_array = 4 * size_index_array + 2;
				char_map[size_index_array] = letter;
				it->sjis = get_sjis_from_number(size_index_array);
				ss << *it;
				size_index_array++;
				std::cout << std::hex << "it->sjis " << it->sjis << " " << code << std::endl;
			}
			else {
				int length;
				

				std::vector<unsigned char> letter = draw_character(code, &length, char_height, library, face, slot);
				/*for (auto b : letter) {
					std::cout << std::hex << int(b) << " ";
				}*/
				char_map[index] = letter;
			}
		}
		else {
			std::cout << "The TTF doesn't include this character" << std::endl;
		}
		FT_Done_Face(face);

	}

	if (char_map.count(get_code2(0x8140)))
		char_map[get_code2(0x8140)][4] = (unsigned char) space_width;

	// Create the output file
	std::vector<unsigned char> output_data;
	// Write size_index_array in little-endian format
	for (int i = 0; i < 2; ++i) {
		output_data.push_back(((size_index_array) >> (i * 8)) & 0xFF);
	}

	std::vector<unsigned char> addresses;
	std::vector<unsigned char> letters;
	int current_position = 0;
	int current_index_addr = 0;
	for (const auto& entry : char_map) {

		

		std::vector<unsigned char> byte_Array = intToByteArray(current_position);
		addresses.insert(addresses.end(), byte_Array.begin(), byte_Array.end());
		letters.insert(letters.end(), entry.second.begin(), entry.second.end());
		current_position += entry.second.size();
		
		current_index_addr = entry.first + 1;
	}

	// Write the letters
	output_data.insert(output_data.end(), addresses.begin(), addresses.end());
	output_data.insert(output_data.end(), letters.begin(), letters.end());

	// Write the output data to the file
	std::ofstream writeFontFile;
	writeFontFile.open(full_output_pathcstr, std::ios::out | std::ios::binary);
	writeFontFile.write((const char*)&output_data[0], output_data.size());
	writeFontFile.close();

	// Write the INI file
	std::ofstream writeIniFile;
	writeIniFile.open(output_inicstr, std::ios::out);
	writeIniFile << ss.rdbuf();
	writeIniFile.close();

	std::cout << "Successfully wrote the output files." << std::endl;
	return 0;


}
