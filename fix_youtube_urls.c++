#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

// Struct to represent each YouTube entry
struct YouTubeEntry {
    std::string file;
    std::string youtube_url;
};

// Trim helper to remove leading/trailing whitespaces
std::string trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\r\n");
    const auto end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Load CSV data into memory
std::vector<YouTubeEntry> loadCSV(const std::string& file_path) {
    std::vector<YouTubeEntry> entries;
    std::ifstream file(file_path);
    std::string line;

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        YouTubeEntry entry;
        std::getline(ss, entry.file, ',');
        std::getline(ss, entry.youtube_url, ',');

        // Trim and validate URL
        entry.file = trim(entry.file);
        entry.youtube_url = trim(entry.youtube_url);

        if (!entry.file.empty() && entry.youtube_url.find("http") == 0) {
            entries.push_back(entry);
        }
    }

    return entries;
}

// Save cleaned CSV file
void saveCSV(const std::string& file_path, const std::vector<YouTubeEntry>& entries) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to write to file: " + file_path);
    }

    file << "File,YouTube_URL\n";
    for (const auto& entry : entries) {
        file << entry.file << "," << entry.youtube_url << "\n";
    }
}

int main() {
    const std::string input_file = "dataset/youtube_urls.csv";
    const std::string output_file = "dataset/youtube_urls_fixed.csv";

    try {
        auto entries = loadCSV(input_file);
        std::cout << "Total entries loaded: " << entries.size() << "\n";

        std::set<std::string> seen_urls;
        std::vector<YouTubeEntry> unique_entries;

        for (const auto& entry : entries) {
            if (seen_urls.insert(entry.youtube_url).second) {
                unique_entries.push_back(entry);
            }
        }

        std::cout << "Unique valid entries: " << unique_entries.size() << "\n";
        saveCSV(output_file, unique_entries);
        std::cout << "Cleaned file saved to: " << output_file << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
