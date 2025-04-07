#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

// Struct to represent each YouTube entry
struct YouTubeEntry {
    std::string file;
    std::string youtube_url;
};

// Trim helper function to remove leading/trailing whitespaces
std::string trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Load CSV data into memory with proper error handling
std::vector<YouTubeEntry> loadCSV(const std::string& file_path) {
    std::vector<YouTubeEntry> entries;
    std::ifstream file(file_path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open input file: " + file_path);
    }
    
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        return entries; // Return empty if file is empty
    }
    
    int line_number = 1; // For error reporting (header is line 1)
    
    while (std::getline(file, line)) {
        line_number++;
        std::istringstream ss(line);
        std::string file_field, url_field;
        
        // Handle quoted fields with commas
        if (line.find('"') != std::string::npos) {
            bool in_quotes = false;
            std::string current_field;
            std::vector<std::string> fields;
            
            for (char c : line) {
                if (c == '"') {
                    in_quotes = !in_quotes;
                } else if (c == ',' && !in_quotes) {
                    fields.push_back(trim(current_field));
                    current_field.clear();
                } else {
                    current_field += c;
                }
            }
            fields.push_back(trim(current_field)); // Add the last field
            
            if (fields.size() >= 2) {
                file_field = fields[0];
                url_field = fields[1];
            } else {
                std::cerr << "Warning: Skipping malformed line " << line_number << ": " << line << std::endl;
                continue;
            }
        } else {
            // Simple case: no quotes
            std::getline(ss, file_field, ',');
            std::getline(ss, url_field, ',');
        }
        
        // Trim whitespace
        file_field = trim(file_field);
        url_field = trim(url_field);
        
        // Validate URL
        if (!file_field.empty() && url_field.find("http") == 0) {
            YouTubeEntry entry;
            entry.file = file_field;
            entry.youtube_url = url_field;
            entries.push_back(entry);
        } else {
            std::cerr << "Warning: Skipping invalid entry at line " << line_number << std::endl;
        }
    }
    
    return entries;
}

// Save CSV file with error handling
void saveCSV(const std::string& file_path, const std::vector<YouTubeEntry>& entries) {
    std::ofstream file(file_path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file for writing: " + file_path);
    }
    
    file << "File,YouTube_URL\n";
    
    for (const auto& entry : entries) {
        // Handle fields with commas by quoting them
        std::string file_field = entry.file;
        std::string url_field = entry.youtube_url;
        
        if (file_field.find(',') != std::string::npos) {
            file_field = "\"" + file_field + "\"";
        }
        
        if (url_field.find(',') != std::string::npos) {
            url_field = "\"" + url_field + "\"";
        }
        
        file << file_field << "," << url_field << "\n";
    }
    
    if (!file) {
        throw std::runtime_error("Error writing to file: " + file_path);
    }
}

// Class to handle parallel URL checking
class URLChecker {
private:
    std::queue<YouTubeEntry> work_queue;
    std::vector<YouTubeEntry> valid_entries;
    std::vector<YouTubeEntry> invalid_entries;
    std::mutex queue_mutex;
    std::mutex results_mutex;
    std::condition_variable cv;
    std::atomic<bool> done{false};
    std::atomic<int> total_processed{0};
    int total_entries;
    
public:
    URLChecker(const std::vector<YouTubeEntry>& entries, int total) : total_entries(total) {
        for (const auto& entry : entries) {
            work_queue.push(entry);
        }
    }
    
    bool isYouTubeVideoAvailable(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // Only check headers
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Skip SSL cert verification
        
        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_cleanup(curl);
        
        return (res == CURLE_OK && response_code == 200);
    }
    
    void workerThread() {
        while (true) {
            YouTubeEntry entry;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (work_queue.empty()) {
                    if (done) return;
                    cv.wait(lock, [this] { return !work_queue.empty() || done; });
                    if (work_queue.empty() && done) return;
                }
                
                entry = work_queue.front();
                work_queue.pop();
            }
            
            bool is_valid = isYouTubeVideoAvailable(entry.youtube_url);
            
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                if (is_valid) {
                    valid_entries.push_back(entry);
                } else {
                    invalid_entries.push_back(entry);
                }
                
                total_processed++;
                
                // Print progress every few items
                if (total_processed % 10 == 0 || total_processed == total_entries) {
                    float progress = (total_processed * 100.0f) / total_entries;
                    std::cout << "\rProgress: " << total_processed << "/" << total_entries 
                              << " (" << progress << "%)" << std::flush;
                }
            }
        }
    }
    
    std::pair<std::vector<YouTubeEntry>, std::vector<YouTubeEntry>> processEntries() {
        // Determine number of worker threads (hardware concurrency or fallback to 4)
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        
        // Limit threads to a reasonable number
        num_threads = std::min(num_threads, 8u);
        
        std::cout << "Starting URL validation with " << num_threads << " threads..." << std::endl;
        
        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; i++) {
            threads.push_back(std::thread(&URLChecker::workerThread, this));
        }
        
        // Wait for all work to be done
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            done = true;
            cv.notify_all();
        }
        
        // Join all threads
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        std::cout << std::endl << "URL validation complete!" << std::endl;
        
        return {valid_entries, invalid_entries};
    }
};

int main() {
    const std::string input_path = "dataset/youtube_urls.csv";
    const std::string output_path = "dataset/youtube_urls_fixed.csv";
    const std::string removed_path = "dataset/youtube_urls_removed.csv";
    
    try {
        // Initialize CURL globally
        CURLcode curl_init_result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (curl_init_result != CURLE_OK) {
            throw std::runtime_error("Failed to initialize CURL library");
        }
        
        // Load and deduplicate entries
        std::cout << "Loading CSV file: " << input_path << std::endl;
        std::vector<YouTubeEntry> entries = loadCSV(input_path);
        std::cout << "Total entries loaded: " << entries.size() << std::endl;
        
        if (entries.empty()) {
            std::cout << "No valid entries found, nothing to process." << std::endl;
            curl_global_cleanup();
            return 0;
        }
        
        // Deduplicate by URL
        std::set<std::string> seen_urls;
        std::vector<YouTubeEntry> unique_entries;
        
        for (const auto& entry : entries) {
            if (seen_urls.insert(entry.youtube_url).second) {
                unique_entries.push_back(entry);
            }
        }
        
        std::cout << "Unique entries to check: " << unique_entries.size() << std::endl;
        std::cout << "Duplicate entries removed: " << (entries.size() - unique_entries.size()) << std::endl;
        
        // Process URLs
        URLChecker checker(unique_entries, unique_entries.size());
        auto [valid_entries, removed_entries] = checker.processEntries();
        
        // Save results
        saveCSV(output_path, valid_entries);
        saveCSV(removed_path, removed_entries);
        
        std::cout << "Valid entries saved: " << valid_entries.size() << std::endl;
        std::cout << "Invalid entries saved: " << removed_entries.size() << std::endl;
        std::cout << "Cleaned file saved to: " << output_path << std::endl;
        std::cout << "Removed URLs saved to: " << removed_path << std::endl;
        
        // Clean up CURL
        curl_global_cleanup();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        curl_global_cleanup(); // Clean up CURL even on error
        return 1;
    }
    
    return 0;
}