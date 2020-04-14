#ifndef KINETIC_LIMITS_H_
#define KINETIC_LIMITS_H_

#include <stddef.h>
#include <stdint.h>

namespace com {
namespace seagate {
namespace kinetic {

class Limits {
    public:
    Limits(size_t max_key_size,
        size_t max_tag_size,
        size_t max_version_size,
        size_t max_message_size,
        size_t max_value_size,
        size_t max_connections,
        size_t max_outstanding_read_requests,
        size_t max_outstanding_write_requests,
        size_t max_key_range_count,
        size_t max_identity_count,
        size_t max_pin_size,
        size_t max_batch_size,
        size_t max_deletes_per_batch);

    size_t max_key_size() const;

    size_t max_tag_size() const;

    size_t max_version_size() const;
    size_t max_message_size() const;
    size_t max_value_size() const;
    size_t max_connections() const;
    size_t max_outstanding_read_requests() const;
    size_t max_outstanding_write_requests() const;
    size_t max_key_range_count() const;
    size_t max_identity_count() const;
    size_t max_pin_size() const;
    size_t max_batch_size() const;
    size_t max_deletes_per_batch() const;



    private:
    const size_t max_key_size_;
    const size_t max_tag_size_;
    const size_t max_version_size_;
    const size_t max_message_size_;
    const size_t max_value_size_;
    const size_t max_connections_;
    const size_t max_outstanding_read_requests_;
    const size_t max_outstanding_write_requests_;
    const size_t max_key_range_count_;
    const size_t max_identity_count_;
    const size_t max_pin_size_;
    const size_t max_batch_size_;
    const size_t max_deletes_per_batch_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_LIMITS_H_
