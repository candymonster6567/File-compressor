/*
 * ============================================================
 *  HUFFMAN CODING — Text File Compressor & Decompressor
 *  A complete implementation using Binary Trees & Priority Queues
 * ============================================================
 *
 *  COMPILE:  g++ -std=c++17 -O2 -o huffman huffman.cpp
 *
 *  USAGE:
 *    Compress:    ./huffman compress input.txt output.huf
 *    Decompress:  ./huffman decompress output.huf restored.txt
 *
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <string>
#include <bitset>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
using namespace std;

// ─────────────────────────────────────────────────────────────
//  STEP 1 — The Node of our Huffman Binary Tree
// ─────────────────────────────────────────────────────────────
struct HuffmanNode {
    char     character;   // The actual character (only in leaf nodes)
    int      frequency;   // How often this character appears
    HuffmanNode* left;    // Left child  → represents bit '0'
    HuffmanNode* right;   // Right child → represents bit '1'

    // Constructor for leaf nodes (real characters)
    HuffmanNode(char ch, int freq)
        : character(ch), frequency(freq), left(nullptr), right(nullptr) {}

    // Constructor for internal nodes (combined frequency, no real character)
    HuffmanNode(int freq, HuffmanNode* l, HuffmanNode* r)
        : character('\0'), frequency(freq), left(l), right(r) {}
};

// ─────────────────────────────────────────────────────────────
//  Custom comparator for the Priority Queue (min-heap)
//  Nodes with LOWER frequency get popped FIRST.
//  This is the core of Huffman's greedy algorithm.
// ─────────────────────────────────────────────────────────────
struct CompareNodes {
    bool operator()(HuffmanNode* a, HuffmanNode* b) {
        return a->frequency > b->frequency; // min-heap: smaller freq = higher priority
    }
};

// ─────────────────────────────────────────────────────────────
//  Recursively free the tree to avoid memory leaks
// ─────────────────────────────────────────────────────────────
void deleteTree(HuffmanNode* node) {
    if (!node) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

// ─────────────────────────────────────────────────────────────
//  STEP 2 — Count character frequencies
// ─────────────────────────────────────────────────────────────
 unordered_map<char, int> buildFrequencyTable(const  string& text) {
     unordered_map<char, int> freq;
    for (char c : text) {
        freq[c]++;
    }
    return freq;
}

// ─────────────────────────────────────────────────────────────
//  STEP 3 — Build the Huffman Tree
//
//  Algorithm:
//    1. Put every character into the min-heap as a leaf node
//    2. Repeatedly pop the 2 nodes with smallest frequency
//    3. Merge them into a new internal node (sum of frequencies)
//    4. Push the merged node back into the heap
//    5. Repeat until only 1 node remains → that's the root
// ─────────────────────────────────────────────────────────────
HuffmanNode* buildHuffmanTree(const  unordered_map<char, int>& freqTable) {
     priority_queue<HuffmanNode*,  vector<HuffmanNode*>, CompareNodes> minHeap;

    // Create a leaf node for each unique character
    for (auto& [ch, freq] : freqTable) {
        minHeap.push(new HuffmanNode(ch, freq));
    }

    // Edge case: only one unique character
    if (minHeap.size() == 1) {
        HuffmanNode* only = minHeap.top(); minHeap.pop();
        HuffmanNode* root = new HuffmanNode(only->frequency, only, nullptr);
        return root;
    }

    // Merge until one tree remains
    while (minHeap.size() > 1) {
        HuffmanNode* left  = minHeap.top(); minHeap.pop();
        HuffmanNode* right = minHeap.top(); minHeap.pop();

        HuffmanNode* merged = new HuffmanNode(
            left->frequency + right->frequency,
            left,
            right
        );
        minHeap.push(merged);
    }

    return minHeap.top();
}

// ─────────────────────────────────────────────────────────────
//  STEP 4 — Generate Huffman Codes
//
//  Traverse the tree recursively:
//    Going LEFT  → append '0' to the code
//    Going RIGHT → append '1' to the code
//    At a LEAF   → we've found the code for that character
// ─────────────────────────────────────────────────────────────
void generateCodes(HuffmanNode* node,
                   const string& currentCode,
                    unordered_map<char,  string>& codeTable)
{
    if (!node) return;

    // Leaf node: this character's code is complete
    if (!node->left && !node->right) {
        codeTable[node->character] = currentCode.empty() ? "0" : currentCode;
        return;
    }

    generateCodes(node->left,  currentCode + "0", codeTable);
    generateCodes(node->right, currentCode + "1", codeTable);
}

// ─────────────────────────────────────────────────────────────
//  Helper: Serialize the Huffman Tree into the compressed file
//
//  We use a pre-order traversal and write:
//    '1' + the character  → for leaf nodes
//    '0'                  → for internal nodes
//
//  This lets us perfectly reconstruct the tree during decompression.
// ─────────────────────────────────────────────────────────────
void serializeTree(HuffmanNode* node,  string& output) {
    if (!node) return;

    if (!node->left && !node->right) {
        output += '1';
        output += node->character;
    } else {
        output += '0';
        serializeTree(node->left,  output);
        serializeTree(node->right, output);
    }
}

// ─────────────────────────────────────────────────────────────
//  Helper: Reconstruct the Huffman Tree from serialized data
// ─────────────────────────────────────────────────────────────
HuffmanNode* deserializeTree(const  string& data, size_t& index) {
    if (index >= data.size()) return nullptr;

    char marker = data[index++];

    if (marker == '1') {
        // Leaf node
        char ch = data[index++];
        return new HuffmanNode(ch, 0);
    } else {
        // Internal node
        HuffmanNode* left  = deserializeTree(data, index);
        HuffmanNode* right = deserializeTree(data, index);
        return new HuffmanNode(0, left, right);
    }
}

// ─────────────────────────────────────────────────────────────
//  STEP 5 — Write compressed bits to file
//
//  Bits are packed 8 per byte. The last byte may be padded
//  with zeros, so we store the padding count in the header.
// ─────────────────────────────────────────────────────────────
void writeBits( ofstream& out, const  string& bitString) {
    // Pad to a multiple of 8
    int padding = (8 - (bitString.size() % 8)) % 8;
     string padded = bitString +  string(padding, '0');

    // Write padding count as first byte
    out.put(static_cast<char>(padding));

    // Pack bits into bytes and write
    for (size_t i = 0; i < padded.size(); i += 8) {
         bitset<8> byte(padded.substr(i, 8));
        out.put(static_cast<char>(byte.to_ulong()));
    }
}

// ─────────────────────────────────────────────────────────────
//  Read compressed bits back into a bit-string
// ─────────────────────────────────────────────────────────────
 string readBits( ifstream& in) {
    int padding = static_cast<unsigned char>(in.get());
     string bits;

    char byte;
     string prevBits;

    // Read byte-by-byte, building the bit string
    while (in.get(byte)) {
         bitset<8> b(static_cast<unsigned char>(byte));
        prevBits = bits;
        bits += b.to_string();
    }

    // Remove padding from the last byte
    if (padding > 0 && bits.size() >= static_cast<size_t>(padding)) {
        bits = bits.substr(0, bits.size() - padding);
    }

    return bits;
}

// ─────────────────────────────────────────────────────────────
//  Print a beautiful summary of the compression
// ─────────────────────────────────────────────────────────────
void printCompressionStats(const  string& text,
                           const  unordered_map<char,  string>& codes,
                           size_t originalBytes, size_t compressedBytes)
{
     cout << "\n";
     cout << "┌─────────────────────────────────────────────┐\n";
     cout << "│          HUFFMAN COMPRESSION REPORT         │\n";
     cout << "├─────────────────────────────────────────────┤\n";
     cout << "│ CHARACTER CODES (sorted by frequency):      │\n";
     cout << "├───────┬───────────┬─────────────────────────┤\n";
     cout << "│ Char  │   Freq    │  Huffman Code           │\n";
     cout << "├───────┼───────────┼─────────────────────────┤\n";

    // Collect and sort by frequency descending
     unordered_map<char, int> freq;
    for (char c : text) freq[c]++;
     vector< pair<char, int>> sorted(freq.begin(), freq.end());
     sort(sorted.begin(), sorted.end(),
              [](auto& a, auto& b){ return a.second > b.second; });

    for (auto& [ch, f] : sorted) {
         string display;
        if      (ch == ' ')  display = "SPACE";
        else if (ch == '\n') display = "\\n";
        else if (ch == '\t') display = "\\t";
        else                 display =  string(1, ch);

         cout << "│  "   <<  left <<  setw(5) << display
                  << "│  "   <<  setw(9) << f
                  << "│  "   <<  setw(22) << codes.at(ch) << " │\n";
    }

    double ratio = 100.0 * (1.0 - (double)compressedBytes / originalBytes);
     cout << "├───────┴───────────┴─────────────────────────┤\n";
     cout << "│ Original size:   " <<  setw(6) << originalBytes   << " bytes               │\n";
     cout << "│ Compressed size: " <<  setw(6) << compressedBytes << " bytes               │\n";
     cout << "│ Space saved:     " <<  fixed <<  setprecision(1)
              <<  setw(5) << ratio << "%                     │\n";
     cout << "└─────────────────────────────────────────────┘\n\n";
}

// ─────────────────────────────────────────────────────────────
//  COMPRESS — main entry point
// ─────────────────────────────────────────────────────────────
void compress(const  string& inputPath, const  string& outputPath) {
    // 1. Read input file
     ifstream in(inputPath,  ios::binary);
    if (!in) throw  runtime_error("Cannot open input file: " + inputPath);

    string text(( istreambuf_iterator<char>(in)),
                       istreambuf_iterator<char>());
    in.close();

    if (text.empty()) throw  runtime_error("Input file is empty.");

     cout << "✓ Read " << text.size() << " bytes from '" << inputPath << "'\n";

    // 2. Build frequency table
    auto freqTable = buildFrequencyTable(text);
     cout << "✓ Found " << freqTable.size() << " unique characters\n";

    // 3. Build Huffman tree
    HuffmanNode* root = buildHuffmanTree(freqTable);
     cout << "✓ Built Huffman tree\n";

    // 4. Generate codes
    unordered_map<char,  string> codeTable;
    generateCodes(root, "", codeTable);
     cout << "✓ Generated Huffman codes\n";

    // 5. Encode text as bit string
    string encoded;
    encoded.reserve(text.size() * 4); // rough estimate
    for (char c : text) {
        encoded += codeTable[c];
    }

    // 6. Serialize the tree so we can reconstruct it during decompression
    string serializedTree;
    serializeTree(root, serializedTree);
    deleteTree(root);

    // 7. Write compressed file
    //    Format: [4-byte tree length][tree bytes][padding+data bits]
     ofstream out(outputPath,  ios::binary);
    if (!out) throw  runtime_error("Cannot open output file: " + outputPath);

    // Write magic header
    out.write("HUF1", 4);

    // Write tree length (4 bytes, big-endian)
    uint32_t treeLen = static_cast<uint32_t>(serializedTree.size());
    out.put((treeLen >> 24) & 0xFF);
    out.put((treeLen >> 16) & 0xFF);
    out.put((treeLen >>  8) & 0xFF);
    out.put((treeLen      ) & 0xFF);

    // Write serialized tree
    out.write(serializedTree.data(), serializedTree.size());

    // Write compressed bits
    writeBits(out, encoded);
    out.close();

    // 8. Stats
    size_t originalBytes   = text.size();
    size_t compressedBytes = 4 + 4 + serializedTree.size() + 1 + (encoded.size() + 7) / 8;
    printCompressionStats(text, codeTable, originalBytes, compressedBytes);

     cout << "✓ Compressed file written to '" << outputPath << "'\n\n";
}

// ─────────────────────────────────────────────────────────────
//  DECOMPRESS — main entry point
// ─────────────────────────────────────────────────────────────
void decompress(const  string& inputPath, const  string& outputPath) {
     ifstream in(inputPath,  ios::binary);
    if (!in) throw  runtime_error("Cannot open compressed file: " + inputPath);

    // Verify magic header
    char magic[5] = {};
    in.read(magic, 4);
    if ( string(magic, 4) != "HUF1") {
        throw  runtime_error("Not a valid Huffman compressed file (bad magic header).");
    }

    // Read tree length
    uint32_t treeLen = 0;
    treeLen |= (static_cast<unsigned char>(in.get()) << 24);
    treeLen |= (static_cast<unsigned char>(in.get()) << 16);
    treeLen |= (static_cast<unsigned char>(in.get()) <<  8);
    treeLen |= (static_cast<unsigned char>(in.get())      );

    // Read serialized tree
     string serializedTree(treeLen, '\0');
    in.read(serializedTree.data(), treeLen);

    // Reconstruct the tree
    size_t idx = 0;
    HuffmanNode* root = deserializeTree(serializedTree, idx);
     cout << "✓ Reconstructed Huffman tree from file\n";

    // Read and decode bits
     string bits = readBits(in);
    in.close();

    // Decode bit string using the tree
     string decoded;
    HuffmanNode* current = root;

    for (char bit : bits) {
        if (bit == '0') current = current->left;
        else            current = current->right;

        // Handle single-character edge case (tree has no children)
        if (!current) break;

        if (!current->left && !current->right) {
            decoded += current->character;
            current = root;
        }
    }

    deleteTree(root);

    // Write decoded text
     ofstream out(outputPath,  ios::binary);
    if (!out) throw  runtime_error("Cannot open output file: " + outputPath);
    out.write(decoded.data(), decoded.size());
    out.close();

     cout << "✓ Decompressed " << decoded.size() << " bytes to '" << outputPath << "'\n\n";
}

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
     cout << "\n";
     cout << "  ██╗  ██╗██╗   ██╗███████╗███████╗███╗   ███╗ █████╗ ███╗   ██╗\n";
     cout << "  ██║  ██║██║   ██║██╔════╝██╔════╝████╗ ████║██╔══██╗████╗  ██║\n";
     cout << "  ███████║██║   ██║█████╗  █████╗  ██╔████╔██║███████║██╔██╗ ██║\n";
     cout << "  ██╔══██║██║   ██║██╔══╝  ██╔══╝  ██║╚██╔╝██║██╔══██║██║╚██╗██║\n";
     cout << "  ██║  ██║╚██████╔╝██║     ██║     ██║ ╚═╝ ██║██║  ██║██║ ╚████║\n";
     cout << "  ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝     ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝\n";
     cout << "                     C O D I N G   C O M P R E S S O R\n\n";

    if (argc != 4) {
         cerr << "Usage:\n";
         cerr << "  Compress:    " << argv[0] << " compress   <input.txt>  <output.huf>\n";
         cerr << "  Decompress:  " << argv[0] << " decompress <input.huf>  <output.txt>\n\n";
        return 1;
    }

     string mode  = argv[1];
     string input = argv[2];
     string output= argv[3];

    try {
        if (mode == "compress") {
            compress(input, output);
        } else if (mode == "decompress") {
            decompress(input, output);
        } else {
            throw  runtime_error("Unknown mode '" + mode + "'. Use 'compress' or 'decompress'.");
        }
    } catch (const  exception& e) {
         cerr << "\n❌ Error: " << e.what() << "\n\n";
        return 1;
    }

    return 0;
}
