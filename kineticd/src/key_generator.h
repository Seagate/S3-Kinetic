#ifndef KINETIC_KEY_GENERATOR_H_
#define KINETIC_KEY_GENERATOR_H_

#include <string>

namespace com {
namespace seagate {
namespace kinetic {

class KeyGenerator {
 public:
    explicit KeyGenerator(size_t key_size);
    ~KeyGenerator();
    std::string GetNextKey();

 private:
    void IncrementByte(unsigned char* byte);
    void PrintKey(unsigned char* key, size_t size);

    int key_size_;
    unsigned char* current_key_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_KEY_GENERATOR_H_
