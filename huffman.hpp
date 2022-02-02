#pragma once
#include <bit>
#include <climits>
#include <fstream>
#include <list>
#include <map>
#include <vector>

constexpr bool little_endian = std::endian::native == std::endian::little;
static_assert(CHAR_BIT == 8, "Char must be exactly 8 bits wide.");

namespace hf
{
/*
Convert value of type T into byte vector (little endian output format).
*/
template <typename T>
std::vector<uint8_t> to_bytes(const T& value)
{
    std::vector<uint8_t> out;
    if (little_endian)
    {
        for (size_t i = 0; i < sizeof(T); ++i)
            out.push_back(((const uint8_t*)&value)[i]);
    }
    else
    {
        for (size_t i = sizeof(T); i--;)
            out.push_back(((const uint8_t*)&value)[i]);
    }
    return out;
}

/*
Convert byte vector into value of type T (little endian input format).
Start of data is specified by start parameter.
Throws std::runtime_error if input data does not have enough bytes.
*/
template <typename T>
T from_bytes(const std::vector<uint8_t>& data, size_t start = 0)
{
    if (data.size() - start < sizeof(T))
        throw std::runtime_error("from_bytes: input data too small.");
    T out = T();
    if (little_endian)
    {
        for (size_t i = 0; i < sizeof(T); ++i)
            ((uint8_t*)&out)[i] = data[start + i];
    }
    else
    {
        for (size_t i = sizeof(T); i--;)
            ((uint8_t*)&out)[i] = data[start + i];
    }
    return out;
}

/*
Convert value of type T into bit vector (little endian output format).
*/
template <typename T>
std::vector<bool> to_bits(const T& value)
{
    std::vector<bool> out;
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        uint8_t byte = little_endian ? ((uint8_t*)&value)[i]
            : ((uint8_t*)&value)[sizeof(T) - i - 1];
        for (size_t j = 0; j < 8; ++j)
            out.push_back((byte >> (7 - j)) & 1);
    }
    return out;
}

/*
Convert bit vector into value of type T (little endian input format).
Start of data is specified by start parameter.
Throws std::runtime_error if input data does not have enough bits.
*/
template <typename T>
T from_bits(const std::vector<bool>& data, size_t start = 0)
{
    if (data.size() - start < sizeof(T) * 8)
        throw std::runtime_error("from_bits: input data too small.");
    T out = T();
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        uint8_t& byte = little_endian ? ((uint8_t*)&out)[i]
            : ((uint8_t*)&out)[sizeof(T) - i - 1];
        for (size_t j = 0; j < 8; ++j)
        {
            uint8_t bit = 0x80 >> j;
            if (data[start++])
                byte |= bit;
            else
                byte &= ~bit;
        }
    }
    return out;
}

/*
Write bit vector to out_file (open with std::ios::binary!).
Format:
    data size:  data size in bits (64 bit unsigned integer)
    data:       data (padded with 0s if size not divisible by 8)
The file is NOT closed automatically.
*/
void write_data(const std::vector<bool>& data, std::ofstream& out_file)
{
    std::vector<uint8_t> size = to_bytes<uint64_t>(data.size());
    out_file.write((char*)size.data(), sizeof(uint64_t)); // lol
    uint8_t byte = 0;
    bool pad = false;
    for (size_t i = 0; i < data.size(); ++i)
    {
        pad = true;
        int bit = i % 8;
        if (data[i])
            byte |= (0x80 >> bit);
        if (bit == 7)
        {
            out_file.write((char*)&byte, 1);
            byte = 0;
            pad = false;
        }
    }
    if (pad)
        out_file.write((char*)&byte, 1);
}

/*
Read bit vector from in_file (open with std::ios::binary!).
Format: see write_data.
The file is NOT closed automatically.
*/
std::vector<bool> read_data(std::ifstream& in_file)
{
    std::vector<bool> out;
    std::vector<uint8_t> size_bytes(sizeof(uint64_t));
    in_file.read((char*)size_bytes.data(), sizeof(uint64_t));
    if (in_file.eof())
        throw std::runtime_error("read_data: premature end of file.");
    size_t size = from_bytes<uint64_t>(size_bytes);
    uint8_t byte = 0;
    for (size_t i = 0; i < size; ++i)
    {
        int bit = i % 8;
        if (bit == 0)
        {
            in_file.read((char*)&byte, 1);
            if (in_file.eof())
                throw std::runtime_error("read_data: premature end of file.");
        }
        out.push_back(byte & (0x80 >> (i % 8)));
    }
    return out;
}

