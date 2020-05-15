//
// Created by ready on 12.05.2020.
//


#include "HuffmanArchiver.h"

#define DEBUG

#ifdef DEBUG
#include "Debug.h"
#endif

int HuffmanCompress(const Vector<string> &filesNames,
                    const string         &archiveName) {
    assert(filesNames.getSize() > 0);

    //Stream to associate with archive file
    ofstream archive(archiveName, ios::out | ios::binary);
    if (!archive.is_open()) {
        cerr << "Can't create an archive with the name: "
             << archiveName << endl;
        return 1;
    }

    //Create an info file to save the names of files for compressing
    ofstream infoFile(INFO_FILE_NAME, ios::out | ios::binary);
    if (!archive.is_open()) {
        cerr << "Can't create an info file the name: "
             << INFO_FILE_NAME << endl;
        return 1;
    }
    WriteFileNames(infoFile, filesNames);

    infoFile.close();

    Vector<string> allTheFilesForCompressingNames(filesNames);
    //The first file for compressing is always the info file
    allTheFilesForCompressingNames.pushFront(INFO_FILE_NAME);

    //Streams to associate with each of the files for compressing
    Vector<ifstream> fileStreams(allTheFilesForCompressingNames.getSize());
    //Sizes of the files for compressing
    Vector<MY_SIZE_T> fileSizes(fileStreams.getSize(), 0);

    // Frequency table for all ASCII chars in all the files for compressing
    //Array's indices correspond to ASCII code of each letter
    Vector<MY_SIZE_T>lettersFreqs(ALPHABET_SIZE, 0);

    // For all the files for compressing
    for(size_t i = 0; i < fileStreams.getSize(); i++) {
        // Open the current file
        fileStreams[i].open(allTheFilesForCompressingNames[i],
                            ios::in | ios::binary);

        if (!fileStreams[i].is_open()) {
            cerr << "Can't open file to compress: "
                 << allTheFilesForCompressingNames[i] << endl;
            return 1;
        }
        //Update the frequency table with the current file's statistics
        fileSizes[i] = UpdateLettersFreqs(fileStreams[i], lettersFreqs);
    }

#ifdef DEBUG
    cout << "Files:" << endl;
    for(size_t i = 0; i < allTheFilesForCompressingNames.getSize(); ++i) {
        cout << allTheFilesForCompressingNames[i]
             << " : "
             << fileSizes[i]
             << endl;

    }
    printForLetters(lettersFreqs, "Frequencies:");
#endif

    //Build Huffman encode tree
    const Tree<Letter> *encodeTree = BuildEncodeTree(lettersFreqs, ALPHABET_SIZE);

#ifdef DEBUG
    cout << "Tree:" << endl;
    printTree(encodeTree->getRoot(), 0);
#endif

    //Count length of code for each letter
    //Array's indices correspond to ASCII code of each letter
    Vector<MY_BYTE> codeLengths(ALPHABET_SIZE, 0);
    CountLengthsOfCodes(encodeTree->getRoot(), codeLengths, 0);

#ifdef DEBUG
    printForLetters(codeLengths, "Code lengths: ");
#endif

    // Max length of code among all the letters
    MY_BYTE maxLength = codeLengths[0];
    for (size_t i = 1; i < codeLengths.getSize(); ++i) {
        if (maxLength < codeLengths[i]) {
            maxLength = codeLengths[i];
        }
    }

#ifdef DEBUG
    cout << "Max-code length: " << size_t(maxLength) << endl;
#endif

    //Count number of codes for each length
    //Array's indices correspond to length of code
    Vector<MY_BYTE> numberOfCodes(maxLength + 1, 0);
    for (size_t i = 0; i < codeLengths.getSize(); ++i) {
        ++numberOfCodes[codeLengths[i]];
    }
    //Ignore zero-length codes
    numberOfCodes[0] = static_cast<MY_BYTE>(0);

    //Distribute letters by code length
    //First indices correspond to length of code
    Vector<Vector<MY_BYTE>> lettersByLength(maxLength + 1);
    for (size_t i = 0; i < lettersFreqs.getSize(); ++i) {
        if (lettersFreqs[i]) {
            lettersByLength[codeLengths[i]].pushBack(static_cast<MY_BYTE>(i));
        }
    }
    // Compute first code for each length
    //First indices correspond to length of code
    Vector<MY_BYTE> firstCodes(maxLength + 1, 0);
    ComputeFirstCodesForLengths(numberOfCodes, firstCodes);

#ifdef DEBUG
    printFirstCode(firstCodes);
#endif

    //Build table with the code for each letter
    Vector<Vector<bool>> table(ALPHABET_SIZE);
    BuildTable(numberOfCodes,
               firstCodes,
               lettersByLength,
               table);

#ifdef DEBUG
    printCodes(table);
#endif

    //Write all the info into archive
    WriteFilesSizes(archive, fileSizes, allTheFilesForCompressingNames);
    WriteEncodingInfo(numberOfCodes, lettersByLength, archive);
    for(size_t i = 0; i < fileStreams.getSize(); i++) {
        WriteEncoding(fileStreams[i], archive, table);
        fileStreams[i].close();
    }

    archive.close();

    delete encodeTree;

    return 0;
}

