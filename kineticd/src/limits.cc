#include "limits.h"


namespace com {
namespace seagate {
namespace kinetic {

Limits::Limits(size_t max_key_size,
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
        size_t max_deletes_per_batch)
        :   max_key_size_(max_key_size),
            max_tag_size_(max_tag_size),
            max_version_size_(max_version_size),
            max_message_size_(max_message_size),
            max_value_size_(max_value_size),
            max_connections_(max_connections),
            max_outstanding_read_requests_(max_outstanding_read_requests),
            max_outstanding_write_requests_(max_outstanding_write_requests),
            max_key_range_count_(max_key_range_count),
            max_identity_count_(max_identity_count),
            max_pin_size_(max_pin_size),
            max_batch_size_(max_batch_size),
            max_deletes_per_batch_(max_deletes_per_batch) { }

size_t Limits::max_key_size() const {
    return max_key_size_;
}

size_t Limits::max_tag_size() const {
    return max_tag_size_;
}

size_t Limits::max_version_size() const {
    return max_version_size_;
}

size_t Limits::max_message_size() const {
    return max_message_size_;
}

size_t Limits::max_value_size() const {
    return max_value_size_;
}

size_t Limits::max_connections() const {
    return max_connections_;
}

size_t Limits::max_outstanding_read_requests() const {
    return max_outstanding_read_requests_;
}

size_t Limits::max_outstanding_write_requests() const {
    return max_outstanding_write_requests_;
}

size_t Limits::max_key_range_count() const {
    return max_key_range_count_;
}

size_t Limits::max_identity_count() const {
    return max_identity_count_;
}

size_t Limits::max_pin_size() const {
    return max_pin_size_;
}

size_t Limits::max_batch_size() const {
    return max_batch_size_;
}

size_t Limits::max_deletes_per_batch() const {
    return max_deletes_per_batch_;
}


} // namespace kinetic
} // namespace seagate
} // namespace com
