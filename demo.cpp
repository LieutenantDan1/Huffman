#include <chrono>
#include <fstream>
#include "huffman.hpp"
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    std::string input_filename = "", output_filename = "";
    int expect = 0; // 0: nothing, 1: input_filename, 2: output_filename
    int operation = 0; // 0: none, 1: encode, 2: decode
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-i")
            expect = 1;
        else if (arg == "-o")
            expect = 2;
        else if (arg == "-e" || arg == "--encode")
            operation = 1;
        else if (arg == "-d" || arg == "--decode")
            operation = 2;
        else
        {
            switch (expect)
            {
                case 1:
                    input_filename = arg;
                    break;
                case 2:
                    output_filename = arg;
                    break;
                default:
                    break;
            }
            expect = 0;
        }
    }
    if (operation == 0)
    {
        std::cout << "Error: no operation specified.\n";
        return 1;
    }
    else if (operation == 1)
    {
        if (output_filename == "")
        {
            std::cout << "Error: no output file specified.\n";
            return 1;
        }
        std::ifstream input_file(input_filename, std::ios::binary | std::ios::ate);
        if (!input_file)
        {
            std::cout << "Error: could not open " << input_filename << ".\n";
            return 1;
        }
        auto then = std::chrono::steady_clock::now();
        std::vector<char> input_data(input_file.tellg());
        input_file.seekg(std::ios::beg);
        input_file.read(input_data.data(), input_data.size());
        input_file.close();
        std::vector<bool> output_data = hf::encode(input_data);
        std::ofstream out_file(output_filename, std::ios::binary);
        hf::write_data(output_data, out_file);
        out_file.close();
        auto now = std::chrono::steady_clock::now();
        float time = std::chrono::duration<float>(now - then).count();
        size_t in_bits = input_data.size() * 8;
        size_t out_bits = output_data.size();
        float ratio = (float)out_bits / (float)in_bits;
        std::cout << "Successfully " << (ratio >= 1.0f ? "\"" : "") << "compressed" 
        << (ratio >= 1.0f ? "\" " : " ") << in_bits << " bits to "
        << out_bits << " bits (" << (ratio * 100.0f) << "%) (in "
        << time << " s).\n";
        if (ratio >= 0.95f)
        {
            std::cout << "Warning: dataset is either small or incompressible.\n";
        }
    }
    else if (operation == 2)
    {
        std::ifstream input_file(input_filename, std::ios::binary);
        if (!input_file)
        {
            std::cout << "Error: could not open " << input_filename << ".\n";
            return 1;
        }
        auto then = std::chrono::steady_clock::now();
        std::vector<bool> input_data;
        try
        {
            input_data = hf::read_data(input_file);
        }
        catch (std::runtime_error& e)
        {
            std::cout << "Error: failed to read input data (" << e.what() << ").\n";
            return 1;
        }
        input_file.close();
        std::vector<char> output_data = hf::decode<char>(input_data);
        if (output_filename == "")
        {
            std::cout << std::string(output_data.begin(), output_data.end()) << "\n";
        }
        else
        {
            std::ofstream out_file(output_filename, std::ios::binary);
            out_file.write(output_data.data(), output_data.size());
            out_file.close();
            size_t in_bits = input_data.size();
            size_t out_bits = output_data.size() * 8;
            float ratio = (float)out_bits / (float)in_bits;
            auto now = std::chrono::steady_clock::now();
            float time = std::chrono::duration<float>(now - then).count();
            std::cout << "Successfully decompressed " << in_bits << " bits to "
            << out_bits << " bits (" << (ratio * 100.0f) << "%) (in "
            << time << " s).\n";
        }
    }
}