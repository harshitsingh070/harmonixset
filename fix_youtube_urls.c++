#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>

struct YouTubeEntry {
    std::string file;
    std::string youtube_url;
};

std::vector<YouTubeEntry> loadCSV(const std::string& file_path) {
    std::vector<YouTubeEntry> entries;
    std::ifstream file(file_path);
    std::string line;

    // Skip the header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        YouTubeEntry entry;
        std::getline(ss, entry.file, ',');
        std::getline(ss, entry.youtube_url, ',');

        // Ensure URL starts with "http"
        if (entry.youtube_url.find("http") == 0) {
            entries.push_back(entry);
        }
    }
    return entries;
}

void saveCSV(const std::string& file_path, const std::vector<YouTubeEntry>& entries) {
    std::ofstream file(file_path);
    file << "File,YouTube_URL\n"; // Write header

    for (const auto& entry : entries) {
        file << entry.file << "," << entry.youtube_url << "\n";
    }
}

int main() {
    const std::string file_path = "dataset/youtube_urls.csv";
    const std::string fixed_file_path = "dataset/youtube_urls_fixed.csv";

    try {
        std::vector<YouTubeEntry> entries = loadCSV(file_path);

        // Remove duplicates using a set
        std::set<std::string> unique_urls;
        std::vector<YouTubeEntry> unique_entries;

        for (const auto& entry : entries) {
            if (unique_urls.insert(entry.youtube_url).second) {
                unique_entries.push_back(entry);
            }
        }

        // Save the cleaned CSV file
        saveCSV(fixed_file_path, unique_entries);
        std::cout << "Fixed file saved as: " << fixed_file_path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error processing the file: " << e.what() << std::endl;
    }

    return 0;
}