//
//  CPDN Controller Progress File Handler class header.
//
//     Glenn Carver, CPDN, 2026

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "../src/cpdn_control.h"    // for TaskState struct

namespace fs = std::filesystem;

constexpr std::string_view progressfile_name = "cpdn_progressfile.txt";

class ProgressFileHandler {

  public:
    // C++ note. explicit forbids implicit conversions and copy-initialization.
    explicit ProgressFileHandler( const std::string& slot_path );

    ~ProgressFileHandler() = default;

    // Delete copy constructor and assignment operator
    // as these are not appropriate for this class.

    ProgressFileHandler( const ProgressFileHandler& ) = delete;
    ProgressFileHandler& operator=( const ProgressFileHandler& ) = delete;

    // Delete move constructor and assignment operator

    ProgressFileHandler( ProgressFileHandler&& ) = delete;
    ProgressFileHandler& operator=( ProgressFileHandler&& ) = delete;

    // Methods for handling progress file operations.

    // Store task progress from TaskState struct to progress file
    bool write( const TaskState& task, std::string& err_msg ) const;

    // Reads task progress from progress file into TaskState struct
    void read( TaskState& task ) const;

    // Print the progress file to the provided output stream
    void print( std::ostream& os ) const;

    // Getter/setter functions
    std::string name() const { return std::string( progressfile_name ); }
    std::string path() const { return progressfile_path.string(); }
    bool exists() const { return fs::exists( progressfile_path ); }
    bool remove() const { return fs::remove( progressfile_path ); }
    bool is_empty() const { return fs::is_empty( progressfile_path ); }

  private:
    // Private member variables
    fs::path progressfile_path;
};