MY_SIZE_T UpdateLettersFreqs(ifstream           &file,
                             Vector<MY_SIZE_T>  &lettersFreqs) {
    assert(file.is_open());

    file.seekg(0, ifstream::beg); //Reset get() pointer to beginning
    file.clear();                 //Clear EOF flag

    //Size of the file in bytes
    MY_SIZE_T byteSizeFile = 0;

    //Read the whole file and update the table's frequencies
    while (true) {
        auto c = static_cast<MY_BYTE>(file.get());

        if (file.eof()) break;

        lettersFreqs[c]++;
        byteSizeFile++;
    }

    file.seekg(0, ifstream::beg); //Reset get() pointer to beginning
    file.clear();                 //Clear EOF flag

    return byteSizeFile;
}


const Tree<Letter> *BuildEncodeTree(const Vector<MY_SIZE_T>   &lettersFreqs,
                                          size_t              alphSize) {

    assert(alphSize != 0);

    // The increasing-ordered priority queue built on 3-ary heap
    Queue<Tree<Letter>> Q(alphSize, 3);
    // Put all the letters whose frequencies aren't equal to zero into the priority queue
    for (size_t i = 0; i < lettersFreqs.getSize(); i++) {
        if (lettersFreqs[i] != 0) {
            auto *tr = new Tree<Letter>(Letter(lettersFreqs[i], static_cast<MY_BYTE>(i)));
            Q.enqueue(tr);
        }
    }
    // There is nothing to encode
    if (Q.isEmpty()) {
        return nullptr;
    }

    while(Q.getSize() > 1) {

        //Get 2 front elements from the increasing-ordered priority queue
        //Combine them into one
        //And put back into the queue

        Tree<Letter> *Left  = Q.dequeue();
        Tree<Letter> *Right = Q.dequeue();

        Letter leftLet  = Left->getRootData();
        Letter rightLet = Right->getRootData();

        Tree<Letter> *parent = new Tree<Letter>(Letter(leftLet.frequency + rightLet.frequency));
        parent->setLeft(Left->getRoot());
        parent->setRight(Right->getRoot());

        Q.enqueue(parent);
    }


    //Now the priority queue consists only one element which is the root of the resulting tree
    return Q.dequeue();
}
void CountLengthsOfCodes(const Tree<Letter>::Node *root,
                               Vector<MY_BYTE>    &codeLengths,
                               MY_BYTE            level) {
    assert(root);

    if (!root->leftChild && !root->rightChild) {
        codeLengths[root->data.letter] = max<MY_BYTE >(level, 1);
    }

    if (root->leftChild)
        CountLengthsOfCodes(root->leftChild,  codeLengths, level + static_cast<MY_BYTE >(1));

    if (root->rightChild)
        CountLengthsOfCodes(root->rightChild, codeLengths, level + static_cast<MY_BYTE >(1));

}

void ComputeFirstCodesForLengths(const Vector<MY_BYTE> &numberOfCodes,
                                       Vector<MY_BYTE> &firstCode) {

    // Compute first code for each length
    firstCode[numberOfCodes.getSize() - 1] = 0;
    for (size_t i = numberOfCodes.getSize() - 2; i != 0; --i) {
        firstCode[i] = (firstCode[i + 1] + numberOfCodes[i + 1]) >> 1;
    }
}

