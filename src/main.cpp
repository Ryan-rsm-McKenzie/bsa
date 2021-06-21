#include "bsa/tes4.hpp"

using namespace std::literals;

int main()
{
	bsa::tes4::archive bsa;
	bsa.read(u8R"(E:\Games\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\Skyrim - Voices_en0.bsa)"sv);
	//bsa.write(u8"out.bsa", bsa::tes4::version::sse);
}
