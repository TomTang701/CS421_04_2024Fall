#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdlib>  // for rand() and srand()
#include <ctime>    // for time()

// Circular buffer class
class CircularBuffer {
public:
    CircularBuffer(size_t size) : buffer(size), head(0), tail(0), count(0) {}

    // Put data into the circular buffer
    void put(char byte) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [this] { return count != buffer.size(); });

        buffer[head] = byte;
        head = (head + 1) % buffer.size();
        count++;
        not_empty.notify_one();
    }

    // Get data from the circular buffer
    char get() {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this] { return count != 0; });

        char byte = buffer[tail];
        tail = (tail + 1) % buffer.size();
        count--;
        not_full.notify_one();
        return byte;
    }

    // Getter for the count
    size_t getCount() const {
        std::unique_lock<std::mutex> lock(mtx);
        return count;
    }

    // Method to notify all waiting consumers that production is done
    void notifyAllConsumers() {
        not_empty.notify_all();
    }

private:
    std::vector<char> buffer;
    size_t head, tail, count;
    mutable std::mutex mtx;
    std::condition_variable not_empty, not_full;
};

// Global variables for input and output files and maximum number of bytes per operation
std::ifstream inputFile;
std::ofstream outputFile;
CircularBuffer* cb;
bool is_production_done = false;  // Flag to indicate producer is done

void producer() {
    const int chunk_size = 5;  // Read 5 characters per batch
    while (inputFile) {
        std::vector<char> temp(chunk_size);

        inputFile.read(temp.data(), chunk_size);
        std::streamsize bytes_read = inputFile.gcount();

        if (bytes_read == 0) {
            break;
        }

        // Debugging: Print out what is being read from input file
        std::cout << "Producer read " << bytes_read << " bytes from input file\n";

        for (int i = 0; i < bytes_read; i++) {
            cb->put(temp[i]);
        }
    }

    // Signal that the producer is done
    is_production_done = true;
    cb->notifyAllConsumers();  // Wake up any waiting consumer
}

void consumer() {
    while (true) {
        const int chunk_size = 5;
        std::vector<char> temp(chunk_size);

        int valid_bytes = 0;

        for (int i = 0; i < chunk_size; i++) {
            // Check if producer is done and buffer is empty
            if (is_production_done && cb->getCount() == 0) {
                break;  // Exit if no more data
            }
            if (cb->getCount() > 0) {
                temp[i] = cb->get();
                valid_bytes++;  // Only count valid bytes read
            } else {
                break;  // If buffer is empty, break out of the loop
            }
        }

        // Write the valid bytes, even if they are less than the chunk size
        if (valid_bytes > 0) {
            std::cout << "Consumer writing " << valid_bytes << " bytes to output file\n";
            outputFile.write(temp.data(), valid_bytes);
            outputFile.flush();  // Ensure data is flushed to the output file
        }

        // Exit the loop if the producer is done and buffer is empty
        if (is_production_done && cb->getCount() == 0) {
            break;
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file> <n> <m>\n";
        std::cerr << "Example: " << argv[0] << " input.txt output.txt 100 1024\n";
        return 1;
    }

    std::string inputFileName = argv[1];
    std::string outputFileName = argv[2];
    int buffer_size = std::stoi(argv[4]);  // Circular buffer size

    // Open input and output files
    inputFile.open(inputFileName, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open input file: " << inputFileName << "\n";
        return 1;
    }

    outputFile.open(outputFileName, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open output file: " << outputFileName << "\n";
        inputFile.close();  // Close the input file if the output file cannot be opened.
        return 1;
    }

    // Initialize circular buffer
    CircularBuffer buffer(buffer_size);
    cb = &buffer;

    // Create producer and consumer threads
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    // Wait for both threads to complete
    producer_thread.join();
    consumer_thread.join();

    // Close files
    inputFile.close();
    outputFile.close();

    return 0;
}