/*
Encode a vector<T> of data using Huffman coding.
The output is a bit vector where the Huffman tree is stored first, then
the Huffman encoded data.
*/
template <typename T>
std::vector<bool> encode(const std::vector<T>& data)
{
    struct Node
    {
        bool end;
        std::vector<T> values;
        size_t freq;
        std::pair<Node*, Node*> sub_nodes;
        std::vector<bool> encode_tree()
        {
            std::vector<bool> out;
            if (end)
            {
                out.push_back(true);
                std::vector<bool> bits = to_bits(values[0]);
                for (bool b: bits)
                {
                    out.push_back(b);
                }
            }
            else
            {
                out.push_back(false);
                std::vector<bool> bits_l = sub_nodes.first->encode_tree();
                std::vector<bool> bits_r = sub_nodes.second->encode_tree();
                out.insert(out.end(), bits_l.begin(), bits_l.end());
                out.insert(out.end(), bits_r.begin(), bits_r.end());
            }
            return out;
        }
        Node(T value, size_t freq):
        end(true), values({value}), freq(freq)
        {}
        Node(Node* l, Node* r):
        end(false), freq(l->freq + r->freq), sub_nodes(std::pair<Node*, Node*>(l, r))
        {
            values.insert(values.end(), l->values.begin(), l->values.end());
            values.insert(values.end(), r->values.begin(), r->values.end());
        }
        ~Node()
        {
            if (!end)
            {
                delete sub_nodes.first;
                delete sub_nodes.second;
            }
        }
    };
    std::map<T, size_t> freq_table;
    for (size_t i = 0; i < data.size(); ++i)
    {
        freq_table[data[i]] += 1;
    }
    std::list<Node*> nodes(freq_table.size());
    auto nit = nodes.begin();
    for (auto& it: freq_table)
    {
        *nit = new Node(it.first, it.second);
        ++nit;
    }
    freq_table.clear();
    nodes.sort([] (const Node* l, const Node* r) 
    {
        return l->freq > r->freq;
    });
    std::map<T, std::vector<bool>> encoding_lut;
    while (true)
    {
        auto last = std::prev(nodes.end());
        if (last == nodes.begin())
            break;
        Node* l = *last;
        Node* r = *std::prev(last);
        nodes.erase(std::prev(last));
        nodes.erase(last);
        for (T value: l->values)
            encoding_lut[value].push_back(false);
        for (T value: r->values)
            encoding_lut[value].push_back(true);
        Node* parent = new Node(l, r);
        for (auto it = nodes.begin(); ; ++it)
        {
            if (it == nodes.end() || (*it)->freq <= parent->freq)
            {
                nodes.insert(it, parent);
                break;
            }
        }
    }
    Node* tree = *nodes.begin();
    nodes.clear();
    std::vector<bool> out = tree->encode_tree();
    for (size_t i = 0; i < data.size(); ++i)
    {
        std::vector<bool>& sequence = encoding_lut[data[i]];
        for (size_t j = sequence.size(); j--;)
        {
            out.push_back(sequence[j]);
        }
    }
    delete tree;
    return out;
}

/*
Output a vector<T>, decoded from Huffman encoded data.
*/
template <typename T>
std::vector<T> decode(const std::vector<bool>& data)
{
    struct Node
    {
        bool end;
        std::vector<T> values;
        std::pair<Node*, Node*> sub_nodes;
        Node(const std::vector<bool>& data, size_t& start)
        {
            end = data[start++];
            if (end)
            {
                values.push_back(from_bits<T>(data, start));
                start += sizeof(T) * 8;
            }
            else
            {
                sub_nodes.first = new Node(data, start);
                sub_nodes.second = new Node(data, start);
            }
        }
        ~Node()
        {
            if (!end)
            {
                delete sub_nodes.first;
                delete sub_nodes.second;
            }
        }
    };
    size_t start = 0;
    Node* tree = new Node(data, start);
    std::vector<T> out;
    Node* current = tree;
    for (size_t i = start; i < data.size(); ++i)
    {
        if (data[i])
            current = current->sub_nodes.second;
        else
            current = current->sub_nodes.first;
        if (current->end)
        {
            out.push_back(current->values[0]);
            current = tree;
        }
    }
    delete tree;
    return out;
}
}