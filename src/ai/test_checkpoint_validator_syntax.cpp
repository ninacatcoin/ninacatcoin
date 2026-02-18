// Quick syntax validation for Checkpoint Validator
// This file tests that all headers can be included without errors

#include <iostream>
#include <memory>

// Mock the dependencies that might not be available
namespace epee {
    // Mock logging
    class log_space {
    public:
        static void set_console_loglevel(int level) {}
    };
}

// Include the header files to verify syntax
#include "ai_checkpoint_validator.hpp"

int main()
{
    std::cout << "✓ Checkpoint Validator header syntax is valid" << std::endl;
    std::cout << "✓ All includes parsed successfully" << std::endl;
    std::cout << "✓ No compilation errors detected" << std::endl;
    return 0;
}
