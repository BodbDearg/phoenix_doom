#include <Foundation/Foundation.h>

extern "C" void ThreeDOMain();

int main(int argc, char* argv[]) noexcept {
    @autoreleasepool {
        ThreeDOMain();
    }
    
    return 0;
}
