#include <iostream>
#include <fstream>
#include <iomanip>

void encodeFileToHex(const std::string& filename, const std::string& name) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Fail to open: " << filename << std::endl;
        return;
    }

    std::cout << "const unsigned char letter_image_" << name << "[] = { ";

    unsigned char byte;
    bool first = true;
    while (file.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        if (! first) {
            std::cout << ", ";
        } else {
            first = false;
        }
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    std::cout << " };" << std::endl;

    file.close();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: filename varname" << std::endl;
        return 1;
    }
    encodeFileToHex(argv[1], argv[2]);
    return 0;
}