void BuildTable(const Vector<MY_BYTE>          &numberOfCodes,
                const Vector<MY_BYTE>          &firstCode,
                const Vector<Vector<MY_BYTE >> &lettersByLength,
                      Vector<Vector<bool>>     &table) {
    //Build code from first code
    for (size_t i = 1; i < numberOfCodes.getSize(); ++i) {
        if (numberOfCodes[i]) {
            MY_BYTE counter = firstCode[i];
            for (size_t j = 0; j < numberOfCodes[i]; ++j) {
                //Get first i bits in reversed order
                for (MY_BYTE bit = 0; bit < i; ++bit) {
                    table[lettersByLength[i][j]].pushBack(counter & (static_cast<MY_BYTE >(1) << bit));
                }
                ++counter;
            }
        }
    }
}


void WriteFileNames(      std::ofstream  &out,
                    const Vector<string> &names) {
    assert(out.is_open());

    for (size_t i = 0; i < names.getSize(); i++) {
        out << names[i] << "|";
    }
}

// Works only for the case where size is less then ULONG_MAX
int WriteBinary(std::ofstream &out,
                MY_SIZE_T     size) {
    assert(out.is_open());

    if (size > ULONG_MAX) {
        cerr << "Error in func <WriteBinary>: var <size> max-value is "
             << ULONG_MAX << endl;
        return 1;
    }

    for(int k = 3; k != -1; --k) {
        out.put(static_cast<char>((size >> (BIT_SIZE * k)) % BYTE_STEP));
    }
    return 0;
}

void WriteFilesSizes(      std::ofstream     &out,
                     const Vector<MY_SIZE_T> &sizes,
                     const Vector<string>    &names) {
    assert(out.is_open());

    if (WriteBinary(out, sizes.getSize())) {
        cerr << "Too many files" << endl;
        exit(1);
    }

    for(size_t i = 0; i < sizes.getSize(); i++) {
        if (WriteBinary(out, sizes[i])) {
            cerr << "The file "
                 << names[i]
                 << " is too big" << endl;
            exit(1);
        }
    }
}



void WriteEncodingInfo(const Vector<MY_BYTE>          &numberOfCodes,
                       const Vector<Vector<MY_BYTE >> &lettersByLength,
                             std::ofstream            &out) {

    assert(out.is_open());

    out.put(static_cast<char>(numberOfCodes.getSize() - 1));
    for (size_t i = 1; i < numberOfCodes.getSize(); ++i) {
        out.put(numberOfCodes[i]);
    }

    for (size_t i = 1; i < lettersByLength.getSize(); ++i) {
        for (size_t p = 0; p < numberOfCodes[i]; ++p)
            out.put(lettersByLength[i][p]);
    }
}

void WriteEncoding(      std::ifstream        &in,
                         std::ofstream        &out,
                   const Vector<Vector<bool>> &table) {

    assert(in.is_open());
    assert(out.is_open());

    in.seekg(0, ifstream::beg); //Reset get() pointer to beginning
    in.clear();                 //Clear EOF flag

    //Buffer size of byte to output the encoding sequence byte by byte
    char buf   = 0;
    //How many bits in the buffer are currently complete
    char count = 0;

    //For the whole file
    while (true) {

        auto c = static_cast<MY_BYTE>(in.get());

        if(in.eof()) break;

        for(int i = table[c].getSize() - 1; i >= 0; --i) {
            buf |=  (table[c][i]) << (BIT_SIZE - 1 - count);
            ++count;

            if (count == BIT_SIZE)  {
                out.put(buf);
                buf = 0;
                count = 0;
            }
        }
    }

    //If there's last incomplete buffer
    if (count) {
        out.put(buf);
    }

    in.seekg(0, ifstream::beg); //Reset get() pointer to beginning
    in.clear();                 //Clear EOF flag
}


void readBinary(std::ifstream &in,
           MY_SIZE_T &num) {
    num = 0;
    MY_BYTE c = 0;
    for(int k = 3; k != -1; --k)
    {
        c = in.get();
        num += c * (1 << (8 * k));
    }
}

